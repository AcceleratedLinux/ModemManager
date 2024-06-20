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
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>

#include <ModemManager.h>
#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-broadband-bearer-unitac.h"
#include "mm-base-modem-at.h"
#include "mm-log-object.h"
#include "mm-modem-helpers.h"
#include "mm-modem-helpers-unitac.h"

G_DEFINE_TYPE (MMBroadbandBearerUnitac, mm_broadband_bearer_unitac, MM_TYPE_BROADBAND_BEARER)

static void dial_3gpp_context_step (GTask *task);

typedef enum {
    DIAL_3GPP_STEP_FIRST,
    DIAL_3GPP_STEP_PS_ATTACH,
    DIAL_3GPP_STEP_PS_ATTACH_QUERY,
    DIAL_3GPP_STEP_CONNECT_QUERY,
    DIAL_3GPP_STEP_LAST
} Dial3gppStep;

/*****************************************************************************/
/* Common connection context and task */

typedef struct {
    MMBroadbandModem    *modem;
    MMPortSerialAt      *primary;
    MMPort              *data;
    guint               cid;
    gboolean            auth_required;
    Dial3gppStep        step;
    MMBearerIpFamily    family;
    MMBearerIpConfig    *ipv4_config; /* For IPv4 settings */
    MMBearerIpConfig    *ipv6_config; /* For IPv6 settings */
} CommonConnectContext;

static void
common_connect_context_free (CommonConnectContext *ctx)
{
    if (ctx->ipv4_config)
        g_object_unref (ctx->ipv4_config);
    if (ctx->ipv6_config)
        g_object_unref (ctx->ipv6_config);
    if (ctx->data)
        g_object_unref (ctx->data);
    g_object_unref (ctx->modem);
    g_object_unref (ctx->primary);
    g_slice_free (CommonConnectContext, ctx);
}

static GTask *
common_connect_task_new (MMBroadbandBearerUnitac *self,
                         MMBroadbandModem        *modem,
                         MMPortSerialAt          *primary,
                         guint                   cid,
                         MMPort                  *data,
                         GCancellable            *cancellable,
                         GAsyncReadyCallback     callback,
                         gpointer                user_data)
{
    CommonConnectContext *ctx;
    GTask                *task;

    ctx = g_slice_new0 (CommonConnectContext);
    ctx->modem   = g_object_ref (modem);
    ctx->primary = g_object_ref (primary);
    ctx->cid     = cid;
    ctx->step = DIAL_3GPP_STEP_FIRST;
    ctx->ipv4_config = NULL;
    ctx->ipv6_config = NULL;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_task_data (task, ctx, (GDestroyNotify) common_connect_context_free);

    /* We need a net data port */
    if (data)
        ctx->data = g_object_ref (data);
    else {
        ctx->data = mm_base_modem_get_best_data_port (MM_BASE_MODEM (modem), MM_PORT_TYPE_NET);
        if (!ctx->data) {
            g_task_return_new_error (task,
                                     MM_CORE_ERROR,
                                     MM_CORE_ERROR_NOT_FOUND,
                                     "No valid data port found to launch connection");
            g_object_unref (task);
            return NULL;
        }
    }

    return task;
}

/*****************************************************************************/
/* 3GPP IP config (sub-step of the 3GPP Connection sequence) */

static gboolean
get_ip_config_3gpp_finish (MMBroadbandBearer *self,
                           GAsyncResult      *res,
                           MMBearerIpConfig  **ipv4_config,
                           MMBearerIpConfig  **ipv6_config,
                           GError            **error)
{
    MMBearerConnectResult *configs;
    MMBearerIpConfig      *ipv4, *ipv6;

    configs = g_task_propagate_pointer (G_TASK (res), error);
    if (!configs)
        return FALSE;

    ipv4 = mm_bearer_connect_result_peek_ipv4_config (configs);
    ipv6 = mm_bearer_connect_result_peek_ipv6_config (configs);
    g_assert (ipv4 || ipv6);
    if (ipv4_config && ipv4)
        *ipv4_config = g_object_ref (ipv4);
    if (ipv6_config && ipv6)
        *ipv6_config = g_object_ref (ipv6);
    mm_bearer_connect_result_unref (configs);
    return TRUE;
}

static void
complete_get_ip_config_3gpp (GTask *task)
{
    CommonConnectContext  *ctx;
    MMBearerConnectResult *connect_result;

    ctx = g_task_get_task_data (task);

    g_assert (ctx->ipv4_config || ctx->ipv6_config);

    connect_result = mm_bearer_connect_result_new(ctx->data, ctx->ipv4_config, ctx->ipv6_config);

    g_task_return_pointer (task,
                           connect_result,
                           (GDestroyNotify) mm_bearer_connect_result_unref);
    g_object_unref (task);
}

static void
cgcontrdp_ready (MMBaseModem  *modem,
                 GAsyncResult *res,
                 GTask        *task)
{
    MMBroadbandBearerUnitac *self;
    const gchar             *response;
    GError                  *error = NULL;
    CommonConnectContext    *ctx;
    gchar                   *ipv4_address = NULL;
    gchar                   *ipv4_subnet = NULL;
    gchar                   *ipv4_gateway = NULL;
    gchar                   *ipv4_dns_addresses[3] = { NULL, NULL, NULL };
    guint                   ipv4_mtu = 0;
    gchar                   *ipv6_address = NULL;
    guint                   ipv6_subnet;
    gchar                   *ipv6_gateway = NULL;
    gchar                   *ipv6_dns_addresses[3] = { NULL, NULL, NULL };
    guint                   ipv6_mtu = 0;

    self = g_task_get_source_object (task);
    ctx  = g_task_get_task_data (task);

    response = mm_base_modem_at_command_finish (modem, res, &error);

    if (!response || !mm_3gpp_cust_parse_cgcontrdp_response (response,
                                                             &ipv4_address,
                                                             &ipv4_subnet,
                                                             &ipv4_gateway,
                                                             &ipv4_dns_addresses[0],
                                                             &ipv4_dns_addresses[1],
                                                             &ipv4_mtu,
                                                             &ipv6_address,
                                                             &ipv6_subnet,
                                                             &ipv6_gateway,
                                                             &ipv6_dns_addresses[0],
                                                             &ipv6_dns_addresses[1],
                                                             &ipv6_mtu,
                                                             &error)) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    if (ctx->family & MM_BEARER_IP_FAMILY_IPV4 || ctx->family & MM_BEARER_IP_FAMILY_IPV4V6) {
        if (ipv4_address && ipv4_subnet) {

            mm_obj_dbg (self, "IPv4 address retrieved: %s", ipv4_address);
            mm_obj_dbg (self, "IPv4 subnet retrieved: %s", ipv4_subnet);
            mm_bearer_ip_config_set_address (ctx->ipv4_config, ipv4_address);
            mm_bearer_ip_config_set_prefix (ctx->ipv4_config, mm_netmask_to_cidr (ipv4_subnet));

            if (ipv4_gateway) {
                mm_obj_dbg(self, "IPv4 gateway retrieved: %s", ipv4_gateway);
                mm_bearer_ip_config_set_gateway (ctx->ipv4_config, ipv4_gateway);
            }
            if (ipv4_dns_addresses[0])
                mm_obj_dbg (self, "IPv4 primary DNS retrieved: %s", ipv4_dns_addresses[0]);
            if (ipv4_dns_addresses[1])
                mm_obj_dbg (self, "IPv4 secondary DNS retrieved: %s", ipv4_dns_addresses[1]);

            if (ipv4_dns_addresses[0] || ipv4_dns_addresses[1])
                mm_bearer_ip_config_set_dns (ctx->ipv4_config, (const gchar **) ipv4_dns_addresses);

            mm_obj_dbg (self, "IPv4 MTU: %d", ipv4_mtu);
            if (ipv4_mtu != 0)
                mm_bearer_ip_config_set_mtu(ctx->ipv4_config, ipv4_mtu);
        }
    }

    if (ctx->family & MM_BEARER_IP_FAMILY_IPV6 || ctx->family & MM_BEARER_IP_FAMILY_IPV4V6) {
        if (ipv6_address && ipv6_subnet) {

            mm_obj_dbg(self, "IPv6 address retrieved: %s", ipv6_address);
            mm_obj_dbg(self, "IPv6 subnet retrieved: %d", ipv6_subnet);
            mm_bearer_ip_config_set_address(ctx->ipv6_config, ipv6_address);
            mm_bearer_ip_config_set_prefix(ctx->ipv6_config, ipv6_subnet);

            if (ipv6_gateway) {
                mm_obj_dbg(self, "IPv6 gateway retrieved: %s", ipv6_gateway);
                mm_bearer_ip_config_set_gateway(ctx->ipv6_config, ipv6_gateway);
            }
            if (ipv6_dns_addresses[0])
                mm_obj_dbg(self, "IPv6 primary DNS retrieved: %s", ipv6_dns_addresses[0]);
            if (ipv6_dns_addresses[1])
                mm_obj_dbg(self, "IPv6 secondary DNS retrieved: %s", ipv6_dns_addresses[1]);

            if (ipv6_dns_addresses[0] || ipv6_dns_addresses[1])
                mm_bearer_ip_config_set_dns(ctx->ipv6_config, (const gchar **) ipv6_dns_addresses);

            if (ipv6_mtu != 0) {
                mm_obj_dbg(self, "IPv6 MTU: %d", ipv6_mtu);
                mm_bearer_ip_config_set_mtu(ctx->ipv6_config, ipv6_mtu);
            }
            else if (ipv4_mtu != 0) {
                mm_obj_dbg(self, "No IPv6 MTU. Using IPv4 MTU for IPv6: %d", ipv4_mtu);
                mm_bearer_ip_config_set_mtu(ctx->ipv6_config, ipv4_mtu);
            }
            else
                mm_obj_dbg (self, "No IPv6 MTU");
        }
    }

    g_free (ipv4_address);
    g_free (ipv4_subnet);
    g_free (ipv4_gateway);
    g_free (ipv4_dns_addresses[0]);
    g_free (ipv4_dns_addresses[1]);
    g_free (ipv6_address);
    g_free (ipv6_gateway);
    g_free (ipv6_dns_addresses[0]);
    g_free (ipv6_dns_addresses[1]);

    mm_obj_dbg (self, "finished IP settings retrieval for PDP context #%u...", ctx->cid);
    complete_get_ip_config_3gpp (task);
}

static void
cgpiaf_ready (MMBaseModem  *modem,
              GAsyncResult *res,
              GTask        *task)
{
    MMBroadbandBearerUnitac *self;
    GError                  *error = NULL;
    CommonConnectContext    *ctx;

    self = g_task_get_source_object (task);
    ctx  = g_task_get_task_data (task);

    if (!mm_base_modem_at_command_finish (modem, res, &error)) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    // Strange seems like the modem needs some delay before the IPv6 address show up
    sleep(8);
    gchar *cmd = g_strdup_printf ("+CGCONTRDP=%u", ctx->cid);
    mm_obj_dbg (self, "gathering connection details for PDP context #%u...", ctx->cid);
    mm_base_modem_at_command (MM_BASE_MODEM (modem),
                              cmd,
                              10,
                              FALSE,
                              (GAsyncReadyCallback) cgcontrdp_ready,
                              task);

    g_free (cmd);
}

static void
cgpaddr_ready (MMBaseModem  *modem,
               GAsyncResult *res,
               GTask        *task)
{
    MMBroadbandBearerUnitac *self;
    const gchar             *response;
    GError                  *error = NULL;
    gchar                   *ipv4_address = NULL;
    gchar                   *ipv6_address = NULL;

    self = g_task_get_source_object (task);

    response = mm_base_modem_at_command_finish (modem, res, &error);

    if (!response || !mm_unitac_parse_cgpaddr_response (response,
                                                        &ipv4_address,
                                                        &ipv6_address,
                                                        &error)) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    mm_obj_dbg(self, "IPv4 address retrieved: %s", ipv4_address ? ipv4_address : "");
    mm_obj_dbg(self, "IPv6 address retrieved: %s", ipv6_address ? ipv6_address : "");

    g_free (ipv4_address);
    g_free (ipv6_address);

    mm_base_modem_at_command (MM_BASE_MODEM (modem),
                              "+CGPIAF=1,0,1,1",
                              10,
                              FALSE,
                              (GAsyncReadyCallback) cgpiaf_ready,
                              task);

}

static void
get_ip_config_3gpp (MMBroadbandBearer   *_self,
                    MMBroadbandModem    *modem,
                    MMPortSerialAt      *primary,
                    MMPortSerialAt      *secondary,
                    MMPort              *data,
                    guint               cid,
                    MMBearerIpFamily    ip_family,
                    GAsyncReadyCallback callback,
                    gpointer            user_data)
{
    MMBroadbandBearerUnitac *self = MM_BROADBAND_BEARER_UNITAC (_self);
    GTask                   *task;
    CommonConnectContext    *ctx;

    if (!(task = common_connect_task_new (MM_BROADBAND_BEARER_UNITAC (self),
                                          MM_BROADBAND_MODEM (modem),
                                          primary,
                                          cid,
                                          data,
                                          NULL,
                                          callback,
                                          user_data)))
        return;

    ctx = g_task_get_task_data (task);
    ctx->family = ip_family;

    if (ctx->family & MM_BEARER_IP_FAMILY_IPV4 || ctx->family & MM_BEARER_IP_FAMILY_IPV4V6) {
        ctx->ipv4_config = mm_bearer_ip_config_new ();
        mm_bearer_ip_config_set_method (ctx->ipv4_config, MM_BEARER_IP_METHOD_DHCP);
    }
    if (ctx->family & MM_BEARER_IP_FAMILY_IPV6 || ctx->family & MM_BEARER_IP_FAMILY_IPV4V6) {
        ctx->ipv6_config = mm_bearer_ip_config_new ();
        mm_bearer_ip_config_set_method (ctx->ipv6_config, MM_BEARER_IP_METHOD_DHCP);
    }

    gchar *cmd;

    cmd = g_strdup_printf ("+CGPADDR=%u", cid);
    mm_obj_dbg (self, "gathering IP-Address information for PDP context #%u...", cid);
    mm_base_modem_at_command (MM_BASE_MODEM (modem),
                              cmd,
                              10,
                              FALSE,
                              (GAsyncReadyCallback) cgpaddr_ready,
                              task);
    g_free (cmd);

    return;
}

/*****************************************************************************/
/* 3GPP Dialing (sub-step of the 3GPP Connection sequence) */

static MMPort *
dial_3gpp_finish (MMBroadbandBearer *self,
                  GAsyncResult      *res,
                  GError            **error)
{
    return MM_PORT (g_task_propagate_pointer (G_TASK (res), error));
}

static void
cgatt_query_ready (MMBaseModem  *modem,
                   GAsyncResult *res,
                   GTask        *task)
{
    MMBroadbandBearerUnitac *self;
    CommonConnectContext     *ctx;
    const gchar             *response;
    GError                  *error = NULL;
    guint                   attach = 0;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    response = mm_base_modem_at_command_finish (modem, res, &error);

    if (!response || !mm_unitac_parse_cgatt_response (response,
                                                      &attach,
                                                      &error)) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    mm_obj_dbg(self, "PS Attach: %d", attach);

    if (attach != 1)
        g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                                 "PS detached...");

    /* Go on */
    ctx->step++;
    dial_3gpp_context_step (task);
}

static void
cgact_query_ready (MMBaseModem  *modem,
                   GAsyncResult *res,
                   GTask        *task)
{
    MMBroadbandBearerUnitac  *self;
    CommonConnectContext     *ctx;
    const gchar              *response;
    GError                   *error           = NULL;
    GList                    *pdp_active_list = NULL;
    GList                    *l;
    MMBearerConnectionStatus status           = MM_BEARER_CONNECTION_STATUS_UNKNOWN;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    response = mm_base_modem_at_command_full_finish (modem, res, &error);
    if (response)
        pdp_active_list = mm_3gpp_parse_cgact_read_response (response, &error);

    if (error) {
        g_assert (!pdp_active_list);
        g_prefix_error (&error, "couldn't check current list of active PDP contexts: ");
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    for (l = pdp_active_list; l; l = g_list_next (l)) {
        MM3gppPdpContextActive *pdp_active;

        /* We look for he just assume the first active PDP context found is the one we're
         * looking for. */
        pdp_active = (MM3gppPdpContextActive *)(l->data);
        if (pdp_active->cid == ctx->cid) {
            status = (pdp_active->active ? MM_BEARER_CONNECTION_STATUS_CONNECTED : MM_BEARER_CONNECTION_STATUS_DISCONNECTED);
            break;
        }
    }
    mm_3gpp_pdp_context_active_list_free (pdp_active_list);

    /* PDP context not found? This shouldn't happen, error out */
    if (status == MM_BEARER_CONNECTION_STATUS_UNKNOWN)
        g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                                 "PDP context not found in the known contexts list");
    else
        mm_obj_dbg (self, "active PDP context found : %u", ctx->cid);

    /* Go on */
    ctx->step++;
    dial_3gpp_context_step (task);
}

static void
cgatt_attach_ready (MMBaseModem  *modem,
                    GAsyncResult *res,
                    GTask        *task)
{
    CommonConnectContext *ctx;
    GError               *error = NULL;

    ctx = g_task_get_task_data (task);

    if (!mm_base_modem_at_command_full_finish (modem, res, &error)) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Go on */
    ctx->step++;
    dial_3gpp_context_step (task);
}

static void
dial_3gpp_context_step (GTask *task)
{
    CommonConnectContext    *ctx;

    ctx = g_task_get_task_data (task);

    if (g_task_return_error_if_cancelled (task)) {
        g_object_unref (task);
        return;
    }

    switch (ctx->step) {
        case DIAL_3GPP_STEP_FIRST:
            ctx->step++;
            /* fall through */

        case DIAL_3GPP_STEP_PS_ATTACH:
            mm_base_modem_at_command_full (MM_BASE_MODEM(ctx->modem),
                                           ctx->primary,
                                           "+CGATT=1",
                                           20,
                                           FALSE,
                                           FALSE, /* raw */
                                           NULL, /* cancellable */
                                           (GAsyncReadyCallback)cgatt_attach_ready,
                                           task);
            return;

        case DIAL_3GPP_STEP_PS_ATTACH_QUERY:
            mm_base_modem_at_command_full(MM_BASE_MODEM(ctx->modem),
                                          ctx->primary,
                                          "+CGATT?",
                                          10,
                                          FALSE,
                                          FALSE, /* raw */
                                          NULL, /* cancellable */
                                          (GAsyncReadyCallback) cgatt_query_ready,
                                          task);
            return;

        case DIAL_3GPP_STEP_CONNECT_QUERY:
            mm_base_modem_at_command_full(MM_BASE_MODEM(ctx->modem),
                                          ctx->primary,
                                          "+CGACT?",
                                          10,
                                          FALSE,
                                          FALSE, /* raw */
                                          NULL, /* cancellable */
                                          (GAsyncReadyCallback) cgact_query_ready,
                                          task);
            return;

        case DIAL_3GPP_STEP_LAST:
            g_task_return_pointer (task,
                                   g_object_ref (ctx->data),
                                   g_object_unref);
            g_object_unref (task);
            return;

        default:
            g_assert_not_reached ();
    }
}

static void
dial_3gpp (MMBroadbandBearer   *self,
           MMBaseModem         *modem,
           MMPortSerialAt      *primary,
           guint               cid,
           GCancellable        *cancellable,
           GAsyncReadyCallback callback,
           gpointer            user_data)
{
    GTask *task;

    if (!(task = common_connect_task_new (MM_BROADBAND_BEARER_UNITAC (self),
                                          MM_BROADBAND_MODEM (modem),
                                          primary,
                                          cid,
                                          NULL, /* data, unused */
                                          cancellable,
                                          callback,
                                          user_data)))
        return;

    dial_3gpp_context_step (task);
}

/*****************************************************************************/
/* 3GPP disconnection */

static gboolean
disconnect_3gpp_finish (MMBroadbandBearer *self,
                        GAsyncResult      *res,
                        GError            **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cgact_deactivate_ready (MMBaseModem  *modem,
                        GAsyncResult *res,
                        GTask        *task)
{
    MMBroadbandBearerUnitac *self;
    const gchar             *response;
    GError                  *error = NULL;

    self = g_task_get_source_object (task);

    response = mm_base_modem_at_command_finish (modem, res, &error);
    if (!response) {
        if (!g_error_matches (error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_GPRS_UNKNOWN) &&
            !g_error_matches (error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_GPRS_LAST_PDN_DISCONNECTION_NOT_ALLOWED) &&
            !g_error_matches (error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_GPRS_LAST_PDN_DISCONNECTION_NOT_ALLOWED_LEGACY)) {
            g_task_return_error (task, error);
            g_object_unref (task);
            return;
        }

        mm_obj_dbg (self, "ignored error when disconnecting last LTE bearer: %s", error->message);
        g_clear_error (&error);
    }

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
disconnect_3gpp  (MMBroadbandBearer   *self,
                  MMBroadbandModem    *modem,
                  MMPortSerialAt      *primary,
                  MMPortSerialAt      *secondary,
                  MMPort              *data,
                  guint               cid,
                  GAsyncReadyCallback callback,
                  gpointer            user_data)
{
    GTask            *task;
    g_autofree gchar *cmd = NULL;

    if (!(task = common_connect_task_new (MM_BROADBAND_BEARER_UNITAC (self),
                                          MM_BROADBAND_MODEM (modem),
                                          primary,
                                          cid,
                                          data,
                                          NULL,
                                          callback,
                                          user_data)))
        return;

    // PDP Context Deactivate
    cmd = g_strdup_printf ("+CGACT=0,%u", cid);
    mm_obj_dbg (self, "deactivating PDP context #%u...", cid);
    mm_base_modem_at_command (MM_BASE_MODEM (modem),
                              cmd,
                              120,
                              FALSE,
                              (GAsyncReadyCallback) cgact_deactivate_ready,
                              task);
}

/*****************************************************************************/

MMBaseBearer *
mm_broadband_bearer_unitac_new_finish (GAsyncResult *res,
                                       GError       **error)
{
    GObject *source;
    GObject *bearer;

    source = g_async_result_get_source_object (res);
    bearer = g_async_initable_new_finish (G_ASYNC_INITABLE (source), res, error);
    g_object_unref (source);

    if (!bearer)
        return NULL;

    /* Only export valid bearers */
    mm_base_bearer_export (MM_BASE_BEARER (bearer));

    return MM_BASE_BEARER (bearer);
}

void
mm_broadband_bearer_unitac_new (MMBroadbandModem    *modem,
                                MMBearerProperties  *config,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer            user_data)
{

    g_async_initable_new_async (
            MM_TYPE_BROADBAND_BEARER_UNITAC,
            G_PRIORITY_DEFAULT,
            cancellable,
            callback,
            user_data,
            MM_BASE_BEARER_MODEM, modem,
            MM_BASE_BEARER_CONFIG, config,
            NULL);
}

static void
mm_broadband_bearer_unitac_init (MMBroadbandBearerUnitac *self)
{
}

static void
mm_broadband_bearer_unitac_class_init (MMBroadbandBearerUnitacClass *klass)
{
    MMBroadbandBearerClass *broadband_bearer_class = MM_BROADBAND_BEARER_CLASS (klass);

    broadband_bearer_class->disconnect_3gpp           = disconnect_3gpp;
    broadband_bearer_class->disconnect_3gpp_finish    = disconnect_3gpp_finish;
    broadband_bearer_class->dial_3gpp                 = dial_3gpp;
    broadband_bearer_class->dial_3gpp_finish          = dial_3gpp_finish;
    broadband_bearer_class->get_ip_config_3gpp        = get_ip_config_3gpp;
    broadband_bearer_class->get_ip_config_3gpp_finish = get_ip_config_3gpp_finish;

}
