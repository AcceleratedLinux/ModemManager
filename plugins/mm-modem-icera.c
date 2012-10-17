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
 * Copyright (C) 2010 Red Hat, Inc.
 */

/******************************************
 * Generic utilities for Icera-based modems
 ******************************************/

#include <config.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "mm-modem-icera.h"

#include "mm-modem.h"
#include "mm-errors.h"
#include "mm-callback-info.h"
#include "mm-at-serial-port.h"
#include "mm-generic-gsm.h"
#include "mm-modem-helpers.h"
#include "mm-utils.h"
#include "mm-log.h"

struct _MMModemIceraPrivate {
    /* Pending connection attempt */
    MMCallbackInfo *connect_pending_data;
    guint connect_pending_id;
    guint configure_context_id;
    guint configure_context_tries;

    char *username;
    char *password;

    MMModemGsmAccessTech last_act;
};

#define MM_MODEM_ICERA_GET_PRIVATE(m) (MM_MODEM_ICERA_GET_INTERFACE (m)->get_private(m))

static void connect_pending_done (MMModemIcera *self);
static void icera_call_control (MMModemIcera *self,
                                guint32 cid,
                                gboolean activate,
                                MMAtSerialResponseFn callback,
                                gpointer user_data);

static void
cleanup_configure_context (MMModemIcera *self)
{
    MMModemIceraPrivate *priv = MM_MODEM_ICERA_GET_PRIVATE (self);

    if (priv->configure_context_id != 0) {
        g_source_remove (priv->configure_context_id);
        priv->configure_context_id = 0;
    }
}

static void
get_allowed_mode_done (MMAtSerialPort *port,
                       GString *response,
                       GError *error,
                       gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;
    gboolean parsed = FALSE;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);
    else if (!g_str_has_prefix (response->str, "%IPSYS: ")) {
        int a, b;

        if (sscanf (response->str + 8, "%d,%d", &a, &b)) {
            MMModemGsmAllowedMode mode = MM_MODEM_GSM_ALLOWED_MODE_ANY;

            switch (a) {
            case 0:
                mode = MM_MODEM_GSM_ALLOWED_MODE_2G_ONLY;
                break;
            case 1:
                mode = MM_MODEM_GSM_ALLOWED_MODE_3G_ONLY;
                break;
            case 2:
                mode = MM_MODEM_GSM_ALLOWED_MODE_2G_PREFERRED;
                break;
            case 3:
                mode = MM_MODEM_GSM_ALLOWED_MODE_3G_PREFERRED;
                break;
            default:
                break;
            }

            mm_callback_info_set_result (info, GUINT_TO_POINTER (mode), NULL);
            parsed = TRUE;
        }
    }

    if (!error && !parsed)
        info->error = g_error_new_literal (MM_MODEM_ERROR, MM_MODEM_ERROR_GENERAL,
                                           "Could not parse allowed mode results");

    mm_callback_info_schedule (info);
}

void
mm_modem_icera_get_allowed_mode (MMModemIcera *self,
                                 MMModemUIntFn callback,
                                 gpointer user_data)
{
    MMCallbackInfo *info;
    MMAtSerialPort *port;

    info = mm_callback_info_uint_new (MM_MODEM (self), callback, user_data);

    port = mm_generic_gsm_get_best_at_port (MM_GENERIC_GSM (self), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }
    mm_at_serial_port_queue_command (port, "%IPSYS?", 3, get_allowed_mode_done, info);
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

void
mm_modem_icera_set_allowed_mode (MMModemIcera *self,
                                 MMModemGsmAllowedMode mode,
                                 MMModemFn callback,
                                 gpointer user_data)
{
    MMCallbackInfo *info;
    MMAtSerialPort *port;
    char *command;
    int i;

    info = mm_callback_info_new (MM_MODEM (self), callback, user_data);

    port = mm_generic_gsm_get_best_at_port (MM_GENERIC_GSM (self), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    switch (mode) {
    case MM_MODEM_GSM_ALLOWED_MODE_2G_ONLY:
        i = 0;
        break;
    case MM_MODEM_GSM_ALLOWED_MODE_3G_ONLY:
        i = 1;
        break;
    case MM_MODEM_GSM_ALLOWED_MODE_2G_PREFERRED:
        i = 2;
        break;
    case MM_MODEM_GSM_ALLOWED_MODE_3G_PREFERRED:
        i = 3;
        break;
    case MM_MODEM_GSM_ALLOWED_MODE_ANY:
    default:
        i = 5;
        break;
    }

    command = g_strdup_printf ("%%IPSYS=%d", i);
    mm_at_serial_port_queue_command (port, command, 3, set_allowed_mode_done, info);
    g_free (command);
}

static MMModemGsmAccessTech
nwstate_to_act (const char *str)
{
    /* small 'g' means CS, big 'G' means PS */
    if (!strcmp (str, "2g"))
        return MM_MODEM_GSM_ACCESS_TECH_GSM;
    else if (!strcmp (str, "2G-GPRS"))
        return MM_MODEM_GSM_ACCESS_TECH_GPRS;
    else if (!strcmp (str, "2G-EDGE"))
        return MM_MODEM_GSM_ACCESS_TECH_EDGE;
    else if (!strcmp (str, "3G"))
        return MM_MODEM_GSM_ACCESS_TECH_UMTS;
    else if (!strcmp (str, "3g"))
        return MM_MODEM_GSM_ACCESS_TECH_UMTS;
    else if (!strcmp (str, "R99"))
        return MM_MODEM_GSM_ACCESS_TECH_UMTS;
    else if (!strcmp (str, "3G-HSDPA") || !strcmp (str, "HSDPA"))
        return MM_MODEM_GSM_ACCESS_TECH_HSDPA;
    else if (!strcmp (str, "3G-HSUPA") || !strcmp (str, "HSUPA"))
        return MM_MODEM_GSM_ACCESS_TECH_HSUPA;
    else if (!strcmp (str, "3G-HSDPA-HSUPA") || !strcmp (str, "HSDPA-HSUPA"))
        return MM_MODEM_GSM_ACCESS_TECH_HSPA;
    else if (!strcmp (str, "3G-HSDPA-HSUPA-HSPA+") || !strcmp (str, "HSDPA-HSUPA-HSPA+"))
        return MM_MODEM_GSM_ACCESS_TECH_HSPA_PLUS;

    return MM_MODEM_GSM_ACCESS_TECH_UNKNOWN;
}

static void
nwstate_changed (MMAtSerialPort *port,
                 GMatchInfo *info,
                 gpointer user_data)
{
    MMModemIcera *self = MM_MODEM_ICERA (user_data);
    MMModemGsmAccessTech act = MM_MODEM_GSM_ACCESS_TECH_UNKNOWN;
    char *str;
    int rssi = -1;

    str = g_match_info_fetch (info, 1);
    if (str) {
        rssi = atoi (str);
        rssi = CLAMP (rssi, -1, 5);
        g_free (str);
    }

    /* Check the <connection state> field first for the connected access
     * technology, otherwise if not connected (ie, "-") use the available
     * access technology from the <tech> field.
     */
    str = g_match_info_fetch (info, 4);
    if (!str || (strcmp (str, "-") == 0)) {
        g_free (str);
        str = g_match_info_fetch (info, 3);
    }
    if (str) {
        act = nwstate_to_act (str);
        g_free (str);
    }

    MM_MODEM_ICERA_GET_PRIVATE (self)->last_act = act;
    mm_generic_gsm_update_access_technology (MM_GENERIC_GSM (self), act);
}

static void
pacsp_received (MMAtSerialPort *port,
                GMatchInfo *info,
                gpointer user_data)
{
    return;
}

static void
get_nwstate_done (MMAtSerialPort *port,
                  GString *response,
                  GError *error,
                  gpointer user_data)
{
    MMCallbackInfo *info = user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);
    else {
        MMModemIcera *self = MM_MODEM_ICERA (info->modem);
        MMModemIceraPrivate *priv = MM_MODEM_ICERA_GET_PRIVATE (self);

        /* The unsolicited message handler will already have run and
         * removed the NWSTATE response, so we have to work around that.
         */
        mm_callback_info_set_result (info, GUINT_TO_POINTER (priv->last_act), NULL);
        priv->last_act = MM_MODEM_GSM_ACCESS_TECH_UNKNOWN;
    }

    mm_callback_info_schedule (info);
}

void
mm_modem_icera_get_access_technology (MMModemIcera *self,
                                      MMModemUIntFn callback,
                                      gpointer user_data)
{
    MMAtSerialPort *port;
    MMCallbackInfo *info;

    info = mm_callback_info_uint_new (MM_MODEM (self), callback, user_data);

    port = mm_generic_gsm_get_best_at_port (MM_GENERIC_GSM (self), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    mm_at_serial_port_queue_command (port, "%NWSTATE=1", 3, get_nwstate_done, info);
}

/****************************************************************/

static void
disconnect_done (MMAtSerialPort *port,
                 GString *response,
                 GError *error,
                 gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    mm_callback_info_schedule (info);
}

void
mm_modem_icera_do_disconnect (MMGenericGsm *gsm,
                              gint cid,
                              MMModemFn callback,
                              gpointer user_data)
{
    MMCallbackInfo *info;

    /* Cleanup any running connect stuff */
    cleanup_configure_context (MM_MODEM_ICERA (gsm));
    connect_pending_done (MM_MODEM_ICERA (gsm));

    info = mm_callback_info_new (MM_MODEM (gsm), callback, user_data);

    icera_call_control (MM_MODEM_ICERA (gsm), cid, FALSE, disconnect_done, info);
}

/*****************************************************************************/

static void
connect_pending_done (MMModemIcera *self)
{
    MMModemIceraPrivate *priv = MM_MODEM_ICERA_GET_PRIVATE (self);
    GError *error = NULL;

    if (priv->connect_pending_data) {
        if (priv->connect_pending_data->error) {
            error = priv->connect_pending_data->error;
            priv->connect_pending_data->error = NULL;
        }

        /* Complete the connect */
        mm_generic_gsm_connect_complete (MM_GENERIC_GSM (self), error, priv->connect_pending_data);
        priv->connect_pending_data = NULL;
    }

    if (priv->connect_pending_id) {
        g_source_remove (priv->connect_pending_id);
        priv->connect_pending_id = 0;
    }
}

static void
icera_disconnect_done (MMModem *modem,
                       GError *error,
                       gpointer user_data)
{
    mm_info ("Modem signaled disconnection from the network");
}

static void
query_network_error_code_done (MMAtSerialPort *port,
                               GString *response,
                               GError *error,
                               gpointer user_data)
{
    MMModemIcera *self = MM_MODEM_ICERA (user_data);
    MMModemIceraPrivate *priv = MM_MODEM_ICERA_GET_PRIVATE (self);
    MMCallbackInfo *info = priv->connect_pending_data;
    int nw_activation_err;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if ((error == NULL) && g_str_has_prefix (response->str, "%IER: ")) {
        if (sscanf (response->str + 6, "%*d,%*d,%d", &nw_activation_err)) {
            /* 3GPP TS 24.008 Annex G error codes:
             * 27 - Unknown or missing access point name
             * 33 - Requested service option not subscribed
             */
            if (nw_activation_err == 27 || nw_activation_err == 33)
                info->error = mm_mobile_error_for_code (MM_MOBILE_ERROR_GPRS_NOT_SUBSCRIBED);
        }
    }

    if (info->error == NULL) {
        /* Generic error since parsing the specific one didn't work */
        info->error = g_error_new_literal (MM_MODEM_ERROR,
                                           MM_MODEM_ERROR_GENERAL,
                                           "Call setup failed");
    }
    connect_pending_done (self);
}

static void
connection_enabled (MMAtSerialPort *port,
                    GMatchInfo *match_info,
                    gpointer user_data)
{
    MMModemIcera *self = MM_MODEM_ICERA (user_data);
    MMAtSerialPort *primary;
    char *str;
    int status, cid, tmp;

    cid = mm_generic_gsm_get_cid (MM_GENERIC_GSM (self));
    if (cid < 0)
        return;

    str = g_match_info_fetch (match_info, 1);
    g_return_if_fail (str != NULL);
    tmp = atoi (str);
    g_free (str);

    /* Make sure the unsolicited message's CID matches the current CID */
    if (tmp != cid)
        return;

    str = g_match_info_fetch (match_info, 2);
    g_return_if_fail (str != NULL);
    status = atoi (str);
    g_free (str);

    switch (status) {
    case 0:
        /* Disconnected */
        if (mm_modem_get_state (MM_MODEM (self)) >= MM_MODEM_STATE_CONNECTED)
            mm_modem_disconnect (MM_MODEM (self), MM_MODEM_STATE_REASON_NONE, icera_disconnect_done, NULL);
        break;
    case 1:
        /* Connected */
        connect_pending_done (self);
        break;
    case 2:
        /* Connecting */
        break;
    case 3:
        /* Call setup failure? */
        primary = mm_generic_gsm_get_at_port (MM_GENERIC_GSM(self), MM_AT_PORT_FLAG_PRIMARY);
        g_assert (primary);
        /* Get additional error details */
        mm_at_serial_port_queue_command (primary, "AT%IER?", 3,
                                         query_network_error_code_done, self);
        break;
    default:
        mm_warn ("Unknown Icera connect status %d", status);
        break;
    }
}

/****************************************************************/

static gint
_get_cid (MMModemIcera *self)
{
    gint cid;

    cid = mm_generic_gsm_get_cid (MM_GENERIC_GSM (self));
    if (cid < 0) {
        g_warn_if_fail (cid >= 0);
        cid = 0;
    }
    return cid;
}

static void
icera_call_control (MMModemIcera *self,
                    guint32 cid,
                    gboolean activate,
                    MMAtSerialResponseFn callback,
                    gpointer user_data)
{
    char *command;
    MMAtSerialPort *primary;

    primary = mm_generic_gsm_get_at_port (MM_GENERIC_GSM (self), MM_AT_PORT_FLAG_PRIMARY);
    g_assert (primary);

    /* Firmware might have a custom call control command (ie, Sierra) */
    if (MM_MODEM_ICERA_GET_INTERFACE (self)->get_call_control_cmd)
        command = MM_MODEM_ICERA_GET_INTERFACE (self)->get_call_control_cmd (self, cid, activate);
    else
        command = g_strdup_printf ("%%IPDPACT=%d,%d", cid, activate ? 1 : 0);

    mm_at_serial_port_queue_command (primary, command, 15, callback, user_data);
    g_free (command);
}

static void
timeout_done (MMAtSerialPort *port,
              GString *response,
              GError *error,
              gpointer user_data)
{
    connect_pending_done (MM_MODEM_ICERA (user_data));
}

static gboolean
icera_connect_timed_out (gpointer data)
{
    MMModemIcera *self = MM_MODEM_ICERA (data);
    MMModemIceraPrivate *priv = MM_MODEM_ICERA_GET_PRIVATE (self);
    MMCallbackInfo *info = priv->connect_pending_data;

    priv->connect_pending_id = 0;

    if (info) {
        info->error = g_error_new_literal (MM_SERIAL_ERROR,
                                           MM_SERIAL_ERROR_RESPONSE_TIMEOUT,
                                           "Connection timed out");
    }

    icera_call_control (self, _get_cid (self), FALSE, timeout_done, self);
    return FALSE;
}

static void
icera_connected (MMAtSerialPort *port,
                 GString *response,
                 GError *error,
                 gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        mm_generic_gsm_connect_complete (MM_GENERIC_GSM (info->modem), error, info);
    } else {
        MMModemIcera *self = MM_MODEM_ICERA (info->modem);
        MMModemIceraPrivate *priv = MM_MODEM_ICERA_GET_PRIVATE (self);

        g_warn_if_fail (priv->connect_pending_id == 0);
        if (priv->connect_pending_id)
            g_source_remove (priv->connect_pending_id);

        priv->connect_pending_data = info;
        priv->connect_pending_id = g_timeout_add_seconds (30, icera_connect_timed_out, self);

        /* If the implementor has a custom call control command, then assume
         * that it returns OK only when the connection is ready, and thus all
         * we have to do is complete the connection request. Otherwise, if
         * using standard Icera commands, we wait for the connection indication
         * via %IPDPACT unsolicited messages.
         */
        if (MM_MODEM_ICERA_GET_INTERFACE (self)->get_call_control_cmd)
            connect_pending_done (self);
    }
}

static void
old_context_clear_done (MMAtSerialPort *port,
                        GString *response,
                        GError *error,
                        gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    /* Activate the PDP context and start the data session */
    icera_call_control (MM_MODEM_ICERA (info->modem),
                        _get_cid (MM_MODEM_ICERA (info->modem)),
                        TRUE,
                        icera_connected,
                        info);
}

static void configure_context (MMAtSerialPort *port, MMCallbackInfo *info,
                               char *username, char *password, gint cid);

static gboolean
retry_config_context (gpointer data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) data;
    MMModemIcera *self = MM_MODEM_ICERA (info->modem);
    MMModemIceraPrivate *priv = MM_MODEM_ICERA_GET_PRIVATE (self);
    MMAtSerialPort *primary;

    priv->configure_context_id = 0;
    primary = mm_generic_gsm_get_at_port (MM_GENERIC_GSM (self), MM_AT_PORT_FLAG_PRIMARY);
    g_assert (primary);
    configure_context (primary, info, priv->username, priv->password, _get_cid (self));
    return FALSE;
}

static void
auth_done (MMAtSerialPort *port,
           GString *response,
           GError *error,
           gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;
    MMModemIcera *self = MM_MODEM_ICERA (info->modem);
    MMModemIceraPrivate *priv = MM_MODEM_ICERA_GET_PRIVATE (self);

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        /* Retry configuring the context. It sometimes fails with a 583
         * error ["a profile (CID) is currently active"] if a connect
         * is attempted too soon after a disconnect. */
        if (++priv->configure_context_tries < 3) {
            priv->configure_context_id =
                    g_timeout_add_seconds (1, retry_config_context, info);
            return;
        }
        mm_generic_gsm_connect_complete (MM_GENERIC_GSM (info->modem), error, info);
    } else {
        /* Ensure the PDP context is deactivated */
        icera_call_control (self, _get_cid (self), FALSE, old_context_clear_done, info);
    }
}

static void
configure_context(MMAtSerialPort *port, MMCallbackInfo *info,
                  char *username, char *password, gint cid)
{
    char *command;

    /* Both user and password are required; otherwise firmware returns an error */
    if (!username || !password)
		command = g_strdup_printf ("%%IPDPCFG=%d,0,0,\"\",\"\"", cid);
    else {
        command = g_strdup_printf ("%%IPDPCFG=%d,0,1,\"%s\",\"%s\"",
                                   cid,
                                   username ? username : "",
                                   password ? password : "");

    }

    mm_at_serial_port_queue_command (port, command, 3, auth_done, info);
    g_free (command);
}

void
mm_modem_icera_do_connect (MMModemIcera *self,
                           const char *number,
                           MMModemFn callback,
                           gpointer user_data)
{
    MMModem *modem = MM_MODEM (self);
    MMModemIceraPrivate *priv = MM_MODEM_ICERA_GET_PRIVATE (self);
    MMCallbackInfo *info;
    MMAtSerialPort *primary;

    mm_modem_set_state (modem, MM_MODEM_STATE_CONNECTING, MM_MODEM_STATE_REASON_NONE);

    info = mm_callback_info_new (modem, callback, user_data);

    primary = mm_generic_gsm_get_at_port (MM_GENERIC_GSM (self), MM_AT_PORT_FLAG_PRIMARY);
    g_assert (primary);

    priv->configure_context_tries = 0;
    configure_context (primary, info, priv->username, priv->password, _get_cid (self));
}

/****************************************************************/


static void
free_dns_array (gpointer data)
{
    g_array_free ((GArray *) data, TRUE);
}

static void
ip4_config_invoke (MMCallbackInfo *info)
{
    MMModemIp4Fn callback = (MMModemIp4Fn) info->callback;

    callback (info->modem,
              GPOINTER_TO_UINT (mm_callback_info_get_data (info, "ip4-address")),
              GPOINTER_TO_UINT (mm_callback_info_get_data (info, "ip4-netmask")),
              GPOINTER_TO_UINT (mm_callback_info_get_data (info, "ip4-gateway")),
              (GArray *) mm_callback_info_get_data (info, "ip4-dns"),
              info->error, info->user_data);
}

#define IPDPADDR_TAG "%IPDPADDR: "

static void
get_ip4_config_done (MMAtSerialPort *port,
                     GString *response,
                     GError *error,
                     gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;
	char **items, **iter;
    GArray *dns_array;
    int i;
    guint32 tmp;
    gint cid;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        info->error = g_error_copy (error);
        goto out;
    } else if (!g_str_has_prefix (response->str, IPDPADDR_TAG)) {
        info->error = g_error_new_literal (MM_MODEM_ERROR, MM_MODEM_ERROR_GENERAL,
                                           "Retrieving failed: invalid response.");
        goto out;
    }

    cid = _get_cid (MM_MODEM_ICERA (info->modem));
    dns_array = g_array_sized_new (FALSE, TRUE, sizeof (guint32), 2);

    /* %IPDPADDR: <cid>,<ip>,<gw>,<dns1>,<dns2>[,<nbns1>,<nbns2>[,<??>,<netmask>,<gw>]]
     *
     * Sierra USB305: %IPDPADDR: 2, 21.93.217.11, 21.93.217.10, 10.177.0.34, 10.161.171.220, 0.0.0.0, 0.0.0.0
     * K3805-Z: %IPDPADDR: 2, 21.93.217.11, 21.93.217.10, 10.177.0.34, 10.161.171.220, 0.0.0.0, 0.0.0.0, 255.0.0.0, 255.255.255.0, 21.93.217.10,
     */
    items = g_strsplit (response->str + strlen (IPDPADDR_TAG), ", ", 0);

    for (iter = items, i = 0; *iter; iter++, i++) {
        if (i == 0) { /* CID */
            long int num;

            errno = 0;
            num = strtol (*iter, NULL, 10);
            if (errno != 0 || num < 0 || (gint) num != cid) {
                info->error = g_error_new (MM_MODEM_ERROR, MM_MODEM_ERROR_GENERAL,
                                           "Unknown CID in IPDPADDR response ("
                                           "got %d, expected %d)", (guint) num, cid);
                break;
            }
        } else if (i == 1) { /* IP address */
            if (inet_pton (AF_INET, *iter, &tmp) > 0)
                mm_callback_info_set_data (info, "ip4-address", GUINT_TO_POINTER (tmp), NULL);
        } else if (i == 2) { /* Gateway */
            if ((inet_pton (AF_INET, *iter, &tmp) > 0) && (tmp > 0))
                mm_callback_info_set_data (info, "ip4-gateway", GUINT_TO_POINTER (tmp), NULL);
        } else if (i == 3) { /* DNS 1 */
            if (inet_pton (AF_INET, *iter, &tmp) > 0)
                g_array_append_val (dns_array, tmp);
        } else if (i == 4) { /* DNS 2 */
            if (inet_pton (AF_INET, *iter, &tmp) > 0)
                g_array_append_val (dns_array, tmp);
        } else if (i == 8) { /* Netmask */
            if (inet_pton (AF_INET, *iter, &tmp) > 0)
                mm_callback_info_set_data (info, "ip4-netmask", GUINT_TO_POINTER (tmp), NULL);
        } else if (i == 9) { /* Duplicate gateway */
            if (mm_callback_info_get_data (info, "ip4-gateway") == NULL) {
                if (inet_pton (AF_INET, *iter, &tmp) > 0)
                    mm_callback_info_set_data (info, "ip4-gateway", GUINT_TO_POINTER (tmp), NULL);
            }
        }
    }

    g_strfreev (items);
    mm_callback_info_set_data (info, "ip4-dns", dns_array, free_dns_array);

 out:
    mm_callback_info_schedule (info);
}

void
mm_modem_icera_get_ip4_config (MMModemIcera *self,
                               MMModemIp4Fn callback,
                               gpointer user_data)
{
    MMCallbackInfo *info;
    char *command;
    MMAtSerialPort *primary;

    info = mm_callback_info_new_full (MM_MODEM (self),
                                      ip4_config_invoke,
                                      G_CALLBACK (callback),
                                      user_data);

    primary = mm_generic_gsm_get_at_port (MM_GENERIC_GSM (self), MM_AT_PORT_FLAG_PRIMARY);
    g_assert (primary);

    command = g_strdup_printf ("%%IPDPADDR=%d", _get_cid (self));
    mm_at_serial_port_queue_command (primary, command, 3, get_ip4_config_done, info);
    g_free (command);
}

static void
invoke_mm_modem_icera_timestamp_fn (MMCallbackInfo *info)
{
    MMModemIceraTimestampFn callback;
    MMModemIceraTimestamp *timestamp;

    callback = (MMModemIceraTimestampFn) info->callback;
    timestamp = (MMModemIceraTimestamp *) mm_callback_info_get_result (info);

    callback (MM_MODEM_ICERA (info->modem),
              timestamp,
              info->error, info->user_data);
}

static MMCallbackInfo *
mm_callback_info_icera_timestamp_new (MMModemIcera *modem,
                                      MMModemIceraTimestampFn callback,
                                      gpointer user_data)
{
    g_return_val_if_fail (modem != NULL, NULL);

    return mm_callback_info_new_full (MM_MODEM (modem),
                                      invoke_mm_modem_icera_timestamp_fn,
				      (GCallback) callback,
				      user_data);
}

static void
get_local_timestamp_done (MMAtSerialPort *port,
                          GString *response,
                          GError *error,
                          gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;
    MMModemIceraTimestamp *timestamp;
    char sign;
    int offset;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        info->error = g_error_copy (error);
        goto out;
    }

    timestamp = g_malloc0 (sizeof (MMModemIceraTimestamp));

    if (g_str_has_prefix (response->str, "*TLTS: ") &&
        sscanf (response->str + 7,
                "\"%02d/%02d/%02d,%02d:%02d:%02d%c%02d\"",
                &timestamp->year,
                &timestamp->month,
                &timestamp->day,
                &timestamp->hour,
                &timestamp->minute,
                &timestamp->second,
                &sign, &offset) == 8) {
        if (sign == '-')
            timestamp->tz_offset = -offset;
        else
            timestamp->tz_offset = offset;
        mm_callback_info_set_result (info, timestamp, g_free);
    } else {
        mm_warn ("Unknown *TLTS response: %s", response->str);
        mm_callback_info_set_result (info, NULL, g_free);
        g_free (timestamp);
    }

 out:
    mm_callback_info_schedule (info);
}

void
mm_modem_icera_get_local_timestamp (MMModemIcera *self,
                                    MMModemIceraTimestampFn callback,
                                    gpointer user_data)
{
    MMCallbackInfo *info;
    MMAtSerialPort *primary;

    info = mm_callback_info_icera_timestamp_new (self, callback, user_data);

    primary = mm_generic_gsm_get_at_port (MM_GENERIC_GSM (self), MM_AT_PORT_FLAG_PRIMARY);
    g_assert (primary);

    mm_at_serial_port_queue_command (primary, "*TLTS", 3, get_local_timestamp_done, info);
}

/****************************************************************/

typedef struct {
    MMModemGsmBand band;
    char *name;
    gboolean enabled;
    gpointer data;
} Band;

static void
band_free (Band *b)
{
    g_free (b->name);
    g_free (b);
}

static const Band modem_bands[] = {
    /* Sort 3G first since it's preferred */
    { MM_MODEM_GSM_BAND_U2100, "FDD_BAND_I",    FALSE, NULL },
    { MM_MODEM_GSM_BAND_U1900, "FDD_BAND_II",   FALSE, NULL },
    { MM_MODEM_GSM_BAND_U1800, "FDD_BAND_III",  FALSE, NULL },
    { MM_MODEM_GSM_BAND_U17IV, "FDD_BAND_IV",   FALSE, NULL },
    { MM_MODEM_GSM_BAND_U800,  "FDD_BAND_VI",   FALSE, NULL },
    { MM_MODEM_GSM_BAND_U850,  "FDD_BAND_V",    FALSE, NULL },
    { MM_MODEM_GSM_BAND_U900,  "FDD_BAND_VIII", FALSE, NULL },
    /* 2G second */
    { MM_MODEM_GSM_BAND_G850,  "G850",          FALSE, NULL },
    { MM_MODEM_GSM_BAND_DCS,   "DCS",           FALSE, NULL },
    { MM_MODEM_GSM_BAND_EGSM,  "EGSM",          FALSE, NULL },
    { MM_MODEM_GSM_BAND_PCS,   "PCS",           FALSE, NULL },
    /* And ANY last since it's most inclusive */
    { MM_MODEM_GSM_BAND_ANY,   "ANY",           FALSE, NULL },
};

static MMModemGsmBand
icera_band_to_mm (const char *icera)
{
    int i;

    for (i = 0 ; i < G_N_ELEMENTS (modem_bands); i++) {
        if (g_strcmp0 (icera, modem_bands[i].name) == 0)
            return modem_bands[i].band;
    }
    return MM_MODEM_GSM_BAND_UNKNOWN;
}

static const char *
mm_band_to_icera (MMModemGsmBand band)
{
    int i;

    for (i = 0; i < G_N_ELEMENTS (modem_bands); i++) {
        if (modem_bands[i].band == band)
            return modem_bands[i].name;
    }
    return NULL;
}

/* returns a list of 'Band' structs */
static GSList *
build_bands_info (const gchar *response, gboolean build_command)
{
    GSList *bands = NULL;
    GRegex *r;
    GMatchInfo *match;

    /*
     * Response is a number of lines of the form:
     *   "EGSM": 0
     *   "FDD_BAND_I": 1
     *   ...
     * with 1 and 0 indicating whether the particular band is enabled or not.
     */
    r = g_regex_new ("^\"(\\w+)\": (\\d)",
                     G_REGEX_MULTILINE, G_REGEX_MATCH_NEWLINE_ANY,
                     NULL);
    g_assert (r != NULL);

    g_regex_match (r, response, 0, &match);
    while (g_match_info_matches (match)) {
        gchar *name, *enabled;
        Band *b;
        MMModemGsmBand band;

        name = g_match_info_fetch (match, 1);
        enabled = g_match_info_fetch (match, 2);
        band = icera_band_to_mm (name);
        if (band != MM_MODEM_GSM_BAND_UNKNOWN && band != MM_MODEM_GSM_BAND_ANY) {
            b = g_malloc0 (sizeof (Band));
            b->band = band;
            b->enabled = (enabled[0] == '1' ? TRUE : FALSE);
            if (build_command) {
                /* abuse 'name' for the AT command to check band support */
                b->name = g_strdup_printf ("%%IPBM=\"%s\",%c",
                                           name,
                                           b->enabled ? '1' : '0');
            }
            bands = g_slist_append (bands, b);
        }
        g_free (name);
        g_free (enabled);
        g_match_info_next (match, NULL);
    }
    g_match_info_free (match);
    g_regex_unref (r);

    return bands;
}

static void
get_current_bands_done (MMAtSerialPort *port,
                        GString *response,
                        GError *error,
                        gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;
    MMModemGsmBand mm_band = MM_MODEM_GSM_BAND_ANY;
    GSList *bands;
    Band *b;
    GSList *iter;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        info->error = g_error_copy (error);
        mm_callback_info_schedule (info);
        return;
    }

    bands = build_bands_info (response->str, FALSE);
    if (!bands) {
        mm_callback_info_set_result (info, GUINT_TO_POINTER (MM_MODEM_GSM_BAND_UNKNOWN), NULL);
        mm_callback_info_schedule (info);
        return;
    }

    for (iter = bands; iter; iter = g_slist_next (iter)) {
        b = iter->data;
        if (b->enabled)
            mm_band |= b->band;
    }
    g_slist_foreach (bands, (GFunc) band_free, NULL);
    g_slist_free (bands);

    mm_callback_info_set_result (info, GUINT_TO_POINTER (mm_band), NULL);
    mm_callback_info_schedule (info);
}

void
mm_modem_icera_get_current_bands (MMModemIcera *self,
                                  MMModemUIntFn callback,
                                  gpointer user_data)
{
    MMAtSerialPort *port;
    MMCallbackInfo *info;

    info = mm_callback_info_uint_new (MM_MODEM (self), callback, user_data);

    /* Otherwise ask the modem */
    port = mm_generic_gsm_get_best_at_port (MM_GENERIC_GSM (self), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    mm_at_serial_port_queue_command (port, "AT%IPBM?", 3, get_current_bands_done, info);
}

#define NUM_BANDS_TAG "num-bands"
#define BAND_RESULT_TAG "band-result"

static void
get_one_supported_band_done (MMAtSerialPort *port,
                             GString *response,
                             GError *error,
                             gpointer user_data)
{
    Band *b = user_data;
    MMCallbackInfo *info = b->data;
    MMModemGsmBand mm_bands = MM_MODEM_GSM_BAND_UNKNOWN;
    guint num;

    if (mm_callback_info_check_modem_removed (info) == FALSE) {
        /* Update supported bands list */
        mm_bands = GPOINTER_TO_UINT (mm_callback_info_get_data (info, BAND_RESULT_TAG));
        if (error == NULL) {
            mm_bands |= b->band;
            mm_callback_info_set_data (info, BAND_RESULT_TAG, GUINT_TO_POINTER (mm_bands), NULL);
        }

        num = GPOINTER_TO_UINT (mm_callback_info_get_data (info, NUM_BANDS_TAG)) - 1;
        mm_callback_info_set_data (info, NUM_BANDS_TAG, GUINT_TO_POINTER (num), NULL);
        if (num == 0) {
            /* All done */
            mm_callback_info_set_result (info, GUINT_TO_POINTER (mm_bands), NULL);
            mm_callback_info_schedule (info);
        }
    }
    band_free (b);
}

static void
get_supported_bands_done (MMAtSerialPort *port,
                          GString *response,
                          GError *error,
                          gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;
    GSList *bands, *iter;
    Band *b;
    guint i;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        info->error = g_error_copy (error);
        mm_callback_info_schedule (info);
        return;
    }

    bands = build_bands_info (response->str, TRUE);
    if (!bands) {
        mm_callback_info_set_result (info, GUINT_TO_POINTER (MM_MODEM_GSM_BAND_UNKNOWN), NULL);
        mm_callback_info_schedule (info);
        return;
    }

    for (iter = bands, i = 0; iter; iter = g_slist_next (iter), i++) {
        b = iter->data;
        b->data = info;
        mm_at_serial_port_queue_command (port, b->name, 10, get_one_supported_band_done, b);
    }
    /* Free list, but not items; they are freed when the AT response comes back */
    g_slist_free (bands);

    mm_callback_info_set_data (info, NUM_BANDS_TAG, GUINT_TO_POINTER (i), NULL);
}

void
mm_modem_icera_get_supported_bands (MMModemIcera *self,
                                    MMModemUIntFn callback,
                                    gpointer user_data)
{
    MMAtSerialPort *port;
    MMCallbackInfo *info;

    info = mm_callback_info_uint_new (MM_MODEM (self), callback, user_data);

    /* Otherwise ask the modem */
    port = mm_generic_gsm_get_best_at_port (MM_GENERIC_GSM (self), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    /* The modems report some bands as disabled that they don't actually
     * support enabling. Thanks Icera! So we have to try setting each
     * band to it's current enabled/disabled value, and the modem will
     * return an error if it doesn't support that band at all.
     */
    mm_at_serial_port_queue_command (port, "AT%IPBM?", 3, get_supported_bands_done, info);
}

static void
set_band_done (MMAtSerialPort *port,
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

void
mm_modem_icera_set_band (MMModemIcera *self,
                         MMModemGsmBand band,
                         MMModemFn callback,
                         gpointer user_data)
{
    MMCallbackInfo *info;
    MMAtSerialPort *port;
    char *command;
    const char *icera_band;

    info = mm_callback_info_new (MM_MODEM (self), callback, user_data);

    port = mm_generic_gsm_get_best_at_port (MM_GENERIC_GSM (self), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    /* TODO: Check how to pass more than one band in the same AT%%IPBM command */
    if (!utils_check_for_single_value (band)) {
        info->error = g_error_new_literal (MM_MODEM_ERROR, MM_MODEM_ERROR_GENERAL, "Cannot set more than one band.");
        mm_callback_info_schedule (info);
        return;
    }

    icera_band = mm_band_to_icera (band);
    if (!icera_band) {
        info->error = g_error_new_literal (MM_MODEM_ERROR, MM_MODEM_ERROR_GENERAL, "Invalid band.");
        mm_callback_info_schedule (info);
        return;
    }

    command = g_strdup_printf ("AT%%IPBM=\"%s\",1", icera_band);
    mm_at_serial_port_queue_command (port, command, 10, set_band_done, info);
    g_free (command);
}

/****************************************************************/

static void
get_unlock_retries_done (MMAtSerialPort *port,
                         GString *response,
                         GError *error,
                         gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;
    int matched;
    GArray *retry_counts;
    PinRetryCount ur[4] = {
        {"sim-pin", 0}, {"sim-puk", 0}, {"sim-pin2", 0}, {"sim-puk2", 0}
    };

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        info->error = g_error_copy (error);
        goto done;
    }

    matched = sscanf (response->str, "%%PINNUM: %d, %d, %d, %d",
                      &ur[0].count, &ur[1].count, &ur[2].count, &ur[3].count);
    if (matched == 4) {
        if (ur[0].count > 998) {
            info->error = g_error_new (MM_MODEM_ERROR, MM_MODEM_ERROR_GENERAL,
                                       "Invalid PIN attempts left %d", ur[0].count);
            ur[0].count = 0;
        }

        retry_counts = g_array_sized_new (FALSE, TRUE, sizeof (PinRetryCount), 4);
        g_array_append_vals (retry_counts, &ur, 4);
        mm_callback_info_set_result (info, retry_counts, NULL);
    } else {
        info->error = g_error_new_literal (MM_MODEM_ERROR, MM_MODEM_ERROR_GENERAL,
                                           "Could not parse PIN retries results");
    }

done:
    mm_serial_port_close (MM_SERIAL_PORT (port));
    mm_callback_info_schedule (info);
}

void
mm_modem_icera_get_unlock_retries (MMModemIcera *self,
                                   MMModemArrayFn callback,
                                   gpointer user_data)
{
    MMAtSerialPort *port;
    MMCallbackInfo *info;

    mm_dbg ("get_unlock_retries");

    info = mm_callback_info_array_new (MM_MODEM (self), callback, user_data);

    /* Ensure we have a usable port to use for the command */
    port = mm_generic_gsm_get_best_at_port (MM_GENERIC_GSM (self), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    /* Modem may not be enabled yet, which sometimes can't be done until
     * the device has been unlocked.  In this case we have to open the port
     * ourselves.
     */
    if (!mm_serial_port_open (MM_SERIAL_PORT (port), &info->error)) {
        mm_callback_info_schedule (info);
        return;
    }

    /* if the modem have not yet been enabled we need to make sure echoing is turned off */
    mm_at_serial_port_queue_command (port, "E0", 3, NULL, NULL);
    mm_at_serial_port_queue_command (port, "%PINNUM?", 3, get_unlock_retries_done, info);

}

/****************************************************************/

static const char *
get_string_property (GHashTable *properties, const char *name)
{
    GValue *value;

    value = (GValue *) g_hash_table_lookup (properties, name);
    if (value && G_VALUE_HOLDS_STRING (value))
        return g_value_get_string (value);
    return NULL;
}

void
mm_modem_icera_simple_connect (MMModemIcera *self, GHashTable *properties)
{
    MMModemIceraPrivate *priv = MM_MODEM_ICERA_GET_PRIVATE (self);

    g_free (priv->username);
    priv->username = g_strdup (get_string_property (properties, "username"));
    g_free (priv->password);
    priv->password = g_strdup (get_string_property (properties, "password"));
}

/****************************************************************/

void
mm_modem_icera_register_unsolicted_handlers (MMModemIcera *self,
                                             MMAtSerialPort *port)
{
    GRegex *regex;

    /* %NWSTATE: <rssi>,<mccmnc>,<tech>,<connection state>,<regulation>
     *
     * <connection state> shows the actual access technology in-use when a
     * PS connection is active.
     */
    regex = g_regex_new ("\\r\\n%NWSTATE:\\s*(-?\\d+),(\\d+),([^,]*),([^,]*),(\\d+)\\r\\n", G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    mm_at_serial_port_add_unsolicited_msg_handler (port, regex, nwstate_changed, self, NULL);
    g_regex_unref (regex);

    regex = g_regex_new ("\\r\\n\\+PACSP(\\d)\\r\\n", G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    mm_at_serial_port_add_unsolicited_msg_handler (port, regex, pacsp_received, self, NULL);
    g_regex_unref (regex);

    /* %IPDPACT: <cid>,<status>,0 */
    regex = g_regex_new ("\\r\\n%IPDPACT:\\s*(\\d+),\\s*(\\d+),\\s*(\\d+)\\r\\n", G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    mm_at_serial_port_add_unsolicited_msg_handler (port, regex, connection_enabled, self, NULL);
    g_regex_unref (regex);
}

void
mm_modem_icera_change_unsolicited_messages (MMModemIcera *self, gboolean enabled)
{
    MMAtSerialPort *primary;

    primary = mm_generic_gsm_get_at_port (MM_GENERIC_GSM (self), MM_AT_PORT_FLAG_PRIMARY);
    g_assert (primary);

    mm_at_serial_port_queue_command (primary, enabled ? "%NWSTATE=1" : "%NWSTATE=0", 3, NULL, NULL);
}

/****************************************************************/

static void
is_icera_done (MMAtSerialPort *port,
               GString *response,
               GError *error,
               gpointer user_data)
{
    MMCallbackInfo *info = user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);
    else
        mm_callback_info_set_result (info, GUINT_TO_POINTER (TRUE), NULL);
    mm_callback_info_schedule (info);
}

void
mm_modem_icera_is_icera (MMModemIcera *self,
                         MMModemUIntFn callback,
                         gpointer user_data)
{
    MMAtSerialPort *port;
    MMCallbackInfo *info;

    info = mm_callback_info_uint_new (MM_MODEM (self), callback, user_data);

    port = mm_generic_gsm_get_best_at_port (MM_GENERIC_GSM (self), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    mm_at_serial_port_queue_command (port, "%IPSYS?", 5, is_icera_done, info);
}

void
mm_modem_icera_cleanup (MMModemIcera *self)
{
    MMModemIceraPrivate *priv = MM_MODEM_ICERA_GET_PRIVATE (self);

    /* Clear the pending connection if necessary */
    connect_pending_done (self);
    cleanup_configure_context (self);

    g_free (priv->username);
    priv->username = NULL;
    g_free (priv->password);
    priv->password = NULL;
}

/****************************************************************/

MMModemIceraPrivate *
mm_modem_icera_init_private (void)
{
    return g_malloc0 (sizeof (MMModemIceraPrivate));
}

void
mm_modem_icera_dispose_private (MMModemIcera *self)
{
    MMModemIceraPrivate *priv = MM_MODEM_ICERA_GET_PRIVATE (self);

    mm_modem_icera_cleanup (self);
    memset (priv, 0, sizeof (*priv));
    g_free (priv);
}

static void
mm_modem_icera_init (gpointer g_iface)
{
}

GType
mm_modem_icera_get_type (void)
{
    static GType icera_type = 0;

    if (!G_UNLIKELY (icera_type)) {
        const GTypeInfo icera_info = {
            sizeof (MMModemIcera), /* class_size */
            mm_modem_icera_init,   /* base_init */
            NULL,       /* base_finalize */
            NULL,
            NULL,       /* class_finalize */
            NULL,       /* class_data */
            0,
            0,              /* n_preallocs */
            NULL
        };

        icera_type = g_type_register_static (G_TYPE_INTERFACE,
                                             "MMModemIcera",
                                             &icera_info, 0);

        g_type_interface_add_prerequisite (icera_type, MM_TYPE_MODEM);
    }

    return icera_type;
}
