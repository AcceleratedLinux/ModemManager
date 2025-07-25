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
 * Copyright (C) 2011-2012 Google, Inc.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc.
 */

#ifndef MM_IFACE_MODEM_3GPP_H
#define MM_IFACE_MODEM_3GPP_H

#include <glib-object.h>
#include <gio/gio.h>
#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-iface-modem.h"
#include "mm-base-bearer.h"
#include "mm-port-serial-at.h"

#define MM_TYPE_IFACE_MODEM_3GPP mm_iface_modem_3gpp_get_type ()
G_DECLARE_INTERFACE (MMIfaceModem3gpp, mm_iface_modem_3gpp, MM, IFACE_MODEM_3GPP, MMIfaceModem)

#define MM_IFACE_MODEM_3GPP_DBUS_SKELETON           "iface-modem-3gpp-dbus-skeleton"
#define MM_IFACE_MODEM_3GPP_REGISTRATION_STATE      "iface-modem-3gpp-registration-state"
#define MM_IFACE_MODEM_3GPP_CS_NETWORK_SUPPORTED    "iface-modem-3gpp-cs-network-supported"
#define MM_IFACE_MODEM_3GPP_PS_NETWORK_SUPPORTED    "iface-modem-3gpp-ps-network-supported"
#define MM_IFACE_MODEM_3GPP_EPS_NETWORK_SUPPORTED   "iface-modem-3gpp-eps-network-supported"
#define MM_IFACE_MODEM_3GPP_5GS_NETWORK_SUPPORTED   "iface-modem-3gpp-5gs-network-supported"
#define MM_IFACE_MODEM_3GPP_IGNORED_FACILITY_LOCKS  "iface-modem-3gpp-ignored-facility-locks"
#define MM_IFACE_MODEM_3GPP_INITIAL_EPS_BEARER      "iface-modem-3gpp-initial-eps-bearer"
#define MM_IFACE_MODEM_3GPP_PACKET_SERVICE_STATE    "iface-modem-3gpp-packet-service-state"

#define MM_IFACE_MODEM_3GPP_ALL_ACCESS_TECHNOLOGIES_MASK    \
    (MM_MODEM_ACCESS_TECHNOLOGY_GSM |                       \
     MM_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT |               \
     MM_MODEM_ACCESS_TECHNOLOGY_GPRS |                      \
     MM_MODEM_ACCESS_TECHNOLOGY_EDGE |                      \
     MM_MODEM_ACCESS_TECHNOLOGY_UMTS |                      \
     MM_MODEM_ACCESS_TECHNOLOGY_HSDPA |                     \
     MM_MODEM_ACCESS_TECHNOLOGY_HSUPA |                     \
     MM_MODEM_ACCESS_TECHNOLOGY_HSPA |                      \
     MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS |                 \
     MM_MODEM_ACCESS_TECHNOLOGY_LTE |                       \
     MM_MODEM_ACCESS_TECHNOLOGY_5GNR)

struct _MMIfaceModem3gppInterface {
    GTypeInterface g_iface;

    /* Loading of the IMEI property */
    void (*load_imei) (MMIfaceModem3gpp *self,
                       GAsyncReadyCallback callback,
                       gpointer user_data);
    gchar * (*load_imei_finish) (MMIfaceModem3gpp *self,
                                 GAsyncResult *res,
                                 GError **error);

    /* Loading of the facility locks property */
    void (*load_enabled_facility_locks) (MMIfaceModem3gpp *self,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data);
    MMModem3gppFacility (*load_enabled_facility_locks_finish) (MMIfaceModem3gpp *self,
                                                               GAsyncResult *res,
                                                               GError **error);

    /* Loading of the UE mode of operation for EPS property */
    void                          (* load_eps_ue_mode_operation)        (MMIfaceModem3gpp     *self,
                                                                         GAsyncReadyCallback   callback,
                                                                         gpointer              user_data);
    MMModem3gppEpsUeModeOperation (* load_eps_ue_mode_operation_finish) (MMIfaceModem3gpp     *self,
                                                                         GAsyncResult         *res,
                                                                         GError              **error);

    /* Asynchronous setting up unsolicited events */
    void (*setup_unsolicited_events) (MMIfaceModem3gpp *self,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data);
    gboolean (*setup_unsolicited_events_finish) (MMIfaceModem3gpp *self,
                                                 GAsyncResult *res,
                                                 GError **error);

    /* Asynchronous enabling of unsolicited events */
    void (*enable_unsolicited_events) (MMIfaceModem3gpp *self,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data);
    gboolean (*enable_unsolicited_events_finish) (MMIfaceModem3gpp *self,
                                                  GAsyncResult *res,
                                                  GError **error);

    /* Asynchronous cleaning up of unsolicited events */
    void (*cleanup_unsolicited_events) (MMIfaceModem3gpp *self,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data);
    gboolean (*cleanup_unsolicited_events_finish) (MMIfaceModem3gpp *self,
                                                   GAsyncResult *res,
                                                   GError **error);

    /* Asynchronous disabling of unsolicited events */
    void (*disable_unsolicited_events) (MMIfaceModem3gpp *self,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data);
    gboolean (*disable_unsolicited_events_finish) (MMIfaceModem3gpp *self,
                                                   GAsyncResult *res,
                                                   GError **error);

    /* Setup unsolicited registration messages */
    void (* setup_unsolicited_registration_events) (MMIfaceModem3gpp *self,
                                                    GAsyncReadyCallback callback,
                                                    gpointer user_data);
    gboolean (*setup_unsolicited_registration_events_finish) (MMIfaceModem3gpp *self,
                                                              GAsyncResult *res,
                                                              GError **error);

    /* Asynchronous enabling of unsolicited registration events */
    void (*enable_unsolicited_registration_events) (MMIfaceModem3gpp *self,
                                                    gboolean cs_supported,
                                                    gboolean ps_supported,
                                                    gboolean eps_supported,
                                                    GAsyncReadyCallback callback,
                                                    gpointer user_data);
    gboolean (*enable_unsolicited_registration_events_finish) (MMIfaceModem3gpp *self,
                                                               GAsyncResult *res,
                                                               GError **error);

    /* Cleanup unsolicited registration messages */
    void (* cleanup_unsolicited_registration_events) (MMIfaceModem3gpp *self,
                                                      GAsyncReadyCallback callback,
                                                      gpointer user_data);
    gboolean (*cleanup_unsolicited_registration_events_finish) (MMIfaceModem3gpp *self,
                                                                GAsyncResult *res,
                                                                GError **error);
    /* Asynchronous disabling of unsolicited registration events */
    void (*disable_unsolicited_registration_events) (MMIfaceModem3gpp *self,
                                                     gboolean cs_supported,
                                                     gboolean ps_supported,
                                                     gboolean eps_supported,
                                                     GAsyncReadyCallback callback,
                                                     gpointer user_data);
    gboolean (*disable_unsolicited_registration_events_finish) (MMIfaceModem3gpp *self,
                                                                GAsyncResult *res,
                                                                GError **error);

    /* Asynchronous initial default EPS bearer loading */
    void                 (*load_initial_eps_bearer)        (MMIfaceModem3gpp     *self,
                                                            GAsyncReadyCallback   callback,
                                                            gpointer              user_data);
    MMBearerProperties * (*load_initial_eps_bearer_finish) (MMIfaceModem3gpp     *self,
                                                            GAsyncResult         *res,
                                                            GError              **error);

    /* Asynchronous initial default EPS bearer settings loading */
    void                 (*load_initial_eps_bearer_settings)        (MMIfaceModem3gpp     *self,
                                                                     GAsyncReadyCallback   callback,
                                                                     gpointer              user_data);
    MMBearerProperties * (*load_initial_eps_bearer_settings_finish) (MMIfaceModem3gpp     *self,
                                                                     GAsyncResult         *res,
                                                                     GError              **error);

    /* Asynchronous 5GNR registration settings loading */
    void                         (*load_nr5g_registration_settings)        (MMIfaceModem3gpp     *self,
                                                                            GAsyncReadyCallback   callback,
                                                                            gpointer              user_data);
    MMNr5gRegistrationSettings * (*load_nr5g_registration_settings_finish) (MMIfaceModem3gpp     *self,
                                                                            GAsyncResult         *res,
                                                                            GError              **error);

    /* Create initial default EPS bearer object */
    MMBaseBearer * (*create_initial_eps_bearer) (MMIfaceModem3gpp   *self,
                                                 MMBearerProperties *properties);

    /* Run CS/PS/EPS/5GS registration state checks..
     * Note that no registration state is returned, implementations should call
     * mm_iface_modem_3gpp_update_registration_state(). */
    void (* run_registration_checks) (MMIfaceModem3gpp *self,
                                      gboolean is_cs_supported,
                                      gboolean is_ps_supported,
                                      gboolean is_eps_supported,
                                      gboolean is_5gs_supported,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data);
    gboolean (*run_registration_checks_finish) (MMIfaceModem3gpp *self,
                                                GAsyncResult *res,
                                                GError **error);

    /* Try to register in the network */
    void (* register_in_network) (MMIfaceModem3gpp *self,
                                  const gchar *operator_id,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data);
    gboolean (*register_in_network_finish) (MMIfaceModem3gpp *self,
                                            GAsyncResult *res,
                                            GError **error);

    /* Loading of the Operator Code property */
    void (*load_operator_code) (MMIfaceModem3gpp *self,
                                GAsyncReadyCallback callback,
                                gpointer user_data);
    gchar * (*load_operator_code_finish) (MMIfaceModem3gpp *self,
                                          GAsyncResult *res,
                                          GError **error);

    /* Loading of the Operator Name property */
    void (*load_operator_name) (MMIfaceModem3gpp *self,
                                GAsyncReadyCallback callback,
                                gpointer user_data);
    gchar * (*load_operator_name_finish) (MMIfaceModem3gpp *self,
                                          GAsyncResult *res,
                                          GError **error);

    /* Scan current networks, expect a GList of MMModem3gppNetworkInfo */
    void (* scan_networks) (MMIfaceModem3gpp *self,
                            GAsyncReadyCallback callback,
                            gpointer user_data);
    GList * (*scan_networks_finish) (MMIfaceModem3gpp *self,
                                     GAsyncResult *res,
                                     GError **error);

    /* Set UE mode of operation for EPS */
    void     (* set_eps_ue_mode_operation)        (MMIfaceModem3gpp               *self,
                                                   MMModem3gppEpsUeModeOperation   mode,
                                                   GAsyncReadyCallback             callback,
                                                   gpointer                        user_data);
    gboolean (* set_eps_ue_mode_operation_finish) (MMIfaceModem3gpp               *self,
                                                   GAsyncResult                   *res,
                                                   GError                        **error);

    /* Set initial EPS bearer settings */
    void     (* set_initial_eps_bearer_settings)        (MMIfaceModem3gpp     *self,
                                                         MMBearerProperties   *properties,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
    gboolean (* set_initial_eps_bearer_settings_finish) (MMIfaceModem3gpp     *self,
                                                         GAsyncResult         *res,
                                                         GError              **error);

    /* Remove modem personalization */
    void     (* disable_facility_lock) (MMIfaceModem3gpp         *self,
                                        MMModem3gppFacility       facility,
                                        guint8                    slot,
                                        const gchar              *control_key,
                                        GAsyncReadyCallback       callback,
                                        gpointer                  user_data);
    gboolean (* disable_facility_lock_finish) (MMIfaceModem3gpp  *self,
                                               GAsyncResult      *res,
                                               GError           **error);

    /* Set Packet service */
    void     (*set_packet_service_state)        (MMIfaceModem3gpp              *self,
                                                 MMModem3gppPacketServiceState  state,
                                                 GAsyncReadyCallback            callback,
                                                 gpointer                       user_data);
    gboolean (*set_packet_service_state_finish) (MMIfaceModem3gpp              *self,
                                                 GAsyncResult                  *res,
                                                 GError                       **error);

    /* Set 5GNR registration settings */
    void     (* set_nr5g_registration_settings)        (MMIfaceModem3gpp             *self,
                                                        MMNr5gRegistrationSettings   *settings,
                                                        GAsyncReadyCallback           callback,
                                                        gpointer                      user_data);
    gboolean (* set_nr5g_registration_settings_finish) (MMIfaceModem3gpp             *self,
                                                        GAsyncResult                 *res,
                                                        GError                      **error);

    /* Set carrier lock */
    void     (* set_carrier_lock)       (MMIfaceModem3gpp    *self,
                                         const guint8        *data,
                                         gsize                data_size,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data);
    gboolean (*set_carrier_lock_finish) (MMIfaceModem3gpp    *self,
                                         GAsyncResult        *res,
                                         GError             **error);
};

/* Initialize Modem 3GPP interface (async) */
void     mm_iface_modem_3gpp_initialize        (MMIfaceModem3gpp *self,
                                                GCancellable *cancellable,
                                                GAsyncReadyCallback callback,
                                                gpointer user_data);
gboolean mm_iface_modem_3gpp_initialize_finish (MMIfaceModem3gpp *self,
                                                GAsyncResult *res,
                                                GError **error);

/* Enable Modem interface (async) */
void     mm_iface_modem_3gpp_enable        (MMIfaceModem3gpp *self,
                                            GCancellable *cancellable,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data);
gboolean mm_iface_modem_3gpp_enable_finish (MMIfaceModem3gpp *self,
                                            GAsyncResult *res,
                                            GError **error);

/* Disable Modem interface (async) */
void     mm_iface_modem_3gpp_disable        (MMIfaceModem3gpp *self,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data);
gboolean mm_iface_modem_3gpp_disable_finish (MMIfaceModem3gpp *self,
                                             GAsyncResult *res,
                                             GError **error);

#if defined WITH_SUSPEND_RESUME

/* Sync 3GPP interface (async) */
void     mm_iface_modem_3gpp_sync           (MMIfaceModem3gpp *self,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data);
gboolean mm_iface_modem_3gpp_sync_finish    (MMIfaceModem3gpp *self,
                                             GAsyncResult *res,
                                             GError **error);

void     mm_iface_modem_3gpp_terse           (MMIfaceModem3gpp *self,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data);
gboolean mm_iface_modem_3gpp_terse_finish    (MMIfaceModem3gpp *self,
                                             GAsyncResult *res,
                                             GError **error);

#endif

/* Shutdown Modem 3GPP interface */
void mm_iface_modem_3gpp_shutdown (MMIfaceModem3gpp *self);

/* Helpers to query manual operator id */
gchar *mm_iface_modem_3gpp_get_manual_registration_operator_id (MMIfaceModem3gpp *self);

/* Objects implementing this interface can report new registration info,
 * access technologies and location.
 *
 * This may happen when handling unsolicited registration messages, or when
 * the interface asks to run registration state checks.
 *
 * The registration updates may be "deferred" so that they are applied all at
 * the same time.
 */
void mm_iface_modem_3gpp_update_cs_registration_state      (MMIfaceModem3gpp             *self,
                                                            MMModem3gppRegistrationState  state,
                                                            gboolean                      deferred);
void mm_iface_modem_3gpp_update_ps_registration_state      (MMIfaceModem3gpp             *self,
                                                            MMModem3gppRegistrationState  state,
                                                            gboolean                      deferred);
void mm_iface_modem_3gpp_update_eps_registration_state     (MMIfaceModem3gpp             *self,
                                                            MMModem3gppRegistrationState  state,
                                                            gboolean                      deferred);
void mm_iface_modem_3gpp_update_5gs_registration_state     (MMIfaceModem3gpp             *self,
                                                            MMModem3gppRegistrationState  state,
                                                            gboolean                      deferred);
void mm_iface_modem_3gpp_apply_deferred_registration_state (MMIfaceModem3gpp             *self);

void mm_iface_modem_3gpp_update_packet_service_state (MMIfaceModem3gpp              *self,
                                                      MMModem3gppPacketServiceState  state);

void mm_iface_modem_3gpp_update_subscription_state (MMIfaceModem3gpp *self,
                                                    MMModem3gppSubscriptionState state);
void mm_iface_modem_3gpp_update_access_technologies (MMIfaceModem3gpp *self,
                                                     MMModemAccessTechnology access_tech);
void mm_iface_modem_3gpp_update_location            (MMIfaceModem3gpp *self,
                                                     gulong location_area_code,
                                                     gulong tracking_area_code,
                                                     gulong cell_id);
void mm_iface_modem_3gpp_update_pco_list            (MMIfaceModem3gpp *self,
                                                     const GList *pco_list);
void mm_iface_modem_3gpp_update_initial_eps_bearer  (MMIfaceModem3gpp *self,
                                                     MMBearerProperties *properties);
void mm_iface_modem_3gpp_reload_initial_eps_bearer  (MMIfaceModem3gpp *self);

void mm_iface_modem_3gpp_update_network_rejection   (MMIfaceModem3gpp       *self,
                                                     MMNetworkError          error,
                                                     const gchar            *operator_code,
                                                     const gchar            *operator_name,
                                                     MMModemAccessTechnology access_technology);

/* Run all registration checks */
void mm_iface_modem_3gpp_run_registration_checks (MMIfaceModem3gpp *self,
                                                  GAsyncReadyCallback callback,
                                                  gpointer user_data);
gboolean mm_iface_modem_3gpp_run_registration_checks_finish (MMIfaceModem3gpp *self,
                                                             GAsyncResult *res,
                                                             GError **error);

/* Request to reload current registration information */
void     mm_iface_modem_3gpp_reload_current_registration_info        (MMIfaceModem3gpp *self,
                                                                      GAsyncReadyCallback callback,
                                                                      gpointer user_data);
gboolean mm_iface_modem_3gpp_reload_current_registration_info_finish (MMIfaceModem3gpp *self,
                                                                      GAsyncResult *res,
                                                                      GError **error);
void     mm_iface_modem_3gpp_clear_current_operator                  (MMIfaceModem3gpp *self);

/* Allow registering in the network */
void     mm_iface_modem_3gpp_register_in_network        (MMIfaceModem3gpp    *self,
                                                         const gchar         *operator_id,
                                                         gboolean             force_registration,
                                                         guint                max_registration_time,
                                                         GAsyncReadyCallback  callback,
                                                         gpointer             user_data);
gboolean mm_iface_modem_3gpp_register_in_network_finish (MMIfaceModem3gpp    *self,
                                                         GAsyncResult        *res,
                                                         GError             **error);

/* Allow re-registering in the network with last settings */
void     mm_iface_modem_3gpp_reregister_in_network        (MMIfaceModem3gpp     *self,
                                                           GAsyncReadyCallback   callback,
                                                           gpointer              user_data);
gboolean mm_iface_modem_3gpp_reregister_in_network_finish (MMIfaceModem3gpp     *self,
                                                           GAsyncResult         *res,
                                                           GError              **error);

/* Allow requesting packet service explicitly */
void     mm_iface_modem_3gpp_set_packet_service_state        (MMIfaceModem3gpp              *self,
                                                              MMModem3gppPacketServiceState  packet_service_state,
                                                              GAsyncReadyCallback            callback,
                                                              gpointer                       user_data);
gboolean mm_iface_modem_3gpp_set_packet_service_state_finish (MMIfaceModem3gpp              *self,
                                                              GAsyncResult                  *res,
                                                              GError                       **error);

/* Allow waiting for packet service */
void                          mm_iface_modem_3gpp_wait_for_packet_service_state        (MMIfaceModem3gpp              *self,
                                                                                        MMModem3gppPacketServiceState  final_state,
                                                                                        GCancellable                  *cancellable,
                                                                                        GAsyncReadyCallback            callback,
                                                                                        gpointer                       user_data);
MMModem3gppPacketServiceState mm_iface_modem_3gpp_wait_for_packet_service_state_finish (MMIfaceModem3gpp  *self,
                                                                                        GAsyncResult      *res,
                                                                                        GError           **error);

/* Bind properties for simple GetStatus() */
void mm_iface_modem_3gpp_bind_simple_status (MMIfaceModem3gpp *self,
                                             MMSimpleStatus *status);

#endif /* MM_IFACE_MODEM_3GPP_H */
