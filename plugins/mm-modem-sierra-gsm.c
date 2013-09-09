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
 * Copyright (C) 2008 - 2009 Novell, Inc.
 * Copyright (C) 2009 - 2010 Red Hat, Inc.
 */

#include <config.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mm-modem-sierra-gsm.h"
#include "mm-errors.h"
#include "mm-modem-simple.h"
#include "mm-callback-info.h"
#include "mm-modem-helpers.h"
#include "mm-log.h"
#include "mm-modem-icera.h"

static void modem_init (MMModem *modem_class);
static void modem_gsm_network_init (MMModemGsmNetwork *gsm_network_class);
static void modem_icera_init (MMModemIcera *icera_class);
static void modem_simple_init (MMModemSimple *class);

G_DEFINE_TYPE_EXTENDED (MMModemSierraGsm, mm_modem_sierra_gsm, MM_TYPE_GENERIC_GSM, 0,
                        G_IMPLEMENT_INTERFACE (MM_TYPE_MODEM, modem_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_MODEM_GSM_NETWORK, modem_gsm_network_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_MODEM_ICERA, modem_icera_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_MODEM_SIMPLE, modem_simple_init))

#define MM_MODEM_SIERRA_GSM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MM_TYPE_MODEM_SIERRA_GSM, MMModemSierraGsmPrivate))

typedef struct {
    guint enable_wait_id;
    gboolean has_net;
    gboolean has_lte;
    char *username;
    char *password;
    gboolean is_icera;
    MMModemIceraPrivate *icera;
} MMModemSierraGsmPrivate;

MMModem *
mm_modem_sierra_gsm_new (const char *device,
                         const char *driver,
                         const char *plugin,
                         guint32 vendor,
                         guint32 product,
                         gboolean has_lte)
{
    MMModem *modem;

    g_return_val_if_fail (device != NULL, NULL);
    g_return_val_if_fail (driver != NULL, NULL);
    g_return_val_if_fail (plugin != NULL, NULL);

    modem = (MMModem *) g_object_new (MM_TYPE_MODEM_SIERRA_GSM,
                                      MM_MODEM_MASTER_DEVICE, device,
                                      MM_MODEM_DRIVER, driver,
                                      MM_MODEM_PLUGIN, plugin,
                                      MM_MODEM_HW_VID, vendor,
                                      MM_MODEM_HW_PID, product,
                                      NULL);
    if (modem) {
        MM_MODEM_SIERRA_GSM_GET_PRIVATE (modem)->icera = mm_modem_icera_init_private ();
        MM_MODEM_SIERRA_GSM_GET_PRIVATE (modem)->has_lte = has_lte;
    }

    return modem;
}

/*****************************************************************************/

static void
get_allowed_mode_done (MMAtSerialPort *port,
                       GString *response,
                       GError *error,
                       gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;
    MMModemSierraGsmPrivate *priv;
    GRegex *r = NULL;
    GMatchInfo *match_info = NULL;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        info->error = g_error_copy (error);
        goto done;
    }

    /* Example response: !SELRAT: 03, UMTS 3G Preferred */
    r = g_regex_new ("!SELRAT:\\s*(\\d+).*$", 0, 0, NULL);
    if (!r) {
        info->error = g_error_new_literal (MM_MODEM_ERROR,
                                           MM_MODEM_ERROR_GENERAL,
                                           "Failed to parse the allowed mode response");
        goto done;
    }

    priv = MM_MODEM_SIERRA_GSM_GET_PRIVATE (info->modem);
    if (g_regex_match_full (r, response->str, response->len, 0, 0, &match_info, &info->error)) {
        MMModemGsmAllowedMode mode = MM_MODEM_GSM_ALLOWED_MODE_ANY;
        char *str;

        str = g_match_info_fetch (match_info, 1);
        switch (atoi (str)) {
        case 0:
            mode = MM_MODEM_GSM_ALLOWED_MODE_ANY;
            break;
        case 1:
            mode = MM_MODEM_GSM_ALLOWED_MODE_3G_ONLY;
            break;
        case 2:
            mode = MM_MODEM_GSM_ALLOWED_MODE_2G_ONLY;
            break;
        case 3:
            /* in Sierra LTE devices, mode 3 is automatic, including LTE, no preference */
            if (priv->has_lte)
                mode = MM_MODEM_GSM_ALLOWED_MODE_ANY;
            else
                mode = MM_MODEM_GSM_ALLOWED_MODE_3G_PREFERRED;
            break;
        case 4:
            /* in Sierra LTE devices, mode 4 is automatic, including LTE, no preference */
            if (priv->has_lte)
                mode = MM_MODEM_GSM_ALLOWED_MODE_ANY;
            else
                mode = MM_MODEM_GSM_ALLOWED_MODE_2G_PREFERRED;
            break;
        case 5:
            /* 2G and 3G only; but we can't represent that */
            mode = MM_MODEM_GSM_ALLOWED_MODE_3G_PREFERRED;
            break;
        case 6:
            mode = MM_MODEM_GSM_ALLOWED_MODE_4G_ONLY;
            break;
        case 7:
            mode = MM_MODEM_GSM_ALLOWED_MODE_4G_PREFERRED;
            break;
        default:
            info->error = g_error_new (MM_MODEM_ERROR,
                                       MM_MODEM_ERROR_GENERAL,
                                       "Failed to parse the allowed mode response: '%s'",
                                       response->str);
            break;
        }
        g_free (str);

        mm_callback_info_set_result (info, GUINT_TO_POINTER (mode), NULL);
    }

done:
    if (match_info)
        g_match_info_free (match_info);
    if (r)
        g_regex_unref (r);
    mm_callback_info_schedule (info);
}

static void
get_allowed_mode (MMGenericGsm *gsm,
                  MMModemUIntFn callback,
                  gpointer user_data)
{
    MMModemSierraGsm *self = MM_MODEM_SIERRA_GSM (gsm);
    MMCallbackInfo *info;
    MMAtSerialPort *primary;

    if (MM_MODEM_SIERRA_GSM_GET_PRIVATE (self)->is_icera) {
        mm_modem_icera_get_allowed_mode (MM_MODEM_ICERA (self), callback, user_data);
        return;
    }

    info = mm_callback_info_uint_new (MM_MODEM (gsm), callback, user_data);

    /* Sierra secondary ports don't have full AT command interpreters */
    primary = mm_generic_gsm_get_at_port (gsm, MM_AT_PORT_FLAG_PRIMARY);
    if (!primary || mm_port_get_connected (MM_PORT (primary))) {
        g_set_error_literal (&info->error, MM_MODEM_ERROR, MM_MODEM_ERROR_CONNECTED,
                             "Cannot perform this operation while connected");
        mm_callback_info_schedule (info);
        return;
    }

    mm_at_serial_port_queue_command (primary, "!SELRAT?", 3, get_allowed_mode_done, info);
}

static void
set_allowed_mode_done (MMAtSerialPort *port,
                       GString *response,
                       GError *error,
                       gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);

   mm_callback_info_schedule (info);
}

static void
set_allowed_mode (MMGenericGsm *gsm,
                  MMModemGsmAllowedMode mode,
                  MMModemFn callback,
                  gpointer user_data)
{
    MMModemSierraGsm *self = MM_MODEM_SIERRA_GSM (gsm);
    MMCallbackInfo *info;
    MMAtSerialPort *primary;
    char *command;
    int idx = 0;
    MMModemSierraGsmPrivate *priv;

    priv = MM_MODEM_SIERRA_GSM_GET_PRIVATE (self);

    if (priv->is_icera) {
        mm_modem_icera_set_allowed_mode (MM_MODEM_ICERA (self), mode, callback, user_data);
        return;
    }

    info = mm_callback_info_new (MM_MODEM (gsm), callback, user_data);

    /* Sierra secondary ports don't have full AT command interpreters */
    primary = mm_generic_gsm_get_at_port (gsm, MM_AT_PORT_FLAG_PRIMARY);
    if (!primary || mm_port_get_connected (MM_PORT (primary))) {
        g_set_error_literal (&info->error, MM_MODEM_ERROR, MM_MODEM_ERROR_CONNECTED,
                             "Cannot perform this operation while connected");
        mm_callback_info_schedule (info);
        return;
    }

    if (   priv->has_lte == FALSE
        && (   mode == MM_MODEM_GSM_ALLOWED_MODE_4G_ONLY
            || mode == MM_MODEM_GSM_ALLOWED_MODE_4G_PREFERRED)) {
        g_set_error_literal (&info->error, MM_MODEM_ERROR, MM_MODEM_ERROR_OPERATION_NOT_SUPPORTED,
                             "4G allowed modes requested, but modem does not support 4G");
        mm_callback_info_schedule (info);
        return;
    }

    switch (mode) {
    case MM_MODEM_GSM_ALLOWED_MODE_2G_ONLY:
        idx = 2;
        break;
    case MM_MODEM_GSM_ALLOWED_MODE_3G_ONLY:
        idx = 1;
        break;
    case MM_MODEM_GSM_ALLOWED_MODE_4G_ONLY:
        idx = 6;
        break;
    case MM_MODEM_GSM_ALLOWED_MODE_2G_PREFERRED:
        if (priv->has_lte)
            idx = 2; /* 2G preferred not supported, use 2G mode instead */
        else
            idx = 4;
        break;
    case MM_MODEM_GSM_ALLOWED_MODE_3G_PREFERRED:
        if (priv->has_lte)
            idx = 5; /* 3G preferred not supported, use 2G/3G mode instead */
        else
            idx = 3;
        break;
    case MM_MODEM_GSM_ALLOWED_MODE_4G_PREFERRED:
        idx = 7;
        break;
    case MM_MODEM_GSM_ALLOWED_MODE_ANY:
    default:
        break;
    }

    command = g_strdup_printf ("!SELRAT=%d", idx);
    mm_at_serial_port_queue_command (primary, command, 3, set_allowed_mode_done, info);
    g_free (command);
}

static void
get_act_request_done (MMAtSerialPort *port,
                      GString *response,
                      GError *error,
                      gpointer user_data)
{
    MMCallbackInfo *info = user_data;
    MMModemGsmAccessTech act = MM_MODEM_GSM_ACCESS_TECH_UNKNOWN;
    const char *p;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);
    else {
        p = mm_strip_tag (response->str, "*CNTI:");
        p = strchr (p, ',');
        if (p)
            act = mm_gsm_string_to_access_tech (p + 1);
    }

    mm_callback_info_set_result (info, GUINT_TO_POINTER (act), NULL);
    mm_callback_info_schedule (info);
}

static void
get_access_technology (MMGenericGsm *modem,
                       MMModemUIntFn callback,
                       gpointer user_data)
{
    MMModemSierraGsm *self = MM_MODEM_SIERRA_GSM (modem);
    MMAtSerialPort *port;
    MMCallbackInfo *info;

    if (MM_MODEM_SIERRA_GSM_GET_PRIVATE (self)->is_icera) {
        mm_modem_icera_get_access_technology (MM_MODEM_ICERA (self), callback, user_data);
        return;
    }

    info = mm_callback_info_uint_new (MM_MODEM (modem), callback, user_data);

    port = mm_generic_gsm_get_best_at_port (modem, &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    mm_at_serial_port_queue_command (port, "*CNTI=0", 3, get_act_request_done, info);
}

static void
set_band (MMModemGsmNetwork *modem,
          MMModemGsmBand band,
          MMModemFn callback,
          gpointer user_data)
{
    MMCallbackInfo *info;

    if (MM_MODEM_SIERRA_GSM_GET_PRIVATE (modem)->is_icera)
        mm_modem_icera_set_band (MM_MODEM_ICERA (modem), band, callback, user_data);
    else {
        info = mm_callback_info_new (MM_MODEM (modem), callback, user_data);
        info->error = g_error_new_literal (MM_MODEM_ERROR, MM_MODEM_ERROR_OPERATION_NOT_SUPPORTED,
                                           "Operation not supported");
        mm_callback_info_schedule (info);
    }
}

static void
get_band (MMModemGsmNetwork *modem,
          MMModemUIntFn callback,
          gpointer user_data)
{
    MMCallbackInfo *info;

    if (MM_MODEM_SIERRA_GSM_GET_PRIVATE (modem)->is_icera)
        mm_modem_icera_get_current_bands (MM_MODEM_ICERA (modem), callback, user_data);
    else {
        info = mm_callback_info_uint_new (MM_MODEM (modem), callback, user_data);
        info->error = g_error_new_literal (MM_MODEM_ERROR, MM_MODEM_ERROR_OPERATION_NOT_SUPPORTED,
                                           "Operation not supported");
        mm_callback_info_schedule (info);
    }
}

static void
get_supported_bands (MMGenericGsm *gsm,
                     MMModemUIntFn callback,
                     gpointer user_data)
{
    MMCallbackInfo *info;

    if (MM_MODEM_SIERRA_GSM_GET_PRIVATE (gsm)->is_icera)
        mm_modem_icera_get_supported_bands (MM_MODEM_ICERA (gsm), callback, user_data);
    else {
        info = mm_callback_info_uint_new (MM_MODEM (gsm), callback, user_data);
        info->error = g_error_new_literal (MM_MODEM_ERROR, MM_MODEM_ERROR_OPERATION_NOT_SUPPORTED,
                                           "Operation not supported");
        mm_callback_info_schedule (info);
    }
}

static void
get_sim_iccid_done (MMAtSerialPort *port,
                    GString *response,
                    GError *error,
                    gpointer user_data)
{
    MMCallbackInfo *info = user_data;
    const char *p;
    char *parsed;
    GError *local = NULL;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        info->error = g_error_copy (error);
        goto done;
    }

    p = mm_strip_tag (response->str, "!ICCID:");
    if (!p) {
        info->error = g_error_new_literal (MM_MODEM_ERROR,
                                           MM_MODEM_ERROR_GENERAL,
                                           "Failed to parse !ICCID response");
        goto done;
    }

    parsed = mm_gsm_parse_iccid (p, FALSE, &local);
    if (parsed)
        mm_callback_info_set_result (info, g_strdup (parsed), g_free);
    else {
        g_assert (local);
        info->error = local;
    }

done:
    mm_serial_port_close (MM_SERIAL_PORT (port));
    mm_callback_info_schedule (info);
}

static void
get_sim_iccid (MMGenericGsm *modem,
               MMModemStringFn callback,
               gpointer callback_data)
{
    MMAtSerialPort *port;
    MMCallbackInfo *info;
    GError *error = NULL;

    port = mm_generic_gsm_get_best_at_port (modem, &error);
    if (!port)
        goto error;

    if (!mm_serial_port_open (MM_SERIAL_PORT (port), &error))
        goto error;

    info = mm_callback_info_string_new (MM_MODEM (modem), callback, callback_data);
    mm_at_serial_port_queue_command (port, "!ICCID?", 3, get_sim_iccid_done, info);
    return;

error:
    callback (MM_MODEM (modem), NULL, error, callback_data);
    g_clear_error (&error);
}

/*****************************************************************************/
/*    Modem class override functions                                         */
/*****************************************************************************/

static void
icera_check_cb (MMModem *modem,
                guint32 result,
                GError *error,
                gpointer user_data)
{
    MMCallbackInfo *info = user_data;

    if (mm_callback_info_check_modem_removed (info))
        return;

    if (!error && result) {
        /* Turn on unsolicited network state messages */
        MM_MODEM_SIERRA_GSM_GET_PRIVATE (modem)->is_icera = TRUE;
        mm_modem_icera_change_unsolicited_messages (MM_MODEM_ICERA (info->modem), TRUE);
    }

    if (info->modem)
       MM_GENERIC_GSM_CLASS (mm_modem_sierra_gsm_parent_class)->do_enable_power_up_done (MM_GENERIC_GSM (info->modem), NULL, NULL, info);
}

static void
ws46_done_cb (MMAtSerialPort *port,
              GString *response,
              GError *error,
              gpointer user_data)
{
    MMCallbackInfo *info = user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    /* Ignore errors */
    if (!error && response) {
        if (strstr (response->str, "28") ||   /* 4G only */
            strstr (response->str, "30") ||   /* 2G/4G */
            strstr (response->str, "31")) {  /* 3G/4G */
            MM_MODEM_SIERRA_GSM_GET_PRIVATE (info->modem)->has_lte = TRUE;
        }
    }

    mm_modem_icera_is_icera (MM_MODEM_ICERA (info->modem), icera_check_cb, info);
}

static gboolean
sierra_enabled (gpointer user_data)
{
    MMCallbackInfo *info = user_data;
    MMGenericGsm *modem;
    MMModemSierraGsmPrivate *priv;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return FALSE;

    modem = MM_GENERIC_GSM (info->modem);
    priv = MM_MODEM_SIERRA_GSM_GET_PRIVATE (modem);
    priv->enable_wait_id = 0;

    if (priv->has_lte)
        mm_modem_icera_is_icera (MM_MODEM_ICERA (modem), icera_check_cb, info);
    else {
        MMAtSerialPort *primary;

        /* Some modems (e.g. Sierra Wireless MC7710 or ZTE MF820D) won't report LTE
         * capabilities even if they have them. So just run AT+WS46=? as well to see
         * if the current supported modes includes any LTE-specific mode.
         * This is not a big deal, as the AT+WS46=? command is a test command with a
         * cache-able result.
         *
         * E.g.:
         *  AT+WS46=?
         *   +WS46: (12,22,25,28,29)
         *   OK
         *
         */
        primary = mm_generic_gsm_get_at_port (MM_GENERIC_GSM (info->modem), MM_AT_PORT_FLAG_PRIMARY);
        g_assert (primary);
        mm_at_serial_port_queue_command (primary, "+WS46=?", 3, ws46_done_cb, info);
    }

    return FALSE;
}

static void
real_do_enable_power_up_done (MMGenericGsm *gsm,
                              GString *response,
                              GError *error,
                              MMCallbackInfo *info)
{
    MMModemSierraGsmPrivate *priv = MM_MODEM_SIERRA_GSM_GET_PRIVATE (gsm);
    char *driver = NULL;
    guint seconds = 10;

    if (error) {
        /* Chain up to parent */
        MM_GENERIC_GSM_CLASS (mm_modem_sierra_gsm_parent_class)->do_enable_power_up_done (gsm, NULL, error, info);
        return;
    }

    if (response == NULL) {
        /* If both error and response are NULL, that means the modem was already
         * powered up and the power-up command was skipped.  So we don't need to
         * wait for the modem to settle after CFUN=1 is sent.
         */
        sierra_enabled (info);
        return;
    }

    /* Many Sierra devices return OK immediately in response to CFUN=1 but
     * need some time to finish powering up, otherwise subsequent commands
     * may return failure or even crash the modem.  Give more time for older
     * devices like the AC860 and C885, which aren't driven by the 'sierra_net'
     * driver.  Assume any DirectIP (ie, sierra_net) device is new enough
     * to allow a lower timeout.
     */
    g_warn_if_fail (priv->enable_wait_id == 0);
    g_object_get (G_OBJECT (gsm), MM_MODEM_DRIVER, &driver, NULL);
    if (g_strcmp0 (driver, "sierra_net") == 0)
        seconds = 5;

    priv->enable_wait_id = g_timeout_add_seconds (seconds, sierra_enabled, info);
}

static void
get_current_functionality_status_cb (MMAtSerialPort *port,
                                     GString *response,
                                     GError *error,
                                     gpointer user_data)
{
    MMCallbackInfo *info = user_data;
    guint needed = FALSE;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    /* On error, just assume we don't need the power-up command */
    if (!error) {
        const gchar *p;

        p = mm_strip_tag (response->str, "+CFUN:");
        if (p && *p == '1') {
            /* If reported functionality status is '1', then we do not need to
             * issue the power-up command. Otherwise, do it. */
            mm_dbg ("Already in full functionality status, skipping power-up command");
        } else {
            needed = TRUE;
            mm_warn ("Not in full functionality status, power-up command is needed.");
        }
    } else
        mm_warn ("Failed checking if power-up command is needed: '%s'. "
                 "Will assume it isn't.",
                 error->message);

    /* Set result and schedule */
    mm_callback_info_set_result (info,
                                 GUINT_TO_POINTER (needed),
                                 NULL);
    mm_callback_info_schedule (info);
}

static void
do_enable_power_up_check_needed (MMGenericGsm *self,
                                 MMModemUIntFn callback,
                                 gpointer user_data)
{
    MMAtSerialPort *primary;
    MMCallbackInfo *info;

    info = mm_callback_info_uint_new (MM_MODEM (self), callback, user_data);

    /* Get port */
    primary = mm_generic_gsm_get_at_port (self, MM_AT_PORT_FLAG_PRIMARY);
    g_assert (primary);

    /* Get current functionality status */
    mm_dbg ("Getting current functionality status...");
    mm_at_serial_port_queue_command (primary, "+CFUN?", 3, get_current_functionality_status_cb, info);
}

static void
port_grabbed (MMGenericGsm *gsm,
              MMPort *port,
              MMAtPortFlags pflags,
              gpointer user_data)
{
    GRegex *regex;

    if (MM_IS_AT_SERIAL_PORT (port)) {
        g_object_set (G_OBJECT (port), MM_PORT_CARRIER_DETECT, FALSE, NULL);

        if (pflags == MM_AT_PORT_FLAG_SECONDARY) {
            /* Built-in echo removal conflicts with the APP1 port's limited AT
             * parser, which doesn't always prefix responses with <CR><LF>.
             */
            g_object_set (G_OBJECT (port), MM_AT_SERIAL_PORT_REMOVE_ECHO, FALSE, NULL);
        }

        regex = g_regex_new ("\\r\\n\\+PACSP0\\r\\n", G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
        mm_at_serial_port_add_unsolicited_msg_handler (MM_AT_SERIAL_PORT (port), regex, NULL, NULL, NULL);
        g_regex_unref (regex);

        /* Add Icera-specific handlers */
        mm_modem_icera_register_unsolicted_handlers (MM_MODEM_ICERA (gsm), MM_AT_SERIAL_PORT (port));
    } else if (mm_port_get_subsys (port) == MM_PORT_SUBSYS_NET) {
        MM_MODEM_SIERRA_GSM_GET_PRIVATE (gsm)->has_net = TRUE;
        g_object_set (G_OBJECT (gsm), MM_MODEM_IP_METHOD, MM_MODEM_IP_METHOD_DHCP, NULL);
    }
}

static void
ppp_connect_done (MMModem *modem, GError *error, gpointer user_data)
{
    MMCallbackInfo *info = user_data;

    /* Do nothing if modem removed */
    if (!modem || mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);

    mm_callback_info_schedule (info);
}

static void
net_activate_done (MMAtSerialPort *port,
                   GString *response,
                   GError *error,
                   gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    mm_generic_gsm_connect_complete (MM_GENERIC_GSM (info->modem), error, info);
}


static void
auth_done (MMAtSerialPort *port,
           GString *response,
           GError *error,
           gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;
    gint cid;
    char *command;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        info->error = g_error_copy (error);
        mm_callback_info_schedule (info);
        return;
    }

    cid = mm_generic_gsm_get_cid (MM_GENERIC_GSM (info->modem));
    g_warn_if_fail (cid >= 0);

    /* Activate data on the net port */
    command = g_strdup_printf ("!SCACT=1,%d", cid);
    mm_at_serial_port_queue_command (port, command, 10, net_activate_done, info);
    g_free (command);
}

static void
ps_attach_done (MMAtSerialPort *port,
                GString *response,
                GError *error,
                gpointer user_data)
{
    MMCallbackInfo *info = user_data;
    MMModemSierraGsmPrivate *priv;
    MMModem *parent_modem_iface;
    const char *number;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        info->error = g_error_copy (error);
        mm_callback_info_schedule (info);
        return;
    }

    priv = MM_MODEM_SIERRA_GSM_GET_PRIVATE (info->modem);
    if (priv->has_net) {
        gint cid;
        char *command;

        /* If we have a net interface, we don't do PPP */

        cid = mm_generic_gsm_get_cid (MM_GENERIC_GSM (info->modem));
        g_warn_if_fail (cid >= 0);

        if (!priv->username || !priv->password)
            command = g_strdup_printf ("$QCPDPP=%d,0", cid);
        else {
            command = g_strdup_printf ("$QCPDPP=%d,1,\"%s\",\"%s\"",
                                       cid,
                                       priv->password ? priv->password : "",
                                       priv->username ? priv->username : "");

        }

        mm_at_serial_port_queue_command (port, command, 3, auth_done, info);
        g_free (command);
    } else {
        /* We've got a PS attach, chain up to parent for the connect */
        number = mm_callback_info_get_data (info, "number");
        parent_modem_iface = g_type_interface_peek_parent (MM_MODEM_GET_INTERFACE (info->modem));
        parent_modem_iface->connect (info->modem, number, ppp_connect_done, info);
    }
}

static void
do_connect (MMModem *modem,
            const char *number,
            MMModemFn callback,
            gpointer user_data)
{
    MMCallbackInfo *info;
    MMAtSerialPort *port;

    if (MM_MODEM_SIERRA_GSM_GET_PRIVATE (modem)->is_icera) {
        mm_modem_icera_do_connect (MM_MODEM_ICERA (modem), number, callback, user_data);
        return;
    }

    mm_modem_set_state (modem, MM_MODEM_STATE_CONNECTING, MM_MODEM_STATE_REASON_NONE);

    info = mm_callback_info_new (modem, callback, user_data);
    mm_callback_info_set_data (info, "number", g_strdup (number), g_free);

    port = mm_generic_gsm_get_best_at_port (MM_GENERIC_GSM (modem), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    /* Try to initiate a PS attach.  Some Sierra modems can get into a
     * state where if there is no PS attach when dialing, the next time
     * the modem tries to connect it won't ever register with the network.
     */
    mm_at_serial_port_queue_command (port, "+CGATT=1", 10, ps_attach_done, info);
}

static void
get_ip4_config (MMModem *modem,
                MMModemIp4Fn callback,
                gpointer user_data)
{
    MMModem *parent_iface;

    if (MM_MODEM_SIERRA_GSM_GET_PRIVATE (modem)->is_icera) {
        mm_modem_icera_get_ip4_config (MM_MODEM_ICERA (modem), callback, user_data);
    } else {
        parent_iface = g_type_interface_peek_parent (MM_MODEM_GET_INTERFACE (modem));
        parent_iface->get_ip4_config (modem, callback, user_data);
    }
}

static void
clear_user_pass (MMModemSierraGsm *self)
{
    MMModemSierraGsmPrivate *priv = MM_MODEM_SIERRA_GSM_GET_PRIVATE (self);

    g_free (priv->username);
    priv->username = NULL;
    g_free (priv->password);
    priv->password = NULL;
}

static void
do_disconnect (MMGenericGsm *gsm,
               gint cid,
               MMModemFn callback,
               gpointer user_data)
{
    MMModemSierraGsm *self = MM_MODEM_SIERRA_GSM (gsm);
    MMModemSierraGsmPrivate *priv = MM_MODEM_SIERRA_GSM_GET_PRIVATE (self);

    if (priv->is_icera) {
        mm_modem_icera_do_disconnect (gsm, cid, callback, user_data);
        return;
    }

    clear_user_pass (self);

    if (priv->has_net) {
        MMAtSerialPort *primary;
        char *command;

        primary = mm_generic_gsm_get_at_port (gsm, MM_AT_PORT_FLAG_PRIMARY);
        g_assert (primary);

        /* If we have a net interface, deactivate it */
        command = g_strdup_printf ("!SCACT=0,%d", cid);
        mm_at_serial_port_queue_command (primary, command, 3, NULL, NULL);
        g_free (command);
    }

    MM_GENERIC_GSM_CLASS (mm_modem_sierra_gsm_parent_class)->do_disconnect (gsm, cid, callback, user_data);
}


/*****************************************************************************/

static void
disable_unsolicited_done (MMAtSerialPort *port,
                          GString *response,
                          GError *error,
                          gpointer user_data)

{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    /* Ignore all errors */
    mm_callback_info_schedule (info);
}

static void
invoke_call_parent_disable_fn (MMCallbackInfo *info)
{
    /* Note: we won't call the parent disable if info->modem is no longer
     * valid. The invoke is called always once the info gets scheduled, which
     * may happen during removed modem detection. */
    if (info->modem) {
        MMModem *parent_modem_iface;

        parent_modem_iface = g_type_interface_peek_parent (MM_MODEM_GET_INTERFACE (info->modem));
        parent_modem_iface->disable (info->modem, (MMModemFn)info->callback, info->user_data);
    }
}

static void
do_disable (MMModem *modem,
            MMModemFn callback,
            gpointer user_data)
{
    MMModemSierraGsmPrivate *priv = MM_MODEM_SIERRA_GSM_GET_PRIVATE (modem);
    MMAtSerialPort *primary;
    MMCallbackInfo *info;

    info = mm_callback_info_new_full (modem,
                                      invoke_call_parent_disable_fn,
                                      (GCallback)callback,
                                      user_data);

    primary = mm_generic_gsm_get_at_port (MM_GENERIC_GSM (modem), MM_AT_PORT_FLAG_PRIMARY);
    g_assert (primary);

    /* Turn off unsolicited responses */
    if (priv->is_icera) {
        mm_modem_icera_cleanup (MM_MODEM_ICERA (modem));
        mm_modem_icera_change_unsolicited_messages (MM_MODEM_ICERA (modem), FALSE);
    }

    /* Random command to ensure unsolicited message disable completes */
    mm_at_serial_port_queue_command (primary, "E0", 5, disable_unsolicited_done, info);
}

/*****************************************************************************/
/*    Simple Modem class override functions                                  */
/*****************************************************************************/

static char *
simple_dup_string_property (GHashTable *properties, const char *name, GError **error)
{
    GValue *value;

    value = (GValue *) g_hash_table_lookup (properties, name);
    if (!value)
        return NULL;

    if (G_VALUE_HOLDS_STRING (value))
        return g_value_dup_string (value);

    g_set_error (error, MM_MODEM_ERROR, MM_MODEM_ERROR_GENERAL,
                 "Invalid property type for '%s': %s (string expected)",
                 name, G_VALUE_TYPE_NAME (value));

    return NULL;
}

static void
simple_connect (MMModemSimple *simple,
                GHashTable *properties,
                MMModemFn callback,
                gpointer user_data)
{
    MMModemSierraGsmPrivate *priv = MM_MODEM_SIERRA_GSM_GET_PRIVATE (simple);
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;
    MMModemSimple *parent_iface;

    if (priv->is_icera) {
        mm_modem_icera_simple_connect (MM_MODEM_ICERA (simple), properties);
    } else {
        clear_user_pass (MM_MODEM_SIERRA_GSM (simple));
        priv->username = simple_dup_string_property (properties, "username", &info->error);
        priv->password = simple_dup_string_property (properties, "password", &info->error);
    }

    parent_iface = g_type_interface_peek_parent (MM_MODEM_SIMPLE_GET_INTERFACE (simple));
    parent_iface->connect (MM_MODEM_SIMPLE (simple), properties, callback, info);
}

/*****************************************************************************/

static MMModemIceraPrivate *
get_icera_private (MMModemIcera *icera)
{
    return MM_MODEM_SIERRA_GSM_GET_PRIVATE (icera)->icera;
}

static char *
get_icera_call_control_cmd (MMModemIcera *icera, guint32 cid, gboolean activate)
{
    /* Sierra devices with Icera chipsets (USB 305/AT&T Lightning) still
     * support Sierra call control commands, and using these allow DHCP
     * to be used instead of static addressing.  The USB305 sometimes has
     * problems with addressing when using the Icera commands, but the Sierra
     * ones work fine.
     */
    return g_strdup_printf ("!SCACT=%d,%d", activate ? 1 : 0, cid);
}

/*****************************************************************************/

static void
modem_init (MMModem *modem_class)
{
    modem_class->connect = do_connect;
    modem_class->disable = do_disable;
    modem_class->get_ip4_config = get_ip4_config;
}

static void
modem_icera_init (MMModemIcera *icera)
{
    icera->get_private = get_icera_private;
    icera->get_call_control_cmd = get_icera_call_control_cmd;
}

static void
modem_gsm_network_init (MMModemGsmNetwork *class)
{
    class->set_band = set_band;
    class->get_band = get_band;
}

static void
modem_simple_init (MMModemSimple *class)
{
    class->connect = simple_connect;
}

static void
mm_modem_sierra_gsm_init (MMModemSierraGsm *self)
{
}

static void
dispose (GObject *object)
{
    MMModemSierraGsm *self = MM_MODEM_SIERRA_GSM (object);
    MMModemSierraGsmPrivate *priv = MM_MODEM_SIERRA_GSM_GET_PRIVATE (self);

    if (priv->enable_wait_id)
        g_source_remove (priv->enable_wait_id);

    mm_modem_icera_dispose_private (MM_MODEM_ICERA (self));

    clear_user_pass (self);
}

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    /* Do nothing... see set_property() in parent, which also does nothing */
}

static void
get_property (GObject *object,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
    switch (prop_id) {
    case MM_GENERIC_GSM_PROP_POWER_DOWN_CMD:
        /* Use AT+CFUN=4 for power down (low power mode) */
        g_value_set_string (value, "+CFUN=4");
        break;
    case MM_GENERIC_GSM_PROP_POWER_UP_CMD:
        /* Use AT+CFUN=1,0 for power up, to avoid reset that +cfun=1 can trigger in some modems */
        g_value_set_string (value, "+CFUN=1,0");
        break;
    default:
        break;
    }
}

static void
mm_modem_sierra_gsm_class_init (MMModemSierraGsmClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    MMGenericGsmClass *gsm_class = MM_GENERIC_GSM_CLASS (klass);

    mm_modem_sierra_gsm_parent_class = g_type_class_peek_parent (klass);
    g_type_class_add_private (object_class, sizeof (MMModemSierraGsmPrivate));

    object_class->dispose = dispose;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    g_object_class_override_property (object_class,
                                      MM_GENERIC_GSM_PROP_POWER_DOWN_CMD,
                                      MM_GENERIC_GSM_POWER_DOWN_CMD);

    g_object_class_override_property (object_class,
                                      MM_GENERIC_GSM_PROP_POWER_UP_CMD,
                                      MM_GENERIC_GSM_POWER_UP_CMD);

    gsm_class->port_grabbed = port_grabbed;
    gsm_class->do_enable_power_up_check_needed = do_enable_power_up_check_needed;
    gsm_class->do_enable_power_up_done = real_do_enable_power_up_done;
    gsm_class->set_allowed_mode = set_allowed_mode;
    gsm_class->get_allowed_mode = get_allowed_mode;
    gsm_class->get_access_technology = get_access_technology;
    gsm_class->get_sim_iccid = get_sim_iccid;
    gsm_class->do_disconnect = do_disconnect;
    gsm_class->get_supported_bands = get_supported_bands;
}

