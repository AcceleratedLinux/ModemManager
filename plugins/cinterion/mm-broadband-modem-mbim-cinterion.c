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
 * Copyright (C) 2021 Aleksander Morgado <aleksander@aleksander.es>
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "ModemManager.h"
#include "mm-log.h"
#include "mm-errors-types.h"
#include "mm-iface-modem.h"
#include "mm-iface-modem-location.h"
#include "mm-iface-modem-voice.h"
#include "mm-broadband-modem-mbim-cinterion.h"
#include "mm-shared-cinterion.h"
#include "mm-base-modem-at.h"

static void iface_modem_init          (MMIfaceModem         *iface);
static void iface_modem_location_init (MMIfaceModemLocation *iface);
static void iface_modem_voice_init    (MMIfaceModemVoice    *iface);
static void iface_modem_time_init     (MMIfaceModemTime     *iface);
static void shared_cinterion_init     (MMSharedCinterion    *iface);

static MMIfaceModem         *iface_modem_parent;
static MMIfaceModemLocation *iface_modem_location_parent;
static MMIfaceModemVoice    *iface_modem_voice_parent;
static MMIfaceModemTime     *iface_modem_time_parent;

G_DEFINE_TYPE_EXTENDED (MMBroadbandModemMbimCinterion, mm_broadband_modem_mbim_cinterion, MM_TYPE_BROADBAND_MODEM_MBIM, 0,
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM, iface_modem_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_LOCATION, iface_modem_location_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_VOICE, iface_modem_voice_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_TIME, iface_modem_time_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_SHARED_CINTERION, shared_cinterion_init))

/*****************************************************************************/

/* Manufacturer loading (Modem interface) */
static gboolean
response_processor_string_ignore_at_errors (MMBaseModem *self,
                                            gpointer none,
                                            const gchar *command,
                                            const gchar *response,
                                            gboolean last_command,
                                            const GError *error,
                                            GVariant **result,
                                            GError **result_error)
{
    if (error) {
        /* Ignore AT errors (ie, ERROR or CMx ERROR) */
        if (error->domain != MM_MOBILE_EQUIPMENT_ERROR || last_command)
            *result_error = g_error_copy (error);

        return FALSE;
    }

    *result = g_variant_new_string (response);
    return TRUE;
}

static gchar *
sanitize_info_reply (GVariant *v, const char *prefix)
{
    const gchar *reply, *p;
    gchar *sanitized;

    /* Strip any leading command reply */
    reply = g_variant_get_string (v, NULL);
    p = strstr (reply, prefix);
    if (p)
        reply = p + strlen (prefix);
    sanitized = g_strdup (reply);
    return mm_strip_quotes (g_strstrip (sanitized));
}

/*****************************************************************************/
/* Model loading (Modem interface) */

static gchar *
modem_load_model_finish (MMIfaceModem *self,
                         GAsyncResult *res,
                         GError **error)
{
    GVariant *result;
    gchar *model = NULL;

    result = mm_base_modem_at_sequence_finish (MM_BASE_MODEM (self), res, NULL, error);
    if (result) {
        model = sanitize_info_reply (result, "GMM:");
        mm_obj_dbg (self, "loaded model: %s", model);
    }
    return model;
}

static const MMBaseModemAtCommand models[] = {
        { "+GMM", 3, TRUE, response_processor_string_ignore_at_errors },
        { "+GMM", 3, TRUE, response_processor_string_ignore_at_errors },
        { NULL }
};

static void
modem_load_model (MMIfaceModem *self,
                  GAsyncReadyCallback callback,
                  gpointer user_data)
{
    mm_obj_dbg (self, "loading model...");
    mm_base_modem_at_sequence (
            MM_BASE_MODEM (self),
            models,
            NULL, /* response_processor_context */
            NULL, /* response_processor_context_free */
            callback,
            user_data);
}

/*****************************************************************************/
/* Revision loading */

static gchar *
modem_load_revision_finish (MMIfaceModem *self,
                            GAsyncResult *res,
                            GError **error)
{
    GVariant *result;
    gchar *revision = NULL;

    result = mm_base_modem_at_sequence_finish (MM_BASE_MODEM (self), res, NULL, error);
    if (result) {
        revision = sanitize_info_reply (result, "VERSION:");
        mm_obj_dbg (self, "loaded revision: %s", revision);
    }
    return revision;
}

static const MMBaseModemAtCommand revisions[] = {
        { "^VERSION?",  3, TRUE, response_processor_string_ignore_at_errors },
        { NULL }
};

static void
modem_load_revision (MMIfaceModem *self,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    mm_obj_dbg (self, "loading revision...");
    mm_base_modem_at_sequence (
            MM_BASE_MODEM (self),
            revisions,
            NULL, /* response_processor_context */
            NULL, /* response_processor_context_free */
            callback,
            user_data);
}

MMBroadbandModemMbimCinterion *
mm_broadband_modem_mbim_cinterion_new (const gchar *device,
                                      const gchar **drivers,
                                      const gchar *plugin,
                                      guint16 vendor_id,
                                      guint16 product_id)
{
    return g_object_new (MM_TYPE_BROADBAND_MODEM_MBIM_CINTERION,
                         MM_BASE_MODEM_DEVICE, device,
                         MM_BASE_MODEM_DRIVERS, drivers,
                         MM_BASE_MODEM_PLUGIN, plugin,
                         MM_BASE_MODEM_VENDOR_ID, vendor_id,
                         MM_BASE_MODEM_PRODUCT_ID, product_id,
                         /* MBIM bearer supports NET only */
                         MM_BASE_MODEM_DATA_NET_SUPPORTED, TRUE,
                         MM_BASE_MODEM_DATA_TTY_SUPPORTED, FALSE,
                         MM_BROADBAND_MODEM_MBIM_QMI_UNSUPPORTED, FALSE,
                         MM_IFACE_MODEM_SIM_HOT_SWAP_SUPPORTED, TRUE,
                         MM_BROADBAND_MODEM_MBIM_INTEL_FIRMWARE_UPDATE_UNSUPPORTED, TRUE,
                         NULL);
}

static void
mm_broadband_modem_mbim_cinterion_init (MMBroadbandModemMbimCinterion *self)
{
}

static void
iface_modem_init (MMIfaceModem *iface)
{
    iface_modem_parent = g_type_interface_peek_parent (iface);

    iface->reset                = mm_shared_cinterion_modem_reset;
    iface->reset_finish         = mm_shared_cinterion_modem_reset_finish;
    iface->load_model           = modem_load_model;
    iface->load_model_finish    = modem_load_model_finish;
    iface->load_revision        = modem_load_revision;
    iface->load_revision_finish = modem_load_revision_finish;
}

static MMIfaceModem *
peek_parent_interface (MMSharedCinterion *self)
{
    return iface_modem_parent;
}

static void
iface_modem_location_init (MMIfaceModemLocation *iface)
{
    iface_modem_location_parent = g_type_interface_peek_parent (iface);

    iface->load_capabilities                 = mm_shared_cinterion_location_load_capabilities;
    iface->load_capabilities_finish          = mm_shared_cinterion_location_load_capabilities_finish;
    iface->enable_location_gathering         = mm_shared_cinterion_enable_location_gathering;
    iface->enable_location_gathering_finish  = mm_shared_cinterion_enable_location_gathering_finish;
    iface->disable_location_gathering        = mm_shared_cinterion_disable_location_gathering;
    iface->disable_location_gathering_finish = mm_shared_cinterion_disable_location_gathering_finish;
}

static MMIfaceModemLocation *
peek_parent_location_interface (MMSharedCinterion *self)
{
    return iface_modem_location_parent;
}

static void
iface_modem_voice_init (MMIfaceModemVoice *iface)
{
    iface_modem_voice_parent = g_type_interface_peek_parent (iface);

    iface->create_call = mm_shared_cinterion_create_call;

    iface->check_support                     = mm_shared_cinterion_voice_check_support;
    iface->check_support_finish              = mm_shared_cinterion_voice_check_support_finish;
    iface->enable_unsolicited_events         = mm_shared_cinterion_voice_enable_unsolicited_events;
    iface->enable_unsolicited_events_finish  = mm_shared_cinterion_voice_enable_unsolicited_events_finish;
    iface->disable_unsolicited_events        = mm_shared_cinterion_voice_disable_unsolicited_events;
    iface->disable_unsolicited_events_finish = mm_shared_cinterion_voice_disable_unsolicited_events_finish;
    iface->setup_unsolicited_events          = mm_shared_cinterion_voice_setup_unsolicited_events;
    iface->setup_unsolicited_events_finish   = mm_shared_cinterion_voice_setup_unsolicited_events_finish;
    iface->cleanup_unsolicited_events        = mm_shared_cinterion_voice_cleanup_unsolicited_events;
    iface->cleanup_unsolicited_events_finish = mm_shared_cinterion_voice_cleanup_unsolicited_events_finish;
}

static MMIfaceModemVoice *
peek_parent_voice_interface (MMSharedCinterion *self)
{
    return iface_modem_voice_parent;
}

static void
iface_modem_time_init (MMIfaceModemTime *iface)
{
    iface_modem_time_parent = g_type_interface_peek_parent (iface);

    iface->setup_unsolicited_events          = mm_shared_cinterion_time_setup_unsolicited_events;
    iface->setup_unsolicited_events_finish   = mm_shared_cinterion_time_setup_unsolicited_events_finish;
    iface->cleanup_unsolicited_events        = mm_shared_cinterion_time_cleanup_unsolicited_events;
    iface->cleanup_unsolicited_events_finish = mm_shared_cinterion_time_cleanup_unsolicited_events_finish;
}

static MMIfaceModemTime *
peek_parent_time_interface (MMSharedCinterion *self)
{
    return iface_modem_time_parent;
}

static void
shared_cinterion_init (MMSharedCinterion *iface)
{
    iface->peek_parent_interface          = peek_parent_interface;
    iface->peek_parent_location_interface = peek_parent_location_interface;
    iface->peek_parent_voice_interface    = peek_parent_voice_interface;
    iface->peek_parent_time_interface     = peek_parent_time_interface;
}

static void
mm_broadband_modem_mbim_cinterion_class_init (MMBroadbandModemMbimCinterionClass *klass)
{
}
