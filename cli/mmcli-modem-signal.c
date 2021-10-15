/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * mmcli -- Control modem status & access information from the command line
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2013 Aleksander Morgado <aleksander@gnu.org>
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#define _LIBMM_INSIDE_MMCLI
#include <libmm-glib.h>

#include "mmcli.h"
#include "mmcli-common.h"
#include "mmcli-output.h"

/* Context */
typedef struct {
    MMManager *manager;
    GCancellable *cancellable;
    MMObject *object;
    MMModemSignal *modem_signal;
} Context;
static Context *ctx;

/* Options */
static gboolean get_flag;
static gchar *setup_str;
static gchar *setup_threshold_str;

static GOptionEntry entries[] = {
    { "signal-setup-threshold", 0, 0, G_OPTION_ARG_STRING, &setup_threshold_str,
      "Setup threshold values for signal information retrieval",
      "rssi_threshold=<value>,error_rate_threshold=<value>",
    },
    { "signal-setup", 0, 0, G_OPTION_ARG_STRING, &setup_str,
      "Setup extended signal information retrieval",
      "[Rate]"
    },
    { "signal-get", 0, 0, G_OPTION_ARG_NONE, &get_flag,
      "Get all extended signal quality information",
      NULL
    },
    { NULL }
};

GOptionGroup *
mmcli_modem_signal_get_option_group (void)
{
    GOptionGroup *group;

    group = g_option_group_new ("signal",
                                "Signal options:",
                                "Show Signal options",
                                NULL,
                                NULL);
    g_option_group_add_entries (group, entries);

    return group;
}

gboolean
mmcli_modem_signal_options_enabled (void)
{
    static guint n_actions = 0;
    static gboolean checked = FALSE;

    if (checked)
        return !!n_actions;

    n_actions = (!!setup_threshold_str +
                 !!setup_str +
                 get_flag);

    if (n_actions > 1) {
        g_printerr ("error: too many Signal actions requested\n");
        exit (EXIT_FAILURE);
    }

    if (get_flag)
        mmcli_force_sync_operation ();

    checked = TRUE;
    return !!n_actions;
}

static void
context_free (void)
{
    if (!ctx)
        return;

    if (ctx->cancellable)
        g_object_unref (ctx->cancellable);
    if (ctx->modem_signal)
        g_object_unref (ctx->modem_signal);
    if (ctx->object)
        g_object_unref (ctx->object);
    if (ctx->manager)
        g_object_unref (ctx->manager);
    g_free (ctx);
}

static void
ensure_modem_signal (void)
{
    if (!ctx->modem_signal) {
        g_printerr ("error: modem has no extended signal capabilities\n");
        exit (EXIT_FAILURE);
    }

    /* Success */
}

void
mmcli_modem_signal_shutdown (void)
{
    context_free ();
}

static void
print_signal_info (void)
{
    MMSignal *signal;
    gdouble   value;
    guint     error_rate;
    gchar    *refresh_rate;
    gchar    *rssi_threshold;
    gchar    *error_rate_threshold;
    gchar    *cdma1x_rssi = NULL;
    gchar    *cdma1x_ecio = NULL;
    gchar    *cdma1x_error_rate = NULL;
    gchar    *evdo_rssi = NULL;
    gchar    *evdo_ecio = NULL;
    gchar    *evdo_sinr = NULL;
    gchar    *evdo_io = NULL;
    gchar    *evdo_error_rate = NULL;
    gchar    *gsm_rssi = NULL;
    gchar    *gsm_error_rate = NULL;
    gchar    *umts_rssi = NULL;
    gchar    *umts_rscp = NULL;
    gchar    *umts_ecio = NULL;
    gchar    *umts_error_rate = NULL;
    gchar    *lte_rssi = NULL;
    gchar    *lte_rsrp = NULL;
    gchar    *lte_rsrq = NULL;
    gchar    *lte_snr = NULL;
    gchar    *lte_error_rate = NULL;
    gchar    *nr5g_rsrp = NULL;
    gchar    *nr5g_rsrq = NULL;
    gchar    *nr5g_snr = NULL;
    gchar    *nr5g_error_rate = NULL;

    refresh_rate = g_strdup_printf ("%u", mm_modem_signal_get_rate (ctx->modem_signal));
    rssi_threshold = g_strdup_printf ("%u", mm_modem_signal_get_rssi_threshold (ctx->modem_signal));
    error_rate_threshold = g_strdup_printf ("%u", mm_modem_signal_get_error_rate_threshold (ctx->modem_signal));

    signal = mm_modem_signal_peek_cdma (ctx->modem_signal);
    if (signal) {
        if ((value = mm_signal_get_rssi (signal)) != MM_SIGNAL_UNKNOWN)
            cdma1x_rssi = g_strdup_printf ("%.2lf", value);
        if ((value = mm_signal_get_ecio (signal)) != MM_SIGNAL_UNKNOWN)
            cdma1x_ecio = g_strdup_printf ("%.2lf", value);
        if ((error_rate = mm_signal_get_error_rate (signal)) != MM_SIGNAL_UNKNOWN)
            cdma1x_error_rate = g_strdup_printf ("%u", error_rate);
    }

    signal = mm_modem_signal_peek_evdo (ctx->modem_signal);
    if (signal) {
        if ((value = mm_signal_get_rssi (signal)) != MM_SIGNAL_UNKNOWN)
            evdo_rssi = g_strdup_printf ("%.2lf", value);
        if ((value = mm_signal_get_ecio (signal)) != MM_SIGNAL_UNKNOWN)
            evdo_ecio = g_strdup_printf ("%.2lf", value);
        if ((value = mm_signal_get_sinr (signal)) != MM_SIGNAL_UNKNOWN)
            evdo_sinr = g_strdup_printf ("%.2lf", value);
        if ((value = mm_signal_get_io (signal)) != MM_SIGNAL_UNKNOWN)
            evdo_io = g_strdup_printf ("%.2lf", value);
        if ((error_rate = mm_signal_get_error_rate (signal)) != MM_SIGNAL_UNKNOWN)
            evdo_error_rate = g_strdup_printf ("%u", error_rate);
    }

    signal = mm_modem_signal_peek_gsm (ctx->modem_signal);
    if (signal) {
        if ((value = mm_signal_get_rssi (signal)) != MM_SIGNAL_UNKNOWN)
            gsm_rssi = g_strdup_printf ("%.2lf", value);
        if ((error_rate = mm_signal_get_error_rate (signal)) != MM_SIGNAL_UNKNOWN)
            gsm_error_rate = g_strdup_printf ("%u", error_rate);
    }

    signal = mm_modem_signal_peek_umts (ctx->modem_signal);
    if (signal) {
        if ((value = mm_signal_get_rssi (signal)) != MM_SIGNAL_UNKNOWN)
            umts_rssi = g_strdup_printf ("%.2lf", value);
        if ((value = mm_signal_get_rscp (signal)) != MM_SIGNAL_UNKNOWN)
            umts_rscp = g_strdup_printf ("%.2lf", value);
        if ((value = mm_signal_get_ecio (signal)) != MM_SIGNAL_UNKNOWN)
            umts_ecio = g_strdup_printf ("%.2lf", value);
        if ((error_rate = mm_signal_get_error_rate (signal)) != MM_SIGNAL_UNKNOWN)
            umts_error_rate = g_strdup_printf ("%u", error_rate);
    }

    signal = mm_modem_signal_peek_lte (ctx->modem_signal);
    if (signal) {
        if ((value = mm_signal_get_rssi (signal)) != MM_SIGNAL_UNKNOWN)
            lte_rssi = g_strdup_printf ("%.2lf", value);
        if ((value = mm_signal_get_rsrq (signal)) != MM_SIGNAL_UNKNOWN)
            lte_rsrq = g_strdup_printf ("%.2lf", value);
        if ((value = mm_signal_get_rsrp (signal)) != MM_SIGNAL_UNKNOWN)
            lte_rsrp = g_strdup_printf ("%.2lf", value);
        if ((value = mm_signal_get_snr (signal)) != MM_SIGNAL_UNKNOWN)
            lte_snr = g_strdup_printf ("%.2lf", value);
        if ((error_rate = mm_signal_get_error_rate (signal)) != MM_SIGNAL_UNKNOWN)
            lte_error_rate = g_strdup_printf ("%u", error_rate);
    }

    signal = mm_modem_signal_peek_nr5g (ctx->modem_signal);
    if (signal) {
        if ((value = mm_signal_get_rsrq (signal)) != MM_SIGNAL_UNKNOWN)
            nr5g_rsrq = g_strdup_printf ("%.2lf", value);
        if ((value = mm_signal_get_rsrp (signal)) != MM_SIGNAL_UNKNOWN)
            nr5g_rsrp = g_strdup_printf ("%.2lf", value);
        if ((value = mm_signal_get_snr (signal)) != MM_SIGNAL_UNKNOWN)
            nr5g_snr = g_strdup_printf ("%.2lf", value);
        if ((error_rate = mm_signal_get_error_rate (signal)) != MM_SIGNAL_UNKNOWN)
            nr5g_error_rate = g_strdup_printf ("%u", error_rate);
    }

    mmcli_output_string_take_typed (MMC_F_SIGNAL_REFRESH_RATE, refresh_rate, "seconds");
    mmcli_output_string_list_take (MMC_F_SIGNAL_RSSI_THRESHOLD, rssi_threshold);
    mmcli_output_string_list_take (MMC_F_SIGNAL_ERROR_RATE_THRESHOLD, error_rate_threshold);
    mmcli_output_string_take_typed (MMC_F_SIGNAL_CDMA1X_RSSI,  cdma1x_rssi,  "dBm");
    mmcli_output_string_take_typed (MMC_F_SIGNAL_CDMA1X_ECIO,  cdma1x_ecio,  "dBm");
    mmcli_output_string_list_take (MMC_F_SIGNAL_CDMA1X_ERROR_RATE, cdma1x_error_rate);
    mmcli_output_string_take_typed (MMC_F_SIGNAL_EVDO_RSSI,    evdo_rssi,    "dBm");
    mmcli_output_string_take_typed (MMC_F_SIGNAL_EVDO_ECIO,    evdo_ecio,    "dB");
    mmcli_output_string_take_typed (MMC_F_SIGNAL_EVDO_SINR,    evdo_sinr,    "dB");
    mmcli_output_string_take_typed (MMC_F_SIGNAL_EVDO_IO,      evdo_io,      "dBm");
    mmcli_output_string_list_take (MMC_F_SIGNAL_EVDO_ERROR_RATE, evdo_error_rate);
    mmcli_output_string_take_typed (MMC_F_SIGNAL_GSM_RSSI,     gsm_rssi,     "dBm");
    mmcli_output_string_list_take (MMC_F_SIGNAL_GSM_ERROR_RATE, gsm_error_rate);
    mmcli_output_string_take_typed (MMC_F_SIGNAL_UMTS_RSSI,    umts_rssi,    "dBm");
    mmcli_output_string_take_typed (MMC_F_SIGNAL_UMTS_RSCP,    umts_rscp,    "dBm");
    mmcli_output_string_take_typed (MMC_F_SIGNAL_UMTS_ECIO,    umts_ecio,    "dB");
    mmcli_output_string_list_take (MMC_F_SIGNAL_UMTS_ERROR_RATE, umts_error_rate);
    mmcli_output_string_take_typed (MMC_F_SIGNAL_LTE_RSSI,     lte_rssi,     "dBm");
    mmcli_output_string_take_typed (MMC_F_SIGNAL_LTE_RSRQ,     lte_rsrq,     "dB");
    mmcli_output_string_take_typed (MMC_F_SIGNAL_LTE_RSRP,     lte_rsrp,     "dBm");
    mmcli_output_string_take_typed (MMC_F_SIGNAL_LTE_SNR,      lte_snr,      "dB");
    mmcli_output_string_list_take (MMC_F_SIGNAL_LTE_ERROR_RATE, lte_error_rate);
    mmcli_output_string_take_typed (MMC_F_SIGNAL_5G_RSRQ,      nr5g_rsrq,    "dB");
    mmcli_output_string_take_typed (MMC_F_SIGNAL_5G_RSRP,      nr5g_rsrp,    "dBm");
    mmcli_output_string_take_typed (MMC_F_SIGNAL_5G_SNR,       nr5g_snr,     "dB");
    mmcli_output_string_list_take (MMC_F_SIGNAL_5G_ERROR_RATE, nr5g_error_rate);
    mmcli_output_dump ();
}

typedef struct {
    guint rssi_threshold;
    gboolean rssi_set;
    guint error_rate_threshold;
    gboolean error_rate_set;
    GError        *error;
} ParseKeyValueContext;

static gboolean
key_value_foreach (const gchar          *key,
                   const gchar          *value,
                   ParseKeyValueContext *parse_ctx)
{
    if (g_str_equal (key, "rssi_threshold")) {

        if (!mm_get_uint_from_str (value, &parse_ctx->rssi_threshold)) {
            g_set_error (&parse_ctx->error, MM_CORE_ERROR, MM_CORE_ERROR_INVALID_ARGS,
                         "invalid rssi threshold value given: %s", value);
            return FALSE;
        }
        parse_ctx->rssi_set = TRUE;
    } else if (g_str_equal (key, "error_rate_threshold")) {

        if (!mm_get_uint_from_str (value, &parse_ctx->error_rate_threshold)) {
            g_set_error (&parse_ctx->error, MM_CORE_ERROR, MM_CORE_ERROR_INVALID_ARGS,
                         "invalid error rate threshold value given: %s", value);
            return FALSE;
        }
        parse_ctx->error_rate_set = TRUE;
    } else {
        g_set_error (&parse_ctx->error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_UNSUPPORTED,
                     "Invalid properties string, unsupported key '%s'",
                     key);
        return FALSE;
    }
    return TRUE;
}

/**
 * mm_get_threshold_from_str: (skip)
 */
static GVariant *
mm_get_threshold_from_str (const gchar *str)
{
    ParseKeyValueContext parse_ctx;
    GVariantBuilder builder;

    parse_ctx.error = NULL;
    parse_ctx.rssi_set = FALSE;
    parse_ctx.error_rate_set = FALSE;

    mm_common_parse_key_value_string (setup_threshold_str,
                                      &parse_ctx.error,
                                      (MMParseKeyValueForeachFn)key_value_foreach,
                                      &parse_ctx);
    /* If error, destroy the object */
    if (parse_ctx.error) {
        return NULL;
    }

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

    if (parse_ctx.rssi_set)
        g_variant_builder_add (&builder,
                               "{sv}",
                               "rssi_threshold",
                               g_variant_new_uint32 (parse_ctx.rssi_threshold));

    if (parse_ctx.error_rate_set)
        g_variant_builder_add (&builder,
                               "{sv}",
                               "error_rate_threshold",
                               g_variant_new_uint32 (parse_ctx.error_rate_threshold));

    return g_variant_ref_sink (g_variant_builder_end (&builder));
}

static void
setup_thresholds_process_reply (gboolean      result,
                     const GError *error)
{
    if (!result) {
        g_printerr ("error: couldn't setup threshold settings: '%s'\n",
                    error ? error->message : "unknown error");
        exit (EXIT_FAILURE);
    }

    g_print ("Successfully configured threshold settings\n");
}

static void
setup_thresholds_ready (MMModemSignal *modem,
             GAsyncResult  *result)
{
    gboolean res;
    GError *error = NULL;

    res = mm_modem_signal_setup_thresholds_finish (modem, result, &error);
    setup_thresholds_process_reply (res, error);

    mmcli_async_operation_done ();
}


static void
setup_process_reply (gboolean      result,
                     const GError *error)
{
    if (!result) {
        g_printerr ("error: couldn't setup extended signal information retrieval: '%s'\n",
                    error ? error->message : "unknown error");
        exit (EXIT_FAILURE);
    }

    g_print ("Successfully setup extended signal information retrieval\n");
}

static void
setup_ready (MMModemSignal *modem,
             GAsyncResult  *result)
{
    gboolean res;
    GError *error = NULL;

    res = mm_modem_signal_setup_finish (modem, result, &error);
    setup_process_reply (res, error);

    mmcli_async_operation_done ();
}

static void
get_modem_ready (GObject      *source,
                 GAsyncResult *result)
{
    ctx->object = mmcli_get_modem_finish (result, &ctx->manager);
    ctx->modem_signal = mm_object_get_modem_signal (ctx->object);

    /* Setup operation timeout */
    if (ctx->modem_signal)
        mmcli_force_operation_timeout (G_DBUS_PROXY (ctx->modem_signal));

    ensure_modem_signal ();

    if (get_flag)
        g_assert_not_reached ();

    /* Request to setup? */
    if (setup_str) {
        guint rate;

        if (!mm_get_uint_from_str (setup_str, &rate)) {
            g_printerr ("error: invalid rate value '%s'\n", setup_str);
            exit (EXIT_FAILURE);
        }

        g_debug ("Asynchronously setting up extended signal quality information retrieval...");
        mm_modem_signal_setup (ctx->modem_signal,
                               rate,
                               ctx->cancellable,
                               (GAsyncReadyCallback)setup_ready,
                               NULL);
        return;
    }

    /* Request to setup threshold? */
    if (setup_threshold_str) {
        GVariant *variant;

        variant = mm_get_threshold_from_str (setup_threshold_str);
        if (!variant) {
            g_printerr ("error: invalid threshold value '%s'", setup_threshold_str);
            exit (EXIT_FAILURE);
        }

        g_debug ("Asynchronously setting up threshold values...");
        mm_modem_signal_setup_thresholds (ctx->modem_signal,
                               variant,
                               ctx->cancellable,
                               (GAsyncReadyCallback)setup_thresholds_ready,
                               NULL);
        return;
    }

    g_warn_if_reached ();
}

void
mmcli_modem_signal_run_asynchronous (GDBusConnection *connection,
                                       GCancellable    *cancellable)
{
    /* Initialize context */
    ctx = g_new0 (Context, 1);
    if (cancellable)
        ctx->cancellable = g_object_ref (cancellable);

    /* Get proper modem */
    mmcli_get_modem (connection,
                     mmcli_get_common_modem_string (),
                     cancellable,
                     (GAsyncReadyCallback)get_modem_ready,
                     NULL);
}

void
mmcli_modem_signal_run_synchronous (GDBusConnection *connection)
{
    GError *error = NULL;

    /* Initialize context */
    ctx = g_new0 (Context, 1);
    ctx->object = mmcli_get_modem_sync (connection,
                                        mmcli_get_common_modem_string (),
                                        &ctx->manager);
    ctx->modem_signal = mm_object_get_modem_signal (ctx->object);

    /* Setup operation timeout */
    if (ctx->modem_signal)
        mmcli_force_operation_timeout (G_DBUS_PROXY (ctx->modem_signal));

    ensure_modem_signal ();

    /* Request to get signal info? */
    if (get_flag) {
        print_signal_info ();
        return;
    }

    /* Request to set rate? */
    if (setup_str) {
        guint rate;
        gboolean result;

        if (!mm_get_uint_from_str (setup_str, &rate)) {
            g_printerr ("error: invalid rate value '%s'\n", setup_str);
            exit (EXIT_FAILURE);
        }

        g_debug ("Synchronously setting up extended signal quality information retrieval...");
        result = mm_modem_signal_setup_sync (ctx->modem_signal,
                                             rate,
                                             NULL,
                                             &error);
        setup_process_reply (result, error);
        return;
    }

    /* Request to setup threshold? */
    if (setup_threshold_str) {
        GVariant *variant;
        gboolean result;

        variant = mm_get_threshold_from_str (setup_threshold_str);
        if (!variant) {
            g_printerr ("error: invalid threshold value '%s'", setup_threshold_str);
            exit (EXIT_FAILURE);
        }

        g_debug ("Asynchronously setting up threshold values...");
        result = mm_modem_signal_setup_thresholds_sync (ctx->modem_signal,
                               variant,
                               NULL,
                               &error);
        setup_thresholds_process_reply (result, error);
        return;
    }


    g_warn_if_reached ();
}
