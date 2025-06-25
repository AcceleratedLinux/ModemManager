/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details:
 *
 * Copyright (C) 2021 Anubhav Gupta <anubhav.gupta@digi.com>
 * Copyright (C) 2021 UnitacSW <UnitacSW@unitac.cn>
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "ModemManager.h"
#include "mm-log-object.h"
#include "mm-iface-modem.h"
#include "mm-iface-modem-3gpp.h"
#include "mm-base-modem-at.h"
#include "mm-broadband-bearer.h"
#include "mm-broadband-modem-unitac.h"
#include "mm-broadband-bearer-unitac.h"
#include "mm-sim-unitac.h"
#include "mm-iface-modem-signal.h"
#include "mm-modem-helpers.h"
#include "mm-modem-helpers-unitac.h"
#include "mm-sim-unitac.h"

static void iface_modem_init (MMIfaceModemInterface *iface);
static void iface_modem_signal_init (MMIfaceModemSignalInterface *iface);

static MMIfaceModemSignalInterface *iface_signal_parent;

G_DEFINE_TYPE_EXTENDED (MMBroadbandModemUnitac, mm_broadband_modem_unitac, MM_TYPE_BROADBAND_MODEM, 0,
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM, iface_modem_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_SIGNAL, iface_modem_signal_init));

typedef struct {
    MMSignal *lte;
} DetailedSignal;


struct _MMBroadbandModemUnitacPrivate {
    DetailedSignal detailed_signal;
};

/*****************************************************************************/
/* Create Bearer (Modem interface) */

typedef enum {
    CREATE_BEARER_STEP_FIRST,
    CREATE_BEARER_STEP_CHECK_PROFILE,
    CREATE_BEARER_STEP_CREATE_BEARER,
    CREATE_BEARER_STEP_LAST,
} CreateBearerStep;

typedef struct {
    CreateBearerStep step;
    MMBearerProperties *properties;
    MMBaseBearer *bearer;
    gboolean has_net;
} CreateBearerContext;

static void
create_bearer_context_free (CreateBearerContext *ctx)
{
    g_clear_object (&ctx->bearer);
    g_object_unref (ctx->properties);
    g_slice_free (CreateBearerContext, ctx);
}

static MMBaseBearer *
modem_create_bearer_finish (MMIfaceModem *self,
                            GAsyncResult *res,
                            GError **error)
{
    return MM_BASE_BEARER (g_task_propagate_pointer (G_TASK (res), error));
}

static void create_bearer_step (GTask *task);

static void
broadband_bearer_unitac_new_ready (GObject *source,
                                   GAsyncResult *res,
                                   GTask *task)
{
    MMBroadbandModemUnitac *self;
    CreateBearerContext    *ctx;
    GError                 *error = NULL;

    self = g_task_get_source_object (task);
    ctx  = g_task_get_task_data (task);

    g_assert (!ctx->bearer);
    ctx->bearer = mm_broadband_bearer_unitac_new_finish (res, &error);
    if (!ctx->bearer) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    mm_obj_dbg (self, "new unitac broadband bearer created at DBus path '%s'", mm_base_bearer_get_path (ctx->bearer));
    ctx->step++;
    create_bearer_step (task);
}

/*****************************************************************************/
/* Load extended signal information (Signal interface) */
static void
detailed_signal_clear (DetailedSignal *signal)
{
    g_clear_object (&signal->lte);
}

static void
detailed_signal_free (DetailedSignal *signal)
{
    detailed_signal_clear (signal);
    g_slice_free (DetailedSignal, signal);
}

static gboolean
signal_load_values_finish (MMIfaceModemSignal *self,
                           GAsyncResult *res,
                           MMSignal **cdma,
                           MMSignal **evdo,
                           MMSignal **gsm,
                           MMSignal **umts,
                           MMSignal **lte,
                           MMSignal **nr5g,
                           GError **error)
{
    DetailedSignal *signals;

    signals = g_task_propagate_pointer (G_TASK (res), error);
    if (!signals)
        return FALSE;

    *lte  = signals->lte ? g_object_ref (signals->lte) : NULL;

    detailed_signal_free (signals);
    return TRUE;
}

static gboolean
get_gutatus_field(MMBroadbandModemUnitac *self, const gchar *response, const gchar *keyword, gdouble *value)
{
    GRegex *r;
    GMatchInfo *match_info = NULL;
    GError *match_error = NULL;
    gchar regex[256];
    gboolean ret = FALSE;

    sprintf (regex, "%s:(-?\\d+)", keyword);

    r = g_regex_new (regex, 0, 0, NULL);
    g_assert (r != NULL);

    g_regex_match_full (r, response, strlen (response), 0, 0, &match_info, &match_error);
    if (match_error) {
        mm_obj_dbg(self, "%s: USTATUS match error %s", __func__, match_error->message);
        g_error_free(match_error);
        goto done;
    }

    if (!g_match_info_matches (match_info)) {
        mm_obj_dbg (self, "USTATUS %s Couldn't get value", __func__);
        goto done;
    }

    if (!mm_get_double_from_match_info(match_info, 1, value)) {
        mm_obj_dbg(self, "Could not get USTATUS keyword (%s)", keyword);
        goto done;
    }

    mm_obj_dbg (self, "Got USTATUS keyword [%s] = [%f]", keyword, *value);
    ret = TRUE;

    done:
    if (match_info)
        g_match_info_free (match_info);
    g_regex_unref (r);
    return ret;
}

static void update_lte_signal(const gchar *response, MMBroadbandModemUnitac *self)
{
    gdouble value;

    self->priv->detailed_signal.lte = mm_signal_new ();
    if (get_gutatus_field(self, response, "RSSI",  &value)) {
        mm_signal_set_rssi (self->priv->detailed_signal.lte, value);
    }
    if (get_gutatus_field(self, response, "RSRP",  &value)) {
        mm_signal_set_rsrp (self->priv->detailed_signal.lte, value);
    }
    if (get_gutatus_field(self, response, "RSRQ",  &value)) {
        mm_signal_set_rsrq (self->priv->detailed_signal.lte, value);
    }
    if (get_gutatus_field(self, response, "SINR",  &value)) {
        mm_signal_set_sinr (self->priv->detailed_signal.lte, value);
    }
}

static void
ustatus_ready (MMBaseModem *_self,
               GAsyncResult *res,
               GTask *task)
{
    MMBroadbandModemUnitac *self = MM_BROADBAND_MODEM_UNITAC (_self);
    DetailedSignal *signals;
    const gchar *response;
    GError *error = NULL;

    mm_obj_dbg (self, "USTATUS ustatus_ready");

    response = mm_base_modem_at_command_finish (_self, res, &error);
    if (error || !response) {
        mm_obj_dbg (self, "!USTATUS failed: %s", error->message);
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }
    /* The modems from Unitac we support only do 4G */
    update_lte_signal(response, self);

    signals = g_slice_new0 (DetailedSignal);
    signals->lte = self->priv->detailed_signal.lte ? g_object_ref (self->priv->detailed_signal.lte) : NULL;

    g_task_return_pointer (task, signals, (GDestroyNotify)detailed_signal_free);
    g_object_unref (task);
}


static void
signal_load_values (MMIfaceModemSignal *_self,
                    GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
    MMBroadbandModemUnitac *self      = MM_BROADBAND_MODEM_UNITAC (_self);
    GTask *task;

    task = g_task_new (self, cancellable, callback, user_data);

    /* Clear any previous detailed signal values to get new ones */
    detailed_signal_clear (&self->priv->detailed_signal);

    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "!USTATUS",
                              20,
                              FALSE,
                              (GAsyncReadyCallback)ustatus_ready,
                              task);
}

/*****************************************************************************/
/* %WMODE? response parser */

static void
wmode_check_ready (MMBaseModem *_self,
                   GAsyncResult *res,
                   GTask *task)
{
    MMBroadbandModemUnitac *self      = MM_BROADBAND_MODEM_UNITAC (_self);
    const gchar            *response;
    GError                 *error     = NULL;
    CreateBearerContext    *ctx;

    ctx = g_task_get_task_data (task);

    response = mm_base_modem_at_command_finish (_self, res, &error);
    if (error || !response) {
        mm_obj_dbg (self, "%WMODE failed: %s", error->message);
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    if (!mm_unitac_parse_wmode_response (response, &error, self, task)) {
        mm_obj_dbg (self, "couldn't load wmode: %s", error->message);
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    ctx->step++;
    create_bearer_step (task);
}

static void
create_bearer_step (GTask *task)
{
    MMBroadbandModemUnitac *self;
    CreateBearerContext    *ctx;

    self = g_task_get_source_object (task);
    ctx  = g_task_get_task_data (task);

    switch (ctx->step) {
        case CREATE_BEARER_STEP_FIRST:
            ctx->step++;
            /* fall through */

        case CREATE_BEARER_STEP_CHECK_PROFILE:
            mm_obj_dbg (self, "checking current WMODE...");
            mm_base_modem_at_command (
                    MM_BASE_MODEM (self),
                    "%WMODE?",
                    5,
                    FALSE,
                    (GAsyncReadyCallback) wmode_check_ready,
                    task);
            return;

        case CREATE_BEARER_STEP_CREATE_BEARER:
            /* we'll create a unitac bearer */
            mm_obj_dbg (self, "creating unitac broadband bearer...");
            mm_broadband_bearer_unitac_new (
                    MM_BROADBAND_MODEM (self),
                    ctx->properties,
                    NULL, /* cancellable */
                    (GAsyncReadyCallback) broadband_bearer_unitac_new_ready,
                    task);
            return;

        case CREATE_BEARER_STEP_LAST:
            g_assert (ctx->bearer);
            g_task_return_pointer (task, g_object_ref (ctx->bearer), g_object_unref);
            g_object_unref (task);
            return;

        default:
            g_assert_not_reached ();
    }

    g_assert_not_reached ();
}

static void
modem_create_bearer (MMIfaceModem *self,
                     MMBearerProperties *properties,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    CreateBearerContext *ctx;
    GTask               *task;

    ctx = g_slice_new0 (CreateBearerContext);
    ctx->step = CREATE_BEARER_STEP_FIRST;
    ctx->properties = g_object_ref (properties);

    /* Flag whether this modem has exposed a network interface */
    ctx->has_net = !!mm_base_modem_peek_best_data_port (MM_BASE_MODEM (self), MM_PORT_TYPE_NET);

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, (GDestroyNotify) create_bearer_context_free);
    create_bearer_step (task);
}

/*****************************************************************************/
/* Load current bands (Modem interface) */

static GArray *
load_current_bands_finish (MMIfaceModem *self,
                           GAsyncResult *res,
                           GError **error)
{
    return (GArray *) g_task_propagate_pointer (G_TASK (res), error);
}

static void
uband_load_current_bands_ready (MMBaseModem *self,
                                GAsyncResult *res,
                                GTask *task)
{
    GError      *error = NULL;
    const gchar *response;
    GArray      *out;

    response = mm_base_modem_at_command_finish (self, res, &error);
    if (!response) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    out = mm_unitac_parse_uband_response (response, self, &error);
    if (!out) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    g_task_return_pointer (task, out, (GDestroyNotify)g_array_unref);
    g_object_unref (task);
}

static void
load_current_bands (MMIfaceModem *_self,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
    MMBroadbandModemUnitac *self = MM_BROADBAND_MODEM_UNITAC(_self);
    GTask                  *task;

    task = g_task_new (self, NULL, callback, user_data);

    mm_obj_dbg (self, "loading current bands...");
    mm_base_modem_at_command (
            MM_BASE_MODEM (self),
            "!UBAND?",
            3,
            FALSE,
            (GAsyncReadyCallback)uband_load_current_bands_ready,
            task);
}

/*****************************************************************************/
/* Load supported bands (Modem interface) */

static GArray *
load_supported_bands_finish (MMBaseModem  *self,
                             GAsyncResult *res,
                             GError **error)
{
    return (GArray *) g_task_propagate_pointer (G_TASK (res), error);
}

static void
load_supported_bands_ready (MMIfaceModem *self,
                           GAsyncResult *res,
                           GTask *task)
{
    GArray *bands = NULL;
    const gchar *response = NULL;
    GError *error = NULL;

    response = mm_base_modem_at_command_finish (self, res, &error);
    if (!response)
        g_task_return_error (task, error);
    else {
        /***************************
         * AT!UBAND=?
         *
         * !UBAND: (3-7-20-31)
         *
         * OK
         */
        bands = mm_unitac_parse_supported_bands_response(response,&error);
        if (!bands){
            g_task_return_error (task, error);
        }
        else{
            g_task_return_pointer (task, bands, (GDestroyNotify)g_array_unref);
        }
    }
    g_object_unref (task);
}

static void
load_supported_bands (MMIfaceModem *self,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
    mm_base_modem_at_command (
        MM_BASE_MODEM (self),
        "!UBAND=?",
        5,
        FALSE,
        (GAsyncReadyCallback)load_supported_bands_ready,
        g_task_new (self, NULL, callback, user_data));
}

/*****************************************************************************/
/* Modem power down (Modem interface) */

static gboolean
modem_power_down_finish (MMIfaceModem *self,
                         GAsyncResult *res,
                         GError **error)
{
    return !!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
}

static void
modem_power_down (MMIfaceModem *self,
                  GAsyncReadyCallback callback,
                  gpointer user_data)
{
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CFUN=4",
                              10,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Load unlock retries (Modem interface) */

static MMUnlockRetries *
load_unlock_retries_finish (MMIfaceModem *self,
                            GAsyncResult *res,
                            GError **error)
{
    return g_task_propagate_pointer (G_TASK (res), error);
}

static void
load_unlock_retries_ready (MMBaseModem *self,
                           GAsyncResult *res,
                           GTask *task)
{
    const gchar *response;
    GError *error = NULL;
    gint pin1, pin2, puk1,  puk2;

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (!response) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /***************************
     * AT+CPINC?
     *
     * +CPINC: 3, 3, 10, 10
     *
     * OK
     */
    response = mm_strip_tag (response, "+CPINC:");
    if (sscanf (response, " %d, %d, %d, %d", &pin1, &pin2, &puk1,  &puk2) == 4) {
        MMUnlockRetries *retries;
        retries = mm_unlock_retries_new ();
        mm_unlock_retries_set (retries, MM_MODEM_LOCK_SIM_PIN, pin1);
        mm_unlock_retries_set (retries, MM_MODEM_LOCK_SIM_PUK, puk1);
        mm_unlock_retries_set (retries, MM_MODEM_LOCK_SIM_PIN2, pin2);
        mm_unlock_retries_set (retries, MM_MODEM_LOCK_SIM_PUK2, puk2);
        g_task_return_pointer (task, retries, g_object_unref);
    } else {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Invalid unlock retries response: '%s'",
                                 response);
    }
    g_object_unref (task);
}

static void
load_unlock_retries (MMIfaceModem *self,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    mm_base_modem_at_command (
        MM_BASE_MODEM (self),
        "+CPINC?",
        3,
        FALSE,
        (GAsyncReadyCallback)load_unlock_retries_ready,
        g_task_new (self, NULL, callback, user_data));
}

/*****************************************************************************/
/* Initializing the modem (during first enabling) */

static gboolean
enabling_modem_init_finish (MMBroadbandModem *self,
                            GAsyncResult *res,
                            GError **error)
{
    return !!mm_base_modem_at_command_full_finish (MM_BASE_MODEM (self), res, error);
}

static void
enabling_modem_init (MMBroadbandModem *self,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    /* ATZ resets the Unitac modem and switches the WMODE, instead run AT */
    mm_base_modem_at_command_full (MM_BASE_MODEM (self),
                                   mm_base_modem_peek_port_primary (MM_BASE_MODEM (self)),
                                   "AT",
                                   6,
                                   FALSE,
                                   FALSE,
                                   NULL, /* cancellable */
                                   callback,
                                   user_data);
}

/*****************************************************************************/
/* Create SIM (Modem interface) */

static MMBaseSim *
modem_create_sim_finish (MMIfaceModem  *self,
                         GAsyncResult  *res,
                         GError        **error)
{
    return mm_sim_unitac_new_finish (res, error);
}

static void
modem_create_sim (MMIfaceModem         *self,
                  GAsyncReadyCallback  callback,
                  gpointer             user_data)
{
    mm_sim_unitac_new (MM_BASE_MODEM (self),
                       NULL, /* cancellable */
                       callback,
                       user_data);
}

/*****************************************************************************/

MMBroadbandModemUnitac *
mm_broadband_modem_unitac_new (const gchar  *device,
                               const gchar  *physdev,
                               const gchar **drivers,
                               const gchar  *plugin,
                               guint16       vendor_id,
                               guint16       product_id)
{
    return g_object_new (MM_TYPE_BROADBAND_MODEM_UNITAC,
                         MM_BASE_MODEM_DEVICE,     device,
                         MM_BASE_MODEM_PHYSDEV,    physdev,
                         MM_BASE_MODEM_DRIVERS,    drivers,
                         MM_BASE_MODEM_PLUGIN,     plugin,
                         MM_BASE_MODEM_VENDOR_ID,  vendor_id,
                         MM_BASE_MODEM_PRODUCT_ID, product_id,
                         MM_BASE_MODEM_DATA_NET_SUPPORTED, TRUE,
                         NULL);
}

static void
mm_broadband_modem_unitac_init (MMBroadbandModemUnitac *self)
{
    /* Initialize private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                              MM_TYPE_BROADBAND_MODEM_UNITAC,
                                              MMBroadbandModemUnitacPrivate);


}

static void
iface_modem_init (MMIfaceModemInterface *iface)
{
    iface->create_sim                  = modem_create_sim;
    iface->create_sim_finish           = modem_create_sim_finish;
    iface->create_bearer               = modem_create_bearer;
    iface->create_bearer_finish        = modem_create_bearer_finish;
    iface->load_supported_bands        = load_supported_bands;
    iface->load_supported_bands_finish = load_supported_bands_finish;
    iface->load_current_bands          = load_current_bands;
    iface->load_current_bands_finish   = load_current_bands_finish;

    iface->modem_power_down           = modem_power_down;
    iface->modem_power_down_finish    = modem_power_down_finish;
    iface->load_unlock_retries        = load_unlock_retries;
    iface->load_unlock_retries_finish = load_unlock_retries_finish;
}

static void
iface_modem_signal_init (MMIfaceModemSignalInterface *iface)
{
    iface_signal_parent = g_type_interface_peek_parent (iface);
    iface->load_values = signal_load_values;
    iface->load_values_finish = signal_load_values_finish;
}

static void
dispose (GObject *object)
{
    MMBroadbandModemUnitac *self = MM_BROADBAND_MODEM_UNITAC (object);

    detailed_signal_clear (&self->priv->detailed_signal);

    G_OBJECT_CLASS (mm_broadband_modem_unitac_parent_class)->dispose (object);
}

static void
mm_broadband_modem_unitac_class_init (MMBroadbandModemUnitacClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    MMBroadbandModemClass *broadband_modem_class = MM_BROADBAND_MODEM_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMBroadbandModemUnitacPrivate));

    broadband_modem_class->enabling_modem_init        = enabling_modem_init;
    broadband_modem_class->enabling_modem_init_finish = enabling_modem_init_finish;

    object_class->dispose = dispose;

}