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
 * Copyright (C) 2009 - 2011 Red Hat, Inc.
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2015 Azimut Electronics
 * Copyright (C) 2011 - 2016 Aleksander Morgado <aleksander@aleksander.es>
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <ModemManager.h>
#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-daemon-enums-types.h"
#include "mm-iface-modem.h"
#include "mm-iface-modem-3gpp.h"
#include "mm-iface-modem-3gpp-profile-manager.h"
#include "mm-iface-modem-cdma.h"
#include "mm-base-bearer.h"
#include "mm-base-modem.h"
#include "mm-log-object.h"
#include "mm-modem-helpers.h"
#include "mm-error-helpers.h"
#include "mm-bearer-stats.h"
#include "mm-dispatcher-connection.h"
#include "mm-auth-provider.h"
#include "mm-bind.h"

/* We require up to 20s to get a proper IP when using PPP */
#define BEARER_IP_TIMEOUT_DEFAULT 20

#define BEARER_DEFERRED_UNREGISTRATION_TIMEOUT 15

#define BEARER_STATS_UPDATE_TIMEOUT 30

/* Initial connectivity check after 30s, then each 5s */
#define BEARER_CONNECTION_MONITOR_INITIAL_TIMEOUT 30
#define BEARER_CONNECTION_MONITOR_TIMEOUT          5

static void log_object_iface_init (MMLogObjectInterface *iface);
static void bind_iface_init (MMBindInterface *iface);

G_DEFINE_TYPE_EXTENDED (MMBaseBearer, mm_base_bearer, MM_GDBUS_TYPE_BEARER_SKELETON, 0,
                        G_IMPLEMENT_INTERFACE (MM_TYPE_LOG_OBJECT, log_object_iface_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_BIND, bind_iface_init))

typedef enum {
    CONNECTION_FORBIDDEN_REASON_NONE,
    CONNECTION_FORBIDDEN_REASON_UNREGISTERED,
    CONNECTION_FORBIDDEN_REASON_ROAMING,
    CONNECTION_FORBIDDEN_REASON_EMERGENCY_ONLY,
    CONNECTION_FORBIDDEN_REASON_LAST
} ConnectionForbiddenReason;

enum {
    PROP_0,
    PROP_PATH,
    PROP_CONNECTION,
    PROP_BIND_TO,
    PROP_MODEM,
    PROP_STATUS,
    PROP_CONFIG,
    PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

struct _MMBaseBearerPrivate {
    /* The connection to the system bus */
    GDBusConnection *connection;
    guint            dbus_id;

    /* The authorization provider */
    MMAuthProvider *authp;
    GCancellable   *authp_cancellable;

    /* The object this bearer is bound to */
    GObject *bind_to;

    /* The modem which owns this BEARER */
    MMBaseModem *modem;
    /* The path where the BEARER object is exported */
    gchar *path;
    /* Status of this bearer */
    MMBearerStatus status;
    /* Whether we must ignore all disconnection updates if they're
     * detected by ModemManager itself. */
    gboolean ignore_disconnection_reports;
    /* Configuration of the bearer */
    MMBearerProperties *config;

    /* Cancellable for connect() */
    GCancellable *connect_cancellable;
    /* handler id for the disconnect + cancel connect request */
    gulong disconnect_signal_handler;

    /* Connection status monitoring */
    guint connection_monitor_id;
    /* Flag to specify whether connection monitoring is supported or not */
    gboolean load_connection_status_unsupported;

    /*-- 3GPP specific --*/
    guint deferred_3gpp_unregistration_id;
    /* Reason if 3GPP connection is forbidden */
    ConnectionForbiddenReason reason_3gpp;
    /* Handler ID for the registration state change signals */
    guint id_3gpp_registration_change;

    /*-- CDMA specific --*/
    guint deferred_cdma_unregistration_id;
    /* Reason if CDMA connection is forbidden */
    ConnectionForbiddenReason reason_cdma;
    /* Handler IDs for the registration state change signals */
    guint id_cdma1x_registration_change;
    guint id_evdo_registration_change;

    /* The stats object to expose */
    MMBearerStats *stats;
    /* Handler id for the stats update timeout */
    guint stats_update_id;
    /* Timer to measure the duration of the connection */
    GTimer *duration_timer;
    /* Flag to specify whether reloading stats is supported or not */
    gboolean reload_stats_supported;
};

/*****************************************************************************/

static const gchar *connection_forbidden_reason_str [CONNECTION_FORBIDDEN_REASON_LAST] = {
    "none",
    "Not registered in the network",
    "Registered in roaming network, and roaming not allowed",
    "Emergency services only",
};

/*****************************************************************************/

void
mm_base_bearer_export (MMBaseBearer *self)
{
    gchar *path;

    path = g_strdup_printf (MM_DBUS_BEARER_PREFIX "/%d", self->priv->dbus_id);
    g_object_set (self,
                  MM_BASE_BEARER_PATH, path,
                  NULL);
    g_free (path);
}

/*****************************************************************************/

static void
connection_monitor_stop (MMBaseBearer *self)
{
    if (self->priv->connection_monitor_id) {
        g_source_remove (self->priv->connection_monitor_id);
        self->priv->connection_monitor_id = 0;
    }
}

static void
load_connection_status_ready (MMBaseBearer *self,
                              GAsyncResult *res)
{
    GError                   *error = NULL;
    MMBearerConnectionStatus  status;

    status = MM_BASE_BEARER_GET_CLASS (self)->load_connection_status_finish (self, res, &error);
    if (status == MM_BEARER_CONNECTION_STATUS_UNKNOWN) {
        /* Only warn if not reporting an "unsupported" error */
        if (!g_error_matches (error, MM_CORE_ERROR, MM_CORE_ERROR_UNSUPPORTED)) {
            mm_obj_warn (self, "checking if connected failed: %s", error->message);
            g_error_free (error);
            return;
        }

        /* If we're being told that connection monitoring is unsupported, just
         * ignore the error and remove the timeout. */
        mm_obj_dbg (self, "connection monitoring is unsupported by the device");
        self->priv->load_connection_status_unsupported = TRUE;
        connection_monitor_stop (self);
        g_error_free (error);
        return;
    }

    /* Report connection or disconnection */
    g_assert (status == MM_BEARER_CONNECTION_STATUS_CONNECTED || status == MM_BEARER_CONNECTION_STATUS_DISCONNECTED);
    mm_obj_dbg (self, "connection status loaded: %s", mm_bearer_connection_status_get_string (status));
    mm_base_bearer_report_connection_status (self, status);
}

static gboolean
connection_monitor_cb (MMBaseBearer *self)
{
    /* If the implementation knows how to load connection status, run it */
    if (self->priv->status == MM_BEARER_STATUS_CONNECTED)
        MM_BASE_BEARER_GET_CLASS (self)->load_connection_status (
            self,
            (GAsyncReadyCallback)load_connection_status_ready,
            NULL);
    return G_SOURCE_CONTINUE;
}

static gboolean
initial_connection_monitor_cb (MMBaseBearer *self)
{
    if (self->priv->status == MM_BEARER_STATUS_CONNECTED)
        MM_BASE_BEARER_GET_CLASS (self)->load_connection_status (
            self,
            (GAsyncReadyCallback)load_connection_status_ready,
            NULL);

    /* Add new monitor timeout at a higher rate */
    self->priv->connection_monitor_id = g_timeout_add_seconds (BEARER_CONNECTION_MONITOR_TIMEOUT,
                                                               (GSourceFunc) connection_monitor_cb,
                                                               self);

    /* Remove the initial connection monitor timeout as we added a new one */
    return G_SOURCE_REMOVE;
}

static void
connection_monitor_start (MMBaseBearer *self)
{
    /* If not implemented, don't schedule anything */
    if (!MM_BASE_BEARER_GET_CLASS (self)->load_connection_status ||
        !MM_BASE_BEARER_GET_CLASS (self)->load_connection_status_finish)
        return;

    if (self->priv->load_connection_status_unsupported)
        return;

    /* Schedule initial check */
    g_assert (!self->priv->connection_monitor_id);
    self->priv->connection_monitor_id = g_timeout_add_seconds (BEARER_CONNECTION_MONITOR_INITIAL_TIMEOUT,
                                                               (GSourceFunc) initial_connection_monitor_cb,
                                                               self);
}

/*****************************************************************************/

static void
bearer_update_connection_error (MMBaseBearer *self,
                                const GError *connection_error)
{
    g_autoptr(GVariant) tuple = NULL;

    if (connection_error) {
        g_autoptr(GError) normalized_error = NULL;

        /* Never overwrite a connection error if it's already set */
        tuple = mm_gdbus_bearer_dup_connection_error (MM_GDBUS_BEARER (self));
        if (tuple)
            return;

        /*
         * Limit the type of errors we can expose in the interface;
         * e.g. we don't want QMI or MBIM specific errors reported.
         */
        normalized_error = mm_normalize_error (connection_error);
        tuple = mm_common_error_to_tuple (normalized_error);
    }
    mm_gdbus_bearer_set_connection_error (MM_GDBUS_BEARER (self), tuple);
}

/*****************************************************************************/

static void
bearer_update_interface_stats (MMBaseBearer *self)
{
    mm_gdbus_bearer_set_stats (
        MM_GDBUS_BEARER (self),
        mm_bearer_stats_get_dictionary (self->priv->stats));
}

static void
bearer_reset_ongoing_interface_stats (MMBaseBearer *self)
{
    mm_bearer_stats_set_duration (self->priv->stats, 0);
    mm_bearer_stats_set_tx_bytes (self->priv->stats, 0);
    mm_bearer_stats_set_rx_bytes (self->priv->stats, 0);
    mm_bearer_stats_set_start_date (self->priv->stats, 0);
    mm_bearer_stats_set_uplink_speed (self->priv->stats, 0);
    mm_bearer_stats_set_downlink_speed (self->priv->stats, 0);
    bearer_update_interface_stats (self);
}

static void
bearer_set_ongoing_interface_stats (MMBaseBearer *self,
                                    guint         duration,
                                    guint64       rx_bytes,
                                    guint64       tx_bytes)
{
    guint n_updates = 0;

    /* Make sure we don't reset to 0 these values if we had ever set them
     * before. Just ignore the update if we're reported 0 */

    if (duration) {
        gint delta_duration;

        delta_duration = duration - mm_bearer_stats_get_duration (self->priv->stats);
        if (delta_duration > 0) {
            mm_bearer_stats_set_duration (self->priv->stats, duration);
            mm_bearer_stats_set_total_duration (self->priv->stats,
                                                mm_bearer_stats_get_total_duration (self->priv->stats) + delta_duration);
            n_updates++;
        }
    }

    if (rx_bytes) {
        gint64 delta_rx_bytes;

        delta_rx_bytes = rx_bytes - mm_bearer_stats_get_rx_bytes (self->priv->stats);
        if (delta_rx_bytes > 0) {
            mm_bearer_stats_set_rx_bytes (self->priv->stats, rx_bytes);
            mm_bearer_stats_set_total_rx_bytes (self->priv->stats,
                                                mm_bearer_stats_get_total_rx_bytes (self->priv->stats) + delta_rx_bytes);
            n_updates++;
        }
    }

    if (tx_bytes) {
        gint64 delta_tx_bytes;

        delta_tx_bytes = tx_bytes - mm_bearer_stats_get_tx_bytes (self->priv->stats);
        if (delta_tx_bytes > 0) {
            mm_bearer_stats_set_tx_bytes (self->priv->stats, tx_bytes);
            mm_bearer_stats_set_total_tx_bytes (self->priv->stats,
                                                mm_bearer_stats_get_total_tx_bytes (self->priv->stats) + delta_tx_bytes);
            n_updates++;
        }
    }

    if (n_updates)
        bearer_update_interface_stats (self);
}

static void
bearer_stats_stop (MMBaseBearer *self)
{
    if (self->priv->duration_timer) {
        bearer_set_ongoing_interface_stats (self,
                                            (guint64) g_timer_elapsed (self->priv->duration_timer, NULL),
                                            0,
                                            0);
        g_timer_destroy (self->priv->duration_timer);
        self->priv->duration_timer = NULL;
    }

    if (self->priv->stats_update_id) {
        g_source_remove (self->priv->stats_update_id);
        self->priv->stats_update_id = 0;
    }
}

static void
reload_stats_ready (MMBaseBearer *self,
                    GAsyncResult *res)
{
    GError  *error = NULL;
    guint64  rx_bytes = 0;
    guint64  tx_bytes = 0;

    if (!MM_BASE_BEARER_GET_CLASS (self)->reload_stats_finish (self, &rx_bytes, &tx_bytes, res, &error)) {
        mm_obj_warn (self, "reloading stats failed: %s", error->message);
        g_error_free (error);
        return;
    }

    /* We only update stats if they were retrieved properly */
    bearer_set_ongoing_interface_stats (self,
                                        (guint32) g_timer_elapsed (self->priv->duration_timer, NULL),
                                        rx_bytes,
                                        tx_bytes);
}

static gboolean
stats_update_cb (MMBaseBearer *self)
{
    /* Ignore stats update if we're not connected */
    if (self->priv->status != MM_BEARER_STATUS_CONNECTED)
        return G_SOURCE_CONTINUE;

    /* If the implementation knows how to update stat values, run it */
    if (self->priv->reload_stats_supported) {
        MM_BASE_BEARER_GET_CLASS (self)->reload_stats (
            self,
            (GAsyncReadyCallback)reload_stats_ready,
            NULL);
        return G_SOURCE_CONTINUE;
    }

    /* Otherwise, just update duration and we're done */
    bearer_set_ongoing_interface_stats (self,
                                        (guint32) g_timer_elapsed (self->priv->duration_timer, NULL),
                                        0,
                                        0);
    return G_SOURCE_CONTINUE;
}

static void
bearer_stats_start (MMBaseBearer *self,
                    guint64       uplink_speed,
                    guint64       downlink_speed)
{
    /* Start duration timer */
    g_assert (!self->priv->duration_timer);
    self->priv->duration_timer = g_timer_new ();

    /* Schedule */
    g_assert (!self->priv->stats_update_id);
    self->priv->stats_update_id = g_timeout_add_seconds (BEARER_STATS_UPDATE_TIMEOUT,
                                                         (GSourceFunc) stats_update_cb,
                                                         self);

    mm_bearer_stats_set_start_date (self->priv->stats, (guint64)(g_get_real_time() / G_USEC_PER_SEC));
    mm_bearer_stats_set_uplink_speed (self->priv->stats, uplink_speed);
    mm_bearer_stats_set_downlink_speed (self->priv->stats, downlink_speed);
    bearer_update_interface_stats (self);

    /* Load initial values */
    stats_update_cb (self);
}

/*****************************************************************************/

void
mm_base_bearer_report_speeds (MMBaseBearer *self,
                              guint64       uplink_speed,
                              guint64       downlink_speed)
{
    /* Ignore speeds update if we're not connected */
    if (self->priv->status != MM_BEARER_STATUS_CONNECTED)
        return;
    mm_bearer_stats_set_uplink_speed (self->priv->stats, uplink_speed);
    mm_bearer_stats_set_downlink_speed (self->priv->stats, downlink_speed);
    bearer_update_interface_stats (self);
}

/*****************************************************************************/

static void
dispatcher_connection_run_ready (MMDispatcherConnection *dispatcher,
                                 GAsyncResult           *res,
                                 MMBaseBearer           *self)
{
    g_autoptr(GError) error = NULL;

    if (!mm_dispatcher_connection_run_finish (dispatcher, res, &error))
        mm_obj_warn (self, "errors detected in dispatcher: %s", error->message);

    g_object_unref (self);
}

static void
bearer_run_dispatcher_scripts (MMBaseBearer *self,
                               MMDispatcherConnectionEvent event)
{
    MMDispatcherConnection *dispatcher;
    const gchar *interface;

    interface = mm_gdbus_bearer_get_interface (MM_GDBUS_BEARER (self));
    if (!self->priv->modem || !self->priv->path || !interface)
        return;

    dispatcher = mm_dispatcher_connection_get ();
    mm_dispatcher_connection_run (dispatcher,
                                  g_dbus_object_get_object_path (G_DBUS_OBJECT (self->priv->modem)),
                                  self->priv->path,
                                  interface,
                                  event,
                                  NULL, /* cancellable */
                                  (GAsyncReadyCallback)dispatcher_connection_run_ready,
                                  g_object_ref (self));
}

/*****************************************************************************/

static void
bearer_reset_interface_status (MMBaseBearer *self)
{
    mm_gdbus_bearer_set_profile_id (MM_GDBUS_BEARER (self), MM_3GPP_PROFILE_ID_UNKNOWN);
    mm_gdbus_bearer_set_multiplexed (MM_GDBUS_BEARER (self), FALSE);
    mm_gdbus_bearer_set_connected (MM_GDBUS_BEARER (self), FALSE);
    mm_gdbus_bearer_set_disconnect_request (MM_GDBUS_BEARER (self), FALSE);
    mm_gdbus_bearer_set_suspended (MM_GDBUS_BEARER (self), FALSE);
    mm_gdbus_bearer_set_interface (MM_GDBUS_BEARER (self), NULL);
    mm_gdbus_bearer_set_ip4_config (
        MM_GDBUS_BEARER (self),
        mm_bearer_ip_config_get_dictionary (NULL));
    mm_gdbus_bearer_set_ip6_config (
        MM_GDBUS_BEARER (self),
        mm_bearer_ip_config_get_dictionary (NULL));
}

static void
bearer_update_status (MMBaseBearer *self,
                      MMBearerStatus status)
{
    /* NOTE: we do allow status 'CONNECTED' here; it may happen if we go into
     * DISCONNECTING and we cannot disconnect */

    /* Do nothing if the status is the same */
    if (self->priv->status == status)
        return;

    /* Update the property value */
    self->priv->status = status;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATUS]);

    /* Ensure that we don't expose any connection related data in the
     * interface when going into disconnected state. */
    if (self->priv->status == MM_BEARER_STATUS_DISCONNECTED) {
        g_autoptr(GString) report = NULL;

        /* Report disconnection via dispatcher scripts, before resetting the interface */
        bearer_run_dispatcher_scripts (self, MM_DISPATCHER_CONNECTION_EVENT_DISCONNECTED);

        bearer_reset_interface_status (self);
        /* Cleanup flag to ignore disconnection reports */
        self->priv->ignore_disconnection_reports = FALSE;
        /* Stop statistics */
        bearer_stats_stop (self);
        /* Stop connection monitoring */
        connection_monitor_stop (self);

        /* Build and log report */
        report = g_string_new (NULL);
        g_string_append_printf (report,
                                "connection #%u finished: duration %us",
                                mm_bearer_stats_get_attempts (self->priv->stats),
                                mm_bearer_stats_get_duration (self->priv->stats));
        if (self->priv->reload_stats_supported)
            g_string_append_printf (report,
                                    ", tx: %" G_GUINT64_FORMAT " bytes, rx: %" G_GUINT64_FORMAT " bytes",
                                    mm_bearer_stats_get_tx_bytes (self->priv->stats),
                                    mm_bearer_stats_get_rx_bytes (self->priv->stats));
        mm_obj_msg (self, "%s", report->str);
    }
}

static void
bearer_update_status_connected (MMBaseBearer     *self,
                                const gchar      *interface,
                                gboolean          multiplexed,
                                gint              profile_id,
                                MMBearerIpConfig *ipv4_config,
                                MMBearerIpConfig *ipv6_config,
                                guint64           uplink_speed,
                                guint64           downlink_speed)
{
    mm_gdbus_bearer_set_profile_id (MM_GDBUS_BEARER (self), profile_id);
    mm_gdbus_bearer_set_multiplexed (MM_GDBUS_BEARER (self), multiplexed);
    mm_gdbus_bearer_set_connected (MM_GDBUS_BEARER (self), TRUE);
    mm_gdbus_bearer_set_suspended (MM_GDBUS_BEARER (self), FALSE);
    mm_gdbus_bearer_set_interface (MM_GDBUS_BEARER (self), interface);
    mm_gdbus_bearer_set_ip4_config (
        MM_GDBUS_BEARER (self),
        mm_bearer_ip_config_get_dictionary (ipv4_config));
    mm_gdbus_bearer_set_ip6_config (
        MM_GDBUS_BEARER (self),
        mm_bearer_ip_config_get_dictionary (ipv6_config));

    /* If PPP is involved in the requested IP config, we must ignore
     * all disconnection reports found via CGACT? polling or CGEV URCs.
     * In this case, upper layers should always explicitly disconnect
     * the bearer when ownership of the TTY is given back to MM. */
    if ((ipv4_config && mm_bearer_ip_config_get_method (ipv4_config) == MM_BEARER_IP_METHOD_PPP) ||
        (ipv6_config && mm_bearer_ip_config_get_method (ipv6_config) == MM_BEARER_IP_METHOD_PPP)) {
        mm_obj_dbg (self, "PPP is required for connection, will ignore disconnection reports");
        self->priv->ignore_disconnection_reports = TRUE;
    }

    /* Update the property value */
    self->priv->status = MM_BEARER_STATUS_CONNECTED;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATUS]);

    /* Start statistics */
    bearer_stats_start (self, uplink_speed, downlink_speed);

    /* Start connection monitor, if supported */
    connection_monitor_start (self);

    /* Run dispatcher scripts */
    bearer_run_dispatcher_scripts (self, MM_DISPATCHER_CONNECTION_EVENT_CONNECTED);
}

/*****************************************************************************/

static void
reset_deferred_unregistration (MMBaseBearer *self)
{
    if (self->priv->deferred_cdma_unregistration_id) {
        g_source_remove (self->priv->deferred_cdma_unregistration_id);
        self->priv->deferred_cdma_unregistration_id = 0;
    }

    if (self->priv->deferred_3gpp_unregistration_id) {
        g_source_remove (self->priv->deferred_3gpp_unregistration_id);
        self->priv->deferred_3gpp_unregistration_id = 0;
    }
}

static gboolean
deferred_3gpp_unregistration_cb (MMBaseBearer *self)
{
    g_warn_if_fail (self->priv->reason_3gpp == CONNECTION_FORBIDDEN_REASON_UNREGISTERED);
    self->priv->deferred_3gpp_unregistration_id = 0;

    mm_obj_dbg (self, "forcing bearer disconnection, not registered in 3GPP network");
    mm_base_bearer_disconnect_force (self);
    return G_SOURCE_REMOVE;
}

static void
modem_3gpp_registration_state_changed (MMIfaceModem3gpp *modem,
                                       GParamSpec *pspec,
                                       MMBaseBearer *self)
{
    MMModem3gppRegistrationState state = MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;

    g_object_get (modem,
                  MM_IFACE_MODEM_3GPP_REGISTRATION_STATE, &state,
                  NULL);

    switch (state) {
    case MM_MODEM_3GPP_REGISTRATION_STATE_IDLE:
    case MM_MODEM_3GPP_REGISTRATION_STATE_DENIED:
    case MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN:
        self->priv->reason_3gpp = CONNECTION_FORBIDDEN_REASON_UNREGISTERED;
        break;
    case MM_MODEM_3GPP_REGISTRATION_STATE_HOME:
    case MM_MODEM_3GPP_REGISTRATION_STATE_HOME_SMS_ONLY:
    case MM_MODEM_3GPP_REGISTRATION_STATE_HOME_CSFB_NOT_PREFERRED:
    case MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING:
    case MM_MODEM_3GPP_REGISTRATION_STATE_ATTACHED_RLOS:
        self->priv->reason_3gpp = CONNECTION_FORBIDDEN_REASON_NONE;
        break;
    case MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING:
    case MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING_SMS_ONLY:
    case MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING_CSFB_NOT_PREFERRED:
        if (mm_bearer_properties_get_allow_roaming (mm_base_bearer_peek_config (self)))
            self->priv->reason_3gpp = CONNECTION_FORBIDDEN_REASON_NONE;
        else
            self->priv->reason_3gpp = CONNECTION_FORBIDDEN_REASON_ROAMING;
        break;
    case MM_MODEM_3GPP_REGISTRATION_STATE_EMERGENCY_ONLY:
        self->priv->reason_3gpp = CONNECTION_FORBIDDEN_REASON_EMERGENCY_ONLY;
        break;
    default:
        g_assert_not_reached ();
    }

    /* If no reason to disconnect, or if it's a mixed CDMA+LTE modem without a CDMA reason,
     * just don't do anything. */
    if (self->priv->reason_3gpp == CONNECTION_FORBIDDEN_REASON_NONE ||
        (mm_iface_modem_is_cdma (MM_IFACE_MODEM (modem)) &&
         self->priv->reason_cdma == CONNECTION_FORBIDDEN_REASON_NONE)) {
        reset_deferred_unregistration (self);
        return;
    }

    /* Modem is roaming and roaming not allowed, report right away */
    if (self->priv->reason_3gpp == CONNECTION_FORBIDDEN_REASON_ROAMING) {
        mm_obj_dbg (self, "bearer not allowed to connect, registered in roaming 3GPP network");
        reset_deferred_unregistration (self);
        mm_base_bearer_disconnect_force (self);
        return;
    }

    /* Modem is registered under emergency services only? */
    if (self->priv->reason_3gpp == CONNECTION_FORBIDDEN_REASON_EMERGENCY_ONLY) {
        mm_obj_dbg (self, "bearer not allowed to connect, emergency services only");
        reset_deferred_unregistration (self);
        mm_base_bearer_disconnect_force (self);
        return;
    }

    /* Modem reports being unregistered */
    if (self->priv->reason_3gpp == CONNECTION_FORBIDDEN_REASON_UNREGISTERED) {
        /* If there is already a notification pending, just return */
        if (self->priv->deferred_3gpp_unregistration_id)
            return;

        /* If the bearer is not connected, report right away */
        if (self->priv->status != MM_BEARER_STATUS_CONNECTED) {
            mm_obj_dbg (self, "bearer not allowed to connect, not registered in 3GPP network");
            mm_base_bearer_disconnect_force (self);
            return;
        }

        /* Otherwise, setup the new timeout */
        mm_obj_dbg (self, "connected bearer not registered in 3GPP network");
        self->priv->deferred_3gpp_unregistration_id =
            g_timeout_add_seconds (BEARER_DEFERRED_UNREGISTRATION_TIMEOUT,
                                   (GSourceFunc) deferred_3gpp_unregistration_cb,
                                   self);
        return;
    }

    g_assert_not_reached ();
}

static gboolean
deferred_cdma_unregistration_cb (MMBaseBearer *self)
{
    g_warn_if_fail (self->priv->reason_cdma == CONNECTION_FORBIDDEN_REASON_UNREGISTERED);
    self->priv->deferred_cdma_unregistration_id = 0;

    mm_obj_dbg (self, "forcing bearer disconnection, not registered in CDMA network");
    mm_base_bearer_disconnect_force (self);
    return G_SOURCE_REMOVE;
}

static void
modem_cdma_registration_state_changed (MMIfaceModemCdma *modem,
                                       GParamSpec *pspec,
                                       MMBaseBearer *self)
{
    MMModemCdmaRegistrationState cdma1x_state = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
    MMModemCdmaRegistrationState evdo_state = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;

    g_object_get (modem,
                  MM_IFACE_MODEM_CDMA_CDMA1X_REGISTRATION_STATE, &cdma1x_state,
                  MM_IFACE_MODEM_CDMA_EVDO_REGISTRATION_STATE, &evdo_state,
                  NULL);

    if (cdma1x_state == MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING ||
        evdo_state == MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING) {
        if (mm_bearer_properties_get_allow_roaming (mm_base_bearer_peek_config (self)))
            self->priv->reason_cdma = CONNECTION_FORBIDDEN_REASON_NONE;
        else
            self->priv->reason_cdma = CONNECTION_FORBIDDEN_REASON_ROAMING;
    } else if (cdma1x_state != MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN ||
               evdo_state != MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN) {
        self->priv->reason_cdma = CONNECTION_FORBIDDEN_REASON_NONE;
    } else {
        self->priv->reason_cdma = CONNECTION_FORBIDDEN_REASON_UNREGISTERED;
    }

    /* If no reason to disconnect, or if it's a mixed CDMA+LTE modem without a 3GPP reason,
     * just don't do anything. */
    if (self->priv->reason_cdma == CONNECTION_FORBIDDEN_REASON_NONE ||
        (mm_iface_modem_is_3gpp (MM_IFACE_MODEM (modem)) &&
         self->priv->reason_3gpp == CONNECTION_FORBIDDEN_REASON_NONE)) {
        reset_deferred_unregistration (self);
        return;
    }

    /* Modem is roaming and roaming not allowed, report right away */
    if (self->priv->reason_cdma == CONNECTION_FORBIDDEN_REASON_ROAMING) {
        mm_obj_dbg (self, "bearer not allowed to connect, registered in roaming CDMA network");
        reset_deferred_unregistration (self);
        mm_base_bearer_disconnect_force (self);
        return;
    }

    /* Modem reports being unregistered */
    if (self->priv->reason_cdma == CONNECTION_FORBIDDEN_REASON_UNREGISTERED) {
        /* If there is already a notification pending, just return */
        if (self->priv->deferred_cdma_unregistration_id)
            return;

        /* If the bearer is not connected, report right away */
        if (self->priv->status != MM_BEARER_STATUS_CONNECTED) {
            mm_obj_dbg (self, "bearer not allowed to connect, not registered in CDMA network");
            mm_base_bearer_disconnect_force (self);
            return;
        }

        /* Otherwise, setup the new timeout */
        mm_obj_dbg (self, "connected bearer not registered in CDMA network");
        self->priv->deferred_cdma_unregistration_id =
            g_timeout_add_seconds (BEARER_DEFERRED_UNREGISTRATION_TIMEOUT,
                                   (GSourceFunc) deferred_cdma_unregistration_cb,
                                   self);
        return;
    }

    g_assert_not_reached ();
}

static void
set_signal_handlers (MMBaseBearer *self)
{
    g_assert (self->priv->modem != NULL);
    g_assert (self->priv->config != NULL);

    /* Don't set the 3GPP registration change signal handlers if they
     * are already set. */
    if (mm_iface_modem_is_3gpp (MM_IFACE_MODEM (self->priv->modem)) &&
        !self->priv->id_3gpp_registration_change) {
        self->priv->id_3gpp_registration_change =
            g_signal_connect (self->priv->modem,
                              "notify::" MM_IFACE_MODEM_3GPP_REGISTRATION_STATE,
                              G_CALLBACK (modem_3gpp_registration_state_changed),
                              self);
        modem_3gpp_registration_state_changed (MM_IFACE_MODEM_3GPP (self->priv->modem), NULL, self);
    }

    /* Don't set the CDMA1x/EV-DO registration change signal handlers if they
     * are already set. */
    if (mm_iface_modem_is_cdma (MM_IFACE_MODEM (self->priv->modem)) &&
        !self->priv->id_cdma1x_registration_change &&
        !self->priv->id_evdo_registration_change) {
        self->priv->id_cdma1x_registration_change =
            g_signal_connect (self->priv->modem,
                              "notify::" MM_IFACE_MODEM_CDMA_CDMA1X_REGISTRATION_STATE,
                              G_CALLBACK (modem_cdma_registration_state_changed),
                              self);
        self->priv->id_evdo_registration_change =
            g_signal_connect (self->priv->modem,
                              "notify::" MM_IFACE_MODEM_CDMA_EVDO_REGISTRATION_STATE,
                              G_CALLBACK (modem_cdma_registration_state_changed),
                              self);
        modem_cdma_registration_state_changed (MM_IFACE_MODEM_CDMA (self->priv->modem), NULL, self);
    }
}

static void
reset_signal_handlers (MMBaseBearer *self)
{
    if (!self->priv->modem)
        return;

    if (self->priv->id_3gpp_registration_change) {
        if (g_signal_handler_is_connected (self->priv->modem, self->priv->id_3gpp_registration_change))
            g_signal_handler_disconnect (self->priv->modem, self->priv->id_3gpp_registration_change);
        self->priv->id_3gpp_registration_change = 0;
    }
    if (self->priv->id_cdma1x_registration_change) {
        if (g_signal_handler_is_connected (self->priv->modem, self->priv->id_cdma1x_registration_change))
            g_signal_handler_disconnect (self->priv->modem, self->priv->id_cdma1x_registration_change);
        self->priv->id_cdma1x_registration_change = 0;
    }
    if (self->priv->id_evdo_registration_change) {
        if (g_signal_handler_is_connected (self->priv->modem, self->priv->id_evdo_registration_change))
            g_signal_handler_disconnect (self->priv->modem, self->priv->id_evdo_registration_change);
        self->priv->id_evdo_registration_change = 0;
    }
}

/*****************************************************************************/
/* CONNECT */

gboolean
mm_base_bearer_connect_finish (MMBaseBearer  *self,
                               GAsyncResult  *res,
                               GError       **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
connect_succeeded (MMBaseBearer *self,
                   GTask        *task)
{
    MMBearerConnectResult *result;

    result = g_task_get_task_data (task);

    /* Update bearer and interface status */
    bearer_update_status_connected (
        self,
        mm_port_get_device (mm_bearer_connect_result_peek_data (result)),
        mm_bearer_connect_result_get_multiplexed (result),
        mm_bearer_connect_result_get_profile_id (result),
        mm_bearer_connect_result_peek_ipv4_config (result),
        mm_bearer_connect_result_peek_ipv6_config (result),
        mm_bearer_connect_result_get_uplink_speed (result),
        mm_bearer_connect_result_get_downlink_speed (result));

    g_clear_object (&self->priv->connect_cancellable);

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
disconnect_after_cancel_ready (MMBaseBearer *self,
                               GAsyncResult *res)
{
    g_autoptr(GError) error = NULL;

    if (!MM_BASE_BEARER_GET_CLASS (self)->disconnect_finish (self, res, &error))
        mm_obj_warn (self, "error disconnecting: %s; will assume disconnected anyway", error->message);
    else
        mm_obj_dbg (self, "disconnected bearer '%s'", self->priv->path);

    /* Report disconnection to the bearer object using class method
     * mm_bearer_report_connection_status. This gives subclass implementations a
     * chance to correctly update their own connection state, in case this base
     * class ignores a failed disconnection attempt.
     */
    mm_base_bearer_report_connection_status (self, MM_BEARER_CONNECTION_STATUS_DISCONNECTED);
}

static void
connect_failed (MMBaseBearer *self,
                GTask        *task,
                GError       *error)
{
    /* Update failed attempts */
    mm_bearer_stats_set_failed_attempts (self->priv->stats,
                                         mm_bearer_stats_get_failed_attempts (self->priv->stats) + 1);
    bearer_update_interface_stats (self);

    /* Update reported connection error before the status update */
    bearer_update_connection_error (self, error);
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        bearer_update_status (self, MM_BEARER_STATUS_DISCONNECTING);
        MM_BASE_BEARER_GET_CLASS (self)->disconnect (self,
                                                     (GAsyncReadyCallback)disconnect_after_cancel_ready,
                                                     NULL);
    } else
        bearer_update_status (self, MM_BEARER_STATUS_DISCONNECTED);

    g_clear_object (&self->priv->connect_cancellable);

    g_task_return_error (task, error);
    g_object_unref (task);
}

static gboolean
connect_check_cancel (MMBaseBearer *self,
                      GTask        *task)
{
    GError *error = NULL;

    if (!g_cancellable_is_cancelled (self->priv->connect_cancellable))
        return FALSE;

    mm_obj_dbg (self, "connected, but need to disconnect");
    error = g_error_new (G_IO_ERROR, G_IO_ERROR_CANCELLED,
                         "Bearer got connected, but had to disconnect after cancellation request");
    connect_failed (self, task, error);
    return TRUE;
}

static void
reload_stats_supported_ready (MMBaseBearer *self,
                              GAsyncResult *res,
                              GTask        *task)
{
    if (MM_BASE_BEARER_GET_CLASS (self)->reload_stats_finish (self, NULL, NULL, res, NULL)) {
        mm_obj_dbg (self, "reloading stats is supported by the device");
        self->priv->reload_stats_supported = TRUE;
        mm_gdbus_bearer_set_reload_stats_supported (MM_GDBUS_BEARER (self), self->priv->reload_stats_supported);
    } else
        mm_obj_dbg (self, "reloading stats is not supported by the device");

    if (connect_check_cancel (self, task))
        return;

    connect_succeeded (self, task);
}

static void
connect_ready (MMBaseBearer *self,
               GAsyncResult *res,
               GTask        *task)
{
    GError                           *error = NULL;
    g_autoptr(MMBearerConnectResult)  result = NULL;

    /* NOTE: connect() implementations *MUST* handle cancellations themselves */
    result = MM_BASE_BEARER_GET_CLASS (self)->connect_finish (self, res, &error);
    if (!result) {
        mm_obj_warn (self, "connection attempt #%u failed: %s",
                     mm_bearer_stats_get_attempts (self->priv->stats),
                     error->message);
        /* process profile manager updates right away on error */
        if (mm_iface_modem_is_3gpp (MM_IFACE_MODEM (self->priv->modem)))
            mm_iface_modem_3gpp_profile_manager_update_ignore_stop (MM_IFACE_MODEM_3GPP_PROFILE_MANAGER (self->priv->modem));
        connect_failed (self, task, error);
        return;
    }

    /* delay processing profile manager updates on success */
    if (mm_iface_modem_is_3gpp (MM_IFACE_MODEM (self->priv->modem)))
        mm_iface_modem_3gpp_profile_manager_update_ignore_stop_delayed (MM_IFACE_MODEM_3GPP_PROFILE_MANAGER (self->priv->modem));

    /* Handle cancellations detected after successful connection */
    if (connect_check_cancel (self, task))
        return;

    mm_obj_dbg (self, "connected");
    g_task_set_task_data (task, g_steal_pointer (&result), (GDestroyNotify)mm_bearer_connect_result_unref);

    /* Check that reload statistics is supported by the device; we can only do this while
     * connected. */
    if (MM_BASE_BEARER_GET_CLASS (self)->reload_stats &&
        MM_BASE_BEARER_GET_CLASS (self)->reload_stats_finish) {
        MM_BASE_BEARER_GET_CLASS (self)->reload_stats (
            self,
            (GAsyncReadyCallback)reload_stats_supported_ready,
            task);
        return;
    }

    connect_succeeded (self, task);
}

void
mm_base_bearer_connect (MMBaseBearer *self,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
    GTask *task;

    if (!MM_BASE_BEARER_GET_CLASS (self)->connect) {
        g_assert (!MM_BASE_BEARER_GET_CLASS (self)->connect_finish);
        g_task_report_new_error (
            self,
            callback,
            user_data,
            mm_base_bearer_connect,
            MM_CORE_ERROR,
            MM_CORE_ERROR_FAILED,
            "Bearer doesn't allow explicit connection requests");
        return;
    }

    /* If already connecting, return error, don't allow a second request. */
    if (self->priv->status == MM_BEARER_STATUS_CONNECTING) {
        g_task_report_new_error (
            self,
            callback,
            user_data,
            mm_base_bearer_connect,
            MM_CORE_ERROR,
            MM_CORE_ERROR_IN_PROGRESS,
            "Bearer already being connected");
        return;
    }

    /* If currently disconnecting, return error, previous operation should
     * finish before allowing to connect again. */
    if (self->priv->status == MM_BEARER_STATUS_DISCONNECTING) {
        g_task_report_new_error (
            self,
            callback,
            user_data,
            mm_base_bearer_connect,
            MM_CORE_ERROR,
            MM_CORE_ERROR_FAILED,
            "Bearer currently being disconnected");
        return;
    }

    /* Check 3GPP roaming allowance, *only* roaming related here */
    if (mm_iface_modem_is_3gpp (MM_IFACE_MODEM (self->priv->modem)) &&
        self->priv->reason_3gpp == CONNECTION_FORBIDDEN_REASON_ROAMING) {
        g_task_report_new_error (
            self,
            callback,
            user_data,
            mm_base_bearer_connect,
            MM_CORE_ERROR,
            MM_CORE_ERROR_UNAUTHORIZED,
            "Not allowed to connect bearer in 3GPP network: '%s'",
            connection_forbidden_reason_str[self->priv->reason_3gpp]);
        return;
    }

    /* Check CDMA roaming allowance, *only* roaming related here */
    if (mm_iface_modem_is_cdma (MM_IFACE_MODEM (self->priv->modem)) &&
        self->priv->reason_cdma == CONNECTION_FORBIDDEN_REASON_ROAMING) {
        g_task_report_new_error (
            self,
            callback,
            user_data,
            mm_base_bearer_connect,
            MM_CORE_ERROR,
            MM_CORE_ERROR_UNAUTHORIZED,
            "Not allowed to connect bearer in CDMA network: '%s'",
            connection_forbidden_reason_str[self->priv->reason_cdma]);
        return;
    }

    task = g_task_new (self, NULL, callback, user_data);

    /* If already connected, done */
    if (self->priv->status == MM_BEARER_STATUS_CONNECTED) {
        g_task_return_boolean (task, TRUE);
        g_object_unref (task);
        return;
    }

    /* Update total attempts */
    mm_bearer_stats_set_attempts (self->priv->stats,
                                  mm_bearer_stats_get_attempts (self->priv->stats) + 1);
    bearer_reset_ongoing_interface_stats (self);

    /* Clear previous connection error, if any */
    bearer_update_connection_error (self, NULL);

    /* The connect request may imply a profile update internally, so ignore it */
    if (mm_iface_modem_is_3gpp (MM_IFACE_MODEM (self->priv->modem)))
        mm_iface_modem_3gpp_profile_manager_update_ignore_start (MM_IFACE_MODEM_3GPP_PROFILE_MANAGER (self->priv->modem));

    /* Connecting! */
    mm_obj_dbg (self, "connecting...");
    self->priv->connect_cancellable = g_cancellable_new ();
    bearer_update_status (self, MM_BEARER_STATUS_CONNECTING);
    MM_BASE_BEARER_GET_CLASS (self)->connect (
        self,
        self->priv->connect_cancellable,
        (GAsyncReadyCallback)connect_ready,
        task);
}

typedef struct {
    MMBaseBearer *self;
    GDBusMethodInvocation *invocation;
} HandleConnectContext;

static void
handle_connect_context_free (HandleConnectContext *ctx)
{
    g_object_unref (ctx->invocation);
    g_object_unref (ctx->self);
    g_free (ctx);
}

static void
handle_connect_ready (MMBaseBearer *self,
                      GAsyncResult *res,
                      HandleConnectContext *ctx)
{
    GError *error = NULL;

    if (!mm_base_bearer_connect_finish (self, res, &error))
        mm_dbus_method_invocation_take_error (ctx->invocation, error);
    else
        mm_gdbus_bearer_complete_connect (MM_GDBUS_BEARER (self), ctx->invocation);

    handle_connect_context_free (ctx);
}

static void
handle_connect_auth_ready (MMAuthProvider *authp,
                           GAsyncResult *res,
                           HandleConnectContext *ctx)
{
    GError *error = NULL;

    if (!mm_auth_provider_authorize_finish (authp, res, &error)) {
        mm_dbus_method_invocation_take_error (ctx->invocation, error);
        handle_connect_context_free (ctx);
        return;
    }

    mm_obj_info (ctx->self, "processing user request to connect...");
    mm_base_bearer_connect (ctx->self,
                            (GAsyncReadyCallback)handle_connect_ready,
                            ctx);
}

static gboolean
handle_connect (MMBaseBearer *self,
                GDBusMethodInvocation *invocation)
{
    HandleConnectContext *ctx;

    ctx = g_new0 (HandleConnectContext, 1);
    ctx->self = g_object_ref (self);
    ctx->invocation = g_object_ref (invocation);

    mm_auth_provider_authorize (self->priv->authp,
                                invocation,
                                MM_AUTHORIZATION_DEVICE_CONTROL,
                                self->priv->authp_cancellable,
                                (GAsyncReadyCallback)handle_connect_auth_ready,
                                ctx);
    return TRUE;
}

/*****************************************************************************/
/* DISCONNECT */

gboolean
mm_base_bearer_disconnect_finish (MMBaseBearer *self,
                                  GAsyncResult *res,
                                  GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
disconnect_ready (MMBaseBearer *self,
                  GAsyncResult *res,
                  GTask *task)
{
    GError *error = NULL;

    if (!MM_BASE_BEARER_GET_CLASS (self)->disconnect_finish (self, res, &error)) {
        mm_obj_dbg (self, "couldn't disconnect: %s", error->message);
        bearer_update_status (self, MM_BEARER_STATUS_CONNECTED);
        g_task_return_error (task, error);
    }
    else {
        mm_obj_dbg (self, "disconnected");
        bearer_update_status (self, MM_BEARER_STATUS_DISCONNECTED);
        g_task_return_boolean (task, TRUE);
    }

    g_object_unref (task);
}

static void
status_changed_complete_disconnect (MMBaseBearer *self,
                                    GParamSpec *pspec,
                                    GTask *task)
{
    /* We may get other states here before DISCONNECTED, like DISCONNECTING or
     * even CONNECTED. */
    if (self->priv->status != MM_BEARER_STATUS_DISCONNECTED)
        return;

    mm_obj_dbg (self, "disconnected after cancelling previous connect request");
    g_signal_handler_disconnect (self,
                                 self->priv->disconnect_signal_handler);
    self->priv->disconnect_signal_handler = 0;

    /* Note: interface state is updated when the DISCONNECTED state is set */

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

void
mm_base_bearer_disconnect (MMBaseBearer *self,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    if (!MM_BASE_BEARER_GET_CLASS (self)->disconnect) {
        g_assert (!MM_BASE_BEARER_GET_CLASS (self)->disconnect_finish);
        g_task_return_new_error (
            task,
            MM_CORE_ERROR,
            MM_CORE_ERROR_FAILED,
            "Bearer doesn't allow explicit disconnection requests");
        g_object_unref (task);
        return;
    }

    /* If already disconnected, done */
    if (self->priv->status == MM_BEARER_STATUS_DISCONNECTED) {
        g_task_return_boolean (task, TRUE);
        g_object_unref (task);
        return;
    }

    /* If already disconnecting, return error, don't allow a second request. */
    if (self->priv->status == MM_BEARER_STATUS_DISCONNECTING) {
        g_task_return_new_error (
            task,
            MM_CORE_ERROR,
            MM_CORE_ERROR_IN_PROGRESS,
            "Bearer already being disconnected");
        g_object_unref (task);
        return;
    }

    /* If currently connecting, try to cancel that operation, and wait to get
     * disconnected. */
    if (self->priv->status == MM_BEARER_STATUS_CONNECTING) {
        /* Set ourselves as disconnecting */
        bearer_update_status (self, MM_BEARER_STATUS_DISCONNECTING);

        /* We MUST ensure that we get to DISCONNECTED */
        g_cancellable_cancel (self->priv->connect_cancellable);
        /* Note that we only allow to remove disconnected bearers, so should
         * be safe to assume that we'll get the signal handler called properly
         */
        self->priv->disconnect_signal_handler =
            g_signal_connect (self,
                              "notify::" MM_BASE_BEARER_STATUS,
                              (GCallback)status_changed_complete_disconnect,
                              task); /* takes ownership */

        return;
    }

    /* Disconnecting! */
    bearer_update_status (self, MM_BEARER_STATUS_DISCONNECTING);
    MM_BASE_BEARER_GET_CLASS (self)->disconnect (
        self,
        (GAsyncReadyCallback)disconnect_ready,
        task); /* takes ownership */
}

typedef struct {
    MMBaseBearer *self;
    GDBusMethodInvocation *invocation;
} HandleDisconnectContext;

static void
handle_disconnect_context_free (HandleDisconnectContext *ctx)
{
    g_object_unref (ctx->invocation);
    g_object_unref (ctx->self);
    g_free (ctx);
}

static void
handle_disconnect_ready (MMBaseBearer *self,
                         GAsyncResult *res,
                         HandleDisconnectContext *ctx)
{
    GError *error = NULL;

    if (!mm_base_bearer_disconnect_finish (self, res, &error))
        mm_dbus_method_invocation_take_error (ctx->invocation, error);
    else
        mm_gdbus_bearer_complete_disconnect (MM_GDBUS_BEARER (self), ctx->invocation);

    handle_disconnect_context_free (ctx);
}

static void
handle_disconnect_auth_ready (MMAuthProvider *authp,
                              GAsyncResult *res,
                              HandleDisconnectContext *ctx)
{
    GError *error = NULL;

    if (!mm_auth_provider_authorize_finish (authp, res, &error)) {
        mm_dbus_method_invocation_take_error (ctx->invocation, error);
        handle_disconnect_context_free (ctx);
        return;
    }

    mm_obj_info (ctx->self, "processing user request to disconnect...");
    mm_base_bearer_disconnect (ctx->self,
                               (GAsyncReadyCallback)handle_disconnect_ready,
                               ctx);
}

static gboolean
handle_disconnect (MMBaseBearer *self,
                   GDBusMethodInvocation *invocation)
{
    HandleDisconnectContext *ctx;

    ctx = g_new0 (HandleDisconnectContext, 1);
    ctx->self = g_object_ref (self);
    ctx->invocation = g_object_ref (invocation);

    mm_auth_provider_authorize (self->priv->authp,
                                invocation,
                                MM_AUTHORIZATION_DEVICE_CONTROL,
                                self->priv->authp_cancellable,
                                (GAsyncReadyCallback)handle_disconnect_auth_ready,
                                ctx);
    return TRUE;
}

/*****************************************************************************/

static void
base_bearer_dbus_export (MMBaseBearer *self)
{
    GError *error = NULL;

    /* Handle method invocations */
    g_signal_connect (self,
                      "handle-connect",
                      G_CALLBACK (handle_connect),
                      NULL);
    g_signal_connect (self,
                      "handle-disconnect",
                      G_CALLBACK (handle_disconnect),
                      NULL);

    if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self),
                                           self->priv->connection,
                                           self->priv->path,
                                           &error)) {
        mm_obj_warn (self, "couldn't export to bus: %s", error->message);
        g_error_free (error);
    }
}

static void
base_bearer_dbus_unexport (MMBaseBearer *self)
{
    const gchar *path;

    path = g_dbus_interface_skeleton_get_object_path (G_DBUS_INTERFACE_SKELETON (self));
    /* Only unexport if currently exported */
    if (path) {
        mm_obj_dbg (self, "removing from bus");
        g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self));
    }
}

/*****************************************************************************/

MMBearerStatus
mm_base_bearer_get_status (MMBaseBearer *self)
{
    return self->priv->status;
}

const gchar *
mm_base_bearer_get_path (MMBaseBearer *self)
{
    return self->priv->path;
}

MMBearerProperties *
mm_base_bearer_peek_config (MMBaseBearer *self)
{
    return self->priv->config;
}

MMBearerProperties *
mm_base_bearer_get_config (MMBaseBearer *self)
{
    return (self->priv->config ?
            g_object_ref (self->priv->config) :
            NULL);
}

gint
mm_base_bearer_get_profile_id (MMBaseBearer *self)
{
    return mm_gdbus_bearer_get_profile_id (MM_GDBUS_BEARER (self));
}

MMBearerApnType
mm_base_bearer_get_apn_type (MMBaseBearer *self)
{
    /* when none explicitly requested, apn type always defaults to internet */
    return (self->priv->config ?
            mm_bearer_properties_get_apn_type (self->priv->config) :
            MM_BEARER_APN_TYPE_DEFAULT);
}

/*****************************************************************************/

static void
disconnect_force_ready (MMBaseBearer *self,
                        GAsyncResult *res)
{
    GError *error = NULL;

    if (!MM_BASE_BEARER_GET_CLASS (self)->disconnect_finish (self, res, &error)) {
        mm_obj_warn (self, "error disconnecting: %s; will assume disconnected anyway", error->message);
        g_error_free (error);
    }
    else
        mm_obj_dbg (self, "disconnected");

    /* Report disconnection to the bearer object using class method
     * mm_bearer_report_connection_status. This gives subclass implementations a
     * chance to correctly update their own connection state, in case this base
     * class ignores a failed disconnection attempt.
     */
    mm_base_bearer_report_connection_status (self, MM_BEARER_CONNECTION_STATUS_DISCONNECTED);
}

void
mm_base_bearer_disconnect_force (MMBaseBearer *self)
{
    if (self->priv->status == MM_BEARER_STATUS_DISCONNECTING ||
        self->priv->status == MM_BEARER_STATUS_DISCONNECTED)
        return;

    if (self->priv->ignore_disconnection_reports) {
        mm_obj_msg (self, "disconnection should be forced, but we can't. Request disconnection instead.");
        mm_gdbus_bearer_set_disconnect_request (MM_GDBUS_BEARER(self), TRUE);
        bearer_run_dispatcher_scripts (self,
                                       MM_DISPATCHER_CONNECTION_EVENT_DISCONNECT_REQUEST);
        return;
    }

    mm_obj_msg (self, "forcing disconnection");

    /* If currently connecting, try to cancel that operation. */
    if (self->priv->status == MM_BEARER_STATUS_CONNECTING) {
        g_cancellable_cancel (self->priv->connect_cancellable);
        return;
    }

    /* Disconnecting! */
    bearer_update_status (self, MM_BEARER_STATUS_DISCONNECTING);
    MM_BASE_BEARER_GET_CLASS (self)->disconnect (
        self,
        (GAsyncReadyCallback)disconnect_force_ready,
        NULL);
}

/*****************************************************************************/

static void
report_connection_status (MMBaseBearer             *self,
                          MMBearerConnectionStatus status,
                          const GError             *connection_error)
{
    /* The only status expected at this point is DISCONNECTED or CONNECTED,
     * although here we just process the DISCONNECTED one.
     */
    g_assert (status == MM_BEARER_CONNECTION_STATUS_CONNECTED || status == MM_BEARER_CONNECTION_STATUS_DISCONNECTED);

    /* In the generic bearer implementation we just need to reset the
     * interface status */
    if (status == MM_BEARER_CONNECTION_STATUS_DISCONNECTED) {
        bearer_update_connection_error (self, connection_error);
        bearer_update_status (self, MM_BEARER_STATUS_DISCONNECTED);
    }
}

/*
 * This method is used exclusively in two different scenarios:
 *  a) to report disconnections detected by ModemManager itself (e.g. based on
 *     CGACT polling or CGEV URCs), applicable to bearers using both NET and
 *     PPP data ports.
 *  b) to report failed or successful connection attempts by plugins using NET
 *     data ports that rely on vendor-specific URCs (e.g. Icera, MBM, Option
 *     HSO).
 *
 * The method is also subclass-able because plugins may require specific
 * cleanup operations to be done when a bearer is reported as disconnected.
 * (e.g. the QMI or MBIM implementations require removing signal handlers).
 *
 * For all the scenarios involving a) the plugins are required to call the
 * parent report_connection_status() implementation to report the
 * DISCONNECTED state. For scenarios involving b) the parent reporting is not
 * expected at all. In other words, the parent report_connection_status()
 * is exclusively used in processing disconnections detected by ModemManager
 * itself.
 *
 * If the bearer has been connected and it has required PPP method, we will
 * ignore all disconnection reports because we cannot disconnect a PPP-based
 * bearer before the upper layers have stopped using the TTY. In this case,
 * we must wait for upper layers to detect the disconnection themselves (e.g.
 * pppd should detect it) and disconnect the bearer through DBus.
 */
void
mm_base_bearer_report_connection_status_detailed (MMBaseBearer             *self,
                                                  MMBearerConnectionStatus  status,
                                                  const GError             *connection_error)
{
    /* Reporting disconnection? */
    if (status == MM_BEARER_CONNECTION_STATUS_DISCONNECTED || status == MM_BEARER_CONNECTION_STATUS_CONNECTION_FAILED) {
        if (self->priv->ignore_disconnection_reports) {
            mm_obj_dbg (self, "ignoring disconnection report");
            return;
        }

        /* Setup a generic default error if none explicitly given when reporting
         * bearer disconnections. */
        if (!connection_error) {
            g_autoptr(GError) default_connection_error = NULL;

            default_connection_error = mm_mobile_equipment_error_for_code (MM_MOBILE_EQUIPMENT_ERROR_UNKNOWN, self);
            return MM_BASE_BEARER_GET_CLASS (self)->report_connection_status (self, status, default_connection_error);
        }
    }

    return MM_BASE_BEARER_GET_CLASS (self)->report_connection_status (self, status, connection_error);
}

/*****************************************************************************/

#if defined WITH_SUSPEND_RESUME

typedef struct _SyncingContext SyncingContext;
static void interface_syncing_step (GTask *task);

typedef enum {
    SYNCING_STEP_FIRST,
    SYNCING_STEP_REFRESH_CONNECTION,
    SYNCING_STEP_LAST
} SyncingStep;

struct _SyncingContext {
    SyncingStep    step;
    MMBearerStatus status;
};

gboolean
mm_base_bearer_sync_finish (MMBaseBearer  *self,
                            GAsyncResult  *res,
                            GError       **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
reload_connection_status_ready (MMBaseBearer *self,
                                GAsyncResult *res,
                                GTask        *task)
{
    SyncingContext           *ctx;
    MMBearerConnectionStatus  reloaded_status;
    g_autoptr(GError)         error = NULL;

    ctx = g_task_get_task_data (task);

    /* The only update we're really interested in is the connected->disconnected
     * one, because any other would be extremely strange and it's probably not
     * worth trying to support those; e.g. a disconnected->connected change here
     * would be impossible to be handled correctly. We'll also ignore intermediate
     * states (connecting/disconnecting), as we can rely on the reports of the final
     * state at some point soon.
     *
     * So, just handle DISCONNECTED at this point.
     */
    reloaded_status = MM_BASE_BEARER_GET_CLASS (self)->reload_connection_status_finish (self, res, &error);
    if (reloaded_status == MM_BEARER_CONNECTION_STATUS_UNKNOWN)
        mm_obj_warn (self, "reloading connection status failed: %s", error->message);
    else if ((ctx->status == MM_BEARER_STATUS_CONNECTED) &&
             (reloaded_status == MM_BEARER_CONNECTION_STATUS_DISCONNECTED)) {
        mm_obj_dbg (self, "disconnection detected during status synchronization");
        mm_base_bearer_report_connection_status (self, reloaded_status);
    }

    /* Go on to the next step */
    ctx->step++;
    interface_syncing_step (task);
}

static void
interface_syncing_step (GTask *task)
{
    MMBaseBearer   *self;
    SyncingContext *ctx;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    switch (ctx->step) {
    case SYNCING_STEP_FIRST:
        ctx->step++;
        /* fall through */

    case SYNCING_STEP_REFRESH_CONNECTION:
        /*
         * AT+PPP based connections should not be synced.
         * When a AT+PPP connection bearer is connected, the 'ignore_disconnection_reports' flag is set.
         */
        if (!self->priv->ignore_disconnection_reports) {
            if (!MM_BASE_BEARER_GET_CLASS (self)->reload_connection_status)
                mm_obj_warn (self, "unable to reload connection status, method not implemented");
            else {
                mm_obj_dbg (self, "refreshing connection status");
                MM_BASE_BEARER_GET_CLASS (self)->reload_connection_status (self,
                                                                           (GAsyncReadyCallback) reload_connection_status_ready,
                                                                           task);
                return;
            }
        }
        ctx->step++;
        /* fall through */

    case SYNCING_STEP_LAST:
        /* We are done without errors! */
        g_task_return_boolean (task, TRUE);
        g_object_unref (task);
        return;

    default:
        break;
    }

    g_assert_not_reached ();
}

void
mm_base_bearer_sync (MMBaseBearer        *self,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
    SyncingContext *ctx;
    GTask          *task;

    /* Create SyncingContext and store the original bearer status */
    ctx = g_new0 (SyncingContext, 1);
    ctx->step = SYNCING_STEP_FIRST;
    ctx->status = self->priv->status;

    /* Create sync steps task and execute it */
    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, (GDestroyNotify)g_free);
    interface_syncing_step (task);
}

#endif

/*****************************************************************************/

static gchar *
log_object_build_id (MMLogObject *_self)
{
    MMBaseBearer *self;

    self = MM_BASE_BEARER (_self);
    return g_strdup_printf ("bearer%u", self->priv->dbus_id);
}

/*****************************************************************************/

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    MMBaseBearer *self = MM_BASE_BEARER (object);

    switch (prop_id) {
    case PROP_PATH:
        g_free (self->priv->path);
        self->priv->path = g_value_dup_string (value);

        /* Export when we get a DBus connection AND we have a path */
        if (self->priv->path &&
            self->priv->connection)
            base_bearer_dbus_export (self);
        break;
    case PROP_CONNECTION:
        g_clear_object (&self->priv->connection);
        self->priv->connection = g_value_dup_object (value);

        /* Export when we get a DBus connection AND we have a path */
        if (!self->priv->connection)
            base_bearer_dbus_unexport (self);
        else if (self->priv->path)
            base_bearer_dbus_export (self);
        break;
    case PROP_BIND_TO:
        g_clear_object (&self->priv->bind_to);
        self->priv->bind_to = g_value_dup_object (value);
        mm_bind_to (MM_BIND (self), MM_BASE_BEARER_CONNECTION, self->priv->bind_to);
        break;
    case PROP_MODEM:
        g_clear_object (&self->priv->modem);
        self->priv->modem = g_value_dup_object (value);
        if (self->priv->modem) {
            if (self->priv->config) {
                /* Listen to 3GPP/CDMA registration state changes. We need both
                 * 'config' and 'modem' set. */
                set_signal_handlers (self);
            }
        }
        break;
    case PROP_STATUS:
        /* We don't allow g_object_set()-ing the status property */
        g_assert_not_reached ();
        break;
    case PROP_CONFIG: {
        GVariant *dictionary;

        g_clear_object (&self->priv->config);
        self->priv->config = g_value_dup_object (value);
        if (self->priv->modem) {
            /* Listen to 3GPP/CDMA registration state changes. We need both
             * 'config' and 'modem' set. */
            set_signal_handlers (self);
        }
        /* Also expose the properties */
        dictionary = mm_bearer_properties_get_dictionary (self->priv->config);
        mm_gdbus_bearer_set_properties (MM_GDBUS_BEARER (self), dictionary);
        if (dictionary)
            g_variant_unref (dictionary);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
get_property (GObject *object,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
    MMBaseBearer *self = MM_BASE_BEARER (object);

    switch (prop_id) {
    case PROP_PATH:
        g_value_set_string (value, self->priv->path);
        break;
    case PROP_CONNECTION:
        g_value_set_object (value, self->priv->connection);
        break;
    case PROP_BIND_TO:
        g_value_set_object (value, self->priv->bind_to);
        break;
    case PROP_MODEM:
        g_value_set_object (value, self->priv->modem);
        break;
    case PROP_STATUS:
        g_value_set_enum (value, self->priv->status);
        break;
    case PROP_CONFIG:
        g_value_set_object (value, self->priv->config);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
mm_base_bearer_init (MMBaseBearer *self)
{
    static guint id = 0;

    /* Initialize private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                              MM_TYPE_BASE_BEARER,
                                              MMBaseBearerPrivate);

    /* Each bearer is given a unique id to build its own DBus path */
    self->priv->dbus_id = id++;

    /* Setup authorization provider */
    self->priv->authp = mm_auth_provider_get ();
    self->priv->authp_cancellable = g_cancellable_new ();

    self->priv->status = MM_BEARER_STATUS_DISCONNECTED;
    self->priv->reason_3gpp = CONNECTION_FORBIDDEN_REASON_NONE;
    self->priv->reason_cdma = CONNECTION_FORBIDDEN_REASON_NONE;
    self->priv->reload_stats_supported = FALSE;
    self->priv->stats = mm_bearer_stats_new ();

    /* Set defaults */
    mm_gdbus_bearer_set_interface   (MM_GDBUS_BEARER (self), NULL);
    mm_gdbus_bearer_set_multiplexed (MM_GDBUS_BEARER (self), FALSE);
    mm_gdbus_bearer_set_profile_id  (MM_GDBUS_BEARER (self), MM_3GPP_PROFILE_ID_UNKNOWN);
    mm_gdbus_bearer_set_connected   (MM_GDBUS_BEARER (self), FALSE);
    mm_gdbus_bearer_set_suspended   (MM_GDBUS_BEARER (self), FALSE);
    mm_gdbus_bearer_set_properties  (MM_GDBUS_BEARER (self), NULL);
    mm_gdbus_bearer_set_ip_timeout  (MM_GDBUS_BEARER (self), BEARER_IP_TIMEOUT_DEFAULT);
    mm_gdbus_bearer_set_bearer_type (MM_GDBUS_BEARER (self), MM_BEARER_TYPE_DEFAULT);
    mm_gdbus_bearer_set_ip4_config  (MM_GDBUS_BEARER (self),
                                     mm_bearer_ip_config_get_dictionary (NULL));
    mm_gdbus_bearer_set_ip6_config  (MM_GDBUS_BEARER (self),
                                     mm_bearer_ip_config_get_dictionary (NULL));
    bearer_update_interface_stats (self);
}

static void
finalize (GObject *object)
{
    MMBaseBearer *self = MM_BASE_BEARER (object);

    g_free (self->priv->path);

    G_OBJECT_CLASS (mm_base_bearer_parent_class)->finalize (object);
}

static void
dispose (GObject *object)
{
    MMBaseBearer *self = MM_BASE_BEARER (object);

    connection_monitor_stop (self);
    bearer_stats_stop (self);
    g_clear_object (&self->priv->stats);

    if (self->priv->connection) {
        base_bearer_dbus_unexport (self);
        g_clear_object (&self->priv->connection);
    }

    reset_signal_handlers (self);
    reset_deferred_unregistration (self);

    g_clear_object (&self->priv->modem);
    g_clear_object (&self->priv->bind_to);
    g_clear_object (&self->priv->config);
    g_cancellable_cancel (self->priv->authp_cancellable);
    g_clear_object (&self->priv->authp_cancellable);

    G_OBJECT_CLASS (mm_base_bearer_parent_class)->dispose (object);
}

static void
log_object_iface_init (MMLogObjectInterface *iface)
{
    iface->build_id = log_object_build_id;
}

static void
bind_iface_init (MMBindInterface *iface)
{
}

static void
mm_base_bearer_class_init (MMBaseBearerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMBaseBearerPrivate));

    /* Virtual methods */
    object_class->get_property = get_property;
    object_class->set_property = set_property;
    object_class->finalize = finalize;
    object_class->dispose = dispose;

    klass->report_connection_status = report_connection_status;

    properties[PROP_CONNECTION] =
        g_param_spec_object (MM_BASE_BEARER_CONNECTION,
                             "Connection",
                             "GDBus connection to the system bus.",
                             G_TYPE_DBUS_CONNECTION,
                             G_PARAM_READWRITE);
    g_object_class_install_property (object_class, PROP_CONNECTION, properties[PROP_CONNECTION]);

    properties[PROP_PATH] =
        g_param_spec_string (MM_BASE_BEARER_PATH,
                             "Path",
                             "DBus path of the Bearer",
                             NULL,
                             G_PARAM_READWRITE);
    g_object_class_install_property (object_class, PROP_PATH, properties[PROP_PATH]);

    g_object_class_override_property (object_class, PROP_BIND_TO, MM_BIND_TO);

    properties[PROP_MODEM] =
        g_param_spec_object (MM_BASE_BEARER_MODEM,
                             "Modem",
                             "The Modem which owns this Bearer",
                             MM_TYPE_BASE_MODEM,
                             G_PARAM_READWRITE);
    g_object_class_install_property (object_class, PROP_MODEM, properties[PROP_MODEM]);

    properties[PROP_STATUS] =
        g_param_spec_enum (MM_BASE_BEARER_STATUS,
                           "Bearer status",
                           "Status of the bearer",
                           MM_TYPE_BEARER_STATUS,
                           MM_BEARER_STATUS_DISCONNECTED,
                           G_PARAM_READABLE);
    g_object_class_install_property (object_class, PROP_STATUS, properties[PROP_STATUS]);

    properties[PROP_CONFIG] =
        g_param_spec_object (MM_BASE_BEARER_CONFIG,
                             "Bearer configuration",
                             "List of user provided properties",
                             MM_TYPE_BEARER_PROPERTIES,
                             G_PARAM_READWRITE);
    g_object_class_install_property (object_class, PROP_CONFIG, properties[PROP_CONFIG]);
}

/*****************************************************************************/
/* Helpers to implement connect() */

struct _MMBearerConnectResult {
    volatile gint     ref_count;
    MMPort           *data;
    MMBearerIpConfig *ipv4_config;
    MMBearerIpConfig *ipv6_config;
    gboolean          multiplexed;
    gint              profile_id;
    guint64           uplink_speed;
    guint64           downlink_speed;
};

MMBearerConnectResult *
mm_bearer_connect_result_ref (MMBearerConnectResult *result)
{
    g_atomic_int_inc (&result->ref_count);
    return result;
}

void
mm_bearer_connect_result_unref (MMBearerConnectResult *result)
{
    if (g_atomic_int_dec_and_test (&result->ref_count)) {
        if (result->ipv4_config)
            g_object_unref (result->ipv4_config);
        if (result->ipv6_config)
            g_object_unref (result->ipv6_config);
        if (result->data)
            g_object_unref (result->data);
        g_slice_free (MMBearerConnectResult, result);
    }
}

MMPort *
mm_bearer_connect_result_peek_data (MMBearerConnectResult *result)
{
    return result->data;
}

MMBearerIpConfig *
mm_bearer_connect_result_peek_ipv4_config (MMBearerConnectResult *result)
{
    return result->ipv4_config;
}

MMBearerIpConfig *
mm_bearer_connect_result_peek_ipv6_config (MMBearerConnectResult *result)
{
    return result->ipv6_config;
}

void
mm_bearer_connect_result_set_multiplexed (MMBearerConnectResult *result,
                                          gboolean               multiplexed)
{
    result->multiplexed = multiplexed;
}

gboolean
mm_bearer_connect_result_get_multiplexed (MMBearerConnectResult *result)
{
    return result->multiplexed;
}

void
mm_bearer_connect_result_set_profile_id (MMBearerConnectResult *result,
                                         gint                   profile_id)
{
    result->profile_id = profile_id;
}

gint
mm_bearer_connect_result_get_profile_id (MMBearerConnectResult *result)
{
    return result->profile_id;
}

void
mm_bearer_connect_result_set_uplink_speed (MMBearerConnectResult *result,
                                           guint64                speed)
{
    result->uplink_speed = speed;
}

guint64
mm_bearer_connect_result_get_uplink_speed (MMBearerConnectResult *result)
{
    return result->uplink_speed;
}

void
mm_bearer_connect_result_set_downlink_speed (MMBearerConnectResult *result,
                                             guint64                speed)
{
    result->downlink_speed = speed;
}

guint64
mm_bearer_connect_result_get_downlink_speed (MMBearerConnectResult *result)
{
    return result->downlink_speed;
}

MMBearerConnectResult *
mm_bearer_connect_result_new (MMPort           *data,
                              MMBearerIpConfig *ipv4_config,
                              MMBearerIpConfig *ipv6_config)
{
    MMBearerConnectResult *result;

    /* 'data' must always be given */
    g_assert (MM_IS_PORT (data));

    result = g_slice_new0 (MMBearerConnectResult);
    result->ref_count = 1;
    result->data = g_object_ref (data);
    if (ipv4_config)
        result->ipv4_config = g_object_ref (ipv4_config);
    if (ipv6_config)
        result->ipv6_config = g_object_ref (ipv6_config);
    result->multiplexed = FALSE; /* default */
    result->profile_id = MM_3GPP_PROFILE_ID_UNKNOWN;
    return result;
}
