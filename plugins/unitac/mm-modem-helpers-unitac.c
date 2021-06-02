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

#include <glib.h>
#include <string.h>
#include <arpa/inet.h>

#include "mm-log.h"
#include "mm-modem-helpers.h"
#include "mm-modem-helpers-unitac.h"

/*****************************************************************************/
/* +CGATT? response parser */

gboolean
mm_unitac_parse_cgatt_response (const gchar *response,
                                guint       *attach,
                                GError      **error)
{
    GRegex     *r;
    GMatchInfo *match_info;
    GError     *inner_error = NULL;
    guint      cgatt = 0;

    /* Response may be e.g.:
     * response: '+CGATT:1'
     */
    r = g_regex_new ("\\+CGATT:(\\d)", 0, 0, NULL);
    g_assert (r != NULL);

    g_regex_match_full (r, response, strlen (response), 0, 0, &match_info, &inner_error);
    if (inner_error)
        goto out;

    if (!g_match_info_matches (match_info)) {
        inner_error = g_error_new (MM_CORE_ERROR, MM_CORE_ERROR_INVALID_ARGS, "Couldn't match +CGATT response");
        goto out;
    }

    if (!mm_get_uint_from_match_info(match_info, 1, &cgatt))
    {
        inner_error = g_error_new(MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "Error parsing +CGATT");
        goto out;
    }

    out:

    g_match_info_free (match_info);
    g_regex_unref (r);

    if (inner_error) {
        g_propagate_error (error, inner_error);
        return FALSE;
    }

    *attach = cgatt;

    return TRUE;
}

/*****************************************************************************/
/* CGPADDR=N response parser */

gboolean
mm_unitac_parse_cgpaddr_response (const gchar *response,
                                  gchar       **out_ipv4_address,
                                  gchar       **out_ipv6_address,
                                  GError      **error)
{
    GRegex     *r;
    GMatchInfo *match_info;
    GError     *inner_error = NULL;
    gchar      *ipv4_address = NULL;
    gchar      *ipv6_address = NULL;

    /* Response may be e.g.:
     * response: '+CGPADDR: 1,"172.22.1.100"'
     *
     * We assume only ONE line is returned; because we request +CGPADDR with a specific N CID.
     */
    r = g_regex_new ("\\+CGPADDR: (\\d+)(?:,([^,]*))(?:,([^,]*))?", 0, 0, NULL);
    g_assert (r != NULL);

    g_regex_match_full (r, response, strlen (response), 0, 0, &match_info, &inner_error);
    if (inner_error)
        goto out;

    if (!g_match_info_matches (match_info)) {
        inner_error = g_error_new (MM_CORE_ERROR, MM_CORE_ERROR_INVALID_ARGS, "Couldn't match +CGPADDR response");
        goto out;
    }

    if (out_ipv4_address)
        ipv4_address = mm_get_string_unquoted_from_match_info(match_info, 2);

    if (out_ipv6_address)
        ipv6_address = mm_get_string_unquoted_from_match_info(match_info, 3);

    out:

    g_match_info_free (match_info);
    g_regex_unref (r);

    if (inner_error) {
        g_free (ipv4_address);
        g_free (ipv6_address);
        g_propagate_error (error, inner_error);
        return FALSE;
    }

    if (out_ipv4_address && ipv4_address)
        *out_ipv4_address = ipv4_address;

    if (out_ipv6_address && ipv6_address)
        *out_ipv6_address = ipv6_address;

    return TRUE;
}


/*****************************************************************************/
/* +CGCONTRDP=N response parser */

static gboolean
split_local_address_and_subnet (const gchar *str,
                                gchar       **local_address,
                                gchar       **subnet)
{
    const gchar *separator;
    guint count = 0;

    /* E.g. split: "2.43.2.44.255.255.255.255"
     * into:
     *    local address: "2.43.2.44",
     *    subnet: "255.255.255.255"
     */
    g_assert (str);
    g_assert (local_address);
    g_assert (subnet);

    separator = str;
    while (1) {
        separator = strchr (separator, '.');
        if (separator) {
            count++;
            if (count == 4) {
                if (local_address)
                    *local_address = g_strndup (str, (separator - str));
                if (subnet)
                    *subnet = g_strdup (++separator);
                return TRUE;
            }
            separator++;
            continue;
        }

        /* Not even the full IP? report error parsing */
        if (count < 3)
            return FALSE;

        if (count == 3) {
            if (local_address)
                *local_address = g_strdup (str);
            if (subnet)
                *subnet = NULL;
            return TRUE;
        }
    }
}

static int
ipv6_netmask_len(char * mask_str)
{
    struct in6_addr netmask;
    int len = 0;
    unsigned char val;
    unsigned char *pnt;

    inet_pton(AF_INET6, mask_str, &netmask);
    pnt = (unsigned char *) & netmask;

    while ((*pnt == 0xff) && len < 128)
    {
        len += 8;
        pnt++;
    }

    if (len < 128)
    {
        val = *pnt;
        while (val)
        {
            len++;
            val <<= 1;
        }
    }
    return len;
}

gboolean
mm_3gpp_cust_parse_cgcontrdp_response (const gchar  *response,
                                       gchar        **ipv4_address,
                                       gchar        **ipv4_subnet,
                                       gchar        **ipv4_gateway,
                                       gchar        **ipv4_dns1_addresses,
                                       gchar        **ipv4_dns2_addresses,
                                       guint        *ipv4_mtu,
                                       gchar        **ipv6_address,
                                       guint        *ipv6_subnet,
                                       gchar        **ipv6_gateway,
                                       gchar        **ipv6_dns1_addresses,
                                       gchar        **ipv6_dns2_addresses,
                                       guint        *ipv6_mtu,
                                       GError       **error)
{
    GRegex       *r;
    GMatchInfo   *match_info;
    GError       *inner_error = NULL;
    guint        cid = 0;
    guint        bearer_id = 0;

    gchar        *ipv4_addr = NULL;
    gchar        *ipv4_sub = NULL;
    gchar        *ipv4_gat = NULL;
    gchar        *ipv4_dns1 = NULL;
    gchar        *ipv4_dns2 = NULL;
    guint        v4_mtu = 0;

    gchar        *ipv6_addr = NULL;
    guint        ipv6_sub = 0;
    gchar        *ipv6_gat = NULL;
    gchar        *ipv6_dns1 = NULL;
    gchar        *ipv6_dns2 = NULL;
    guint        v6_mtu = 0;

    r = g_regex_new ("\\+CGCONTRDP: "
                     "(\\d+),(\\d+),([^,]*)" /* cid, bearer id, apn */
                     "(?:,([^,]*))?" /* (a)ip+mask        or (b)ip */
                     "(?:,([^,]*))?" /* (a)gateway        or (b)mask */
                     "(?:,([^,]*))?" /* (a)dns1           or (b)gateway */
                     "(?:,([^,]*))?" /* (a)dns2           or (b)dns1 */
                     "(?:,([^,]*))?" /* (a)p-cscf primary or (b)dns2 */
                     "(?:,(.*))?"    /* others, ignored */
                     ",(\\d+\\d+\\d+\\d+)?" /* MTU */
                     "(?:\\r\\n)?",
                     G_REGEX_DOLLAR_ENDONLY | G_REGEX_RAW, 0, NULL);
    g_assert (r != NULL);

    g_regex_match_full (r, response, strlen (response), 0, 0, &match_info, &inner_error);
    if (inner_error)
        goto out;

    while (!inner_error &&
           g_match_info_matches (match_info))
    {
        // Check CID
        if (!mm_get_uint_from_match_info(match_info, 1, &cid))
        {
            inner_error = g_error_new(MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "Error parsing cid");
            break;
        }

        // Check bearer ID
        if (!mm_get_uint_from_match_info(match_info, 2, &bearer_id))
        {
            inner_error = g_error_new(MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "Error parsing bearer id");
            break;
        }

        // Skip APN check as not needed

        // Get Local Address and Subnet
        gchar      *local_address_and_subnet = NULL;

        local_address_and_subnet = mm_get_string_unquoted_from_match_info(match_info, 4);
        if (local_address_and_subnet)
        {
            if(strstr (local_address_and_subnet, ":"))
            {
                char delim[] = " ";
                if (ipv6_address && ipv6_subnet)
                {
                    char * temp = NULL;
                    temp = strtok(local_address_and_subnet, delim);
                    if (temp != NULL)
                        ipv6_addr = strdup(temp);
                    temp = strtok(NULL, delim);
                    if (temp != NULL)
                        ipv6_sub = ipv6_netmask_len(temp);
                }
                if (ipv6_gateway)
                    ipv6_gat = mm_get_string_unquoted_from_match_info(match_info, 5);
                if (ipv6_dns1_addresses)
                    ipv6_dns1 = mm_get_string_unquoted_from_match_info(match_info, 6);
                if (ipv6_dns2_addresses)
                    ipv6_dns2 = mm_get_string_unquoted_from_match_info(match_info, 7);
                mm_get_uint_from_match_info(match_info, 10, &v6_mtu);
            }
            else
            {
                if (ipv4_address && ipv4_subnet)
                    split_local_address_and_subnet(local_address_and_subnet, &ipv4_addr, &ipv4_sub);
                if (ipv4_gateway)
                    ipv4_gat = mm_get_string_unquoted_from_match_info(match_info, 5);
                if (ipv4_dns1_addresses)
                    ipv4_dns1 = mm_get_string_unquoted_from_match_info(match_info, 6);
                if (ipv4_dns2_addresses)
                    ipv4_dns2 = mm_get_string_unquoted_from_match_info(match_info, 7);
                mm_get_uint_from_match_info(match_info, 10, &v4_mtu);
            }
        }

        g_free (local_address_and_subnet);
        g_match_info_next (match_info, &inner_error);
    }

    out:
    g_match_info_free (match_info);
    g_regex_unref (r);

    if (inner_error) {
        g_free (ipv4_addr);
        g_free (ipv4_sub);
        g_free (ipv4_gat);
        g_free (ipv4_dns1);
        g_free (ipv4_dns2);
        g_free (ipv6_addr);
        g_free (ipv6_gat);
        g_free (ipv6_dns1);
        g_free (ipv6_dns2);
        g_propagate_error (error, inner_error);
        return FALSE;
    }

    /* IPv4 Connection Details */
    if (ipv4_address)
        *ipv4_address = ipv4_addr;
    else
        g_free (ipv4_addr);

    if (ipv4_subnet)
        *ipv4_subnet = ipv4_sub;
    else
        g_free (ipv4_sub);

    if (ipv4_gateway)
        *ipv4_gateway = ipv4_gat;
    if (ipv4_dns1_addresses)
        *ipv4_dns1_addresses = ipv4_dns1;
    if (ipv4_dns2_addresses)
        *ipv4_dns2_addresses = ipv4_dns2;

    *ipv4_mtu = v4_mtu;

    /* IPv6 Connection Details */
    if (ipv6_address)
        *ipv6_address = ipv6_addr;
    else
        g_free (ipv6_addr);

    if (ipv6_subnet)
        *ipv6_subnet = ipv6_sub;

    if (ipv6_gateway)
        *ipv6_gateway = ipv6_gat;
    if (ipv6_dns1_addresses)
        *ipv6_dns1_addresses = ipv6_dns1;
    if (ipv6_dns2_addresses)
        *ipv6_dns2_addresses = ipv6_dns2;

    *ipv6_mtu = v6_mtu;

    return TRUE;
}

/*****************************************************************************/
/* +CESQ response parser */

gboolean
mm_3gpp_parse_cesq_response (const gchar *response,
                             guint *out_rxlev,
                             guint *out_ber,
                             guint *out_rscp,
                             guint *out_ecn0,
                             guint *out_rsrq,
                             guint *out_rsrp,
                             GError **error)
{
    GRegex     *r;
    GMatchInfo *match_info;
    GError     *inner_error = NULL;
    guint      rxlev = 99;
    guint      ber = 99;
    guint      rscp = 255;
    guint      ecn0 = 255;
    guint      rsrq = 255;
    guint      rsrp = 255;
    gboolean   success = FALSE;

    g_assert (out_rxlev);
    g_assert (out_ber);
    g_assert (out_rscp);
    g_assert (out_ecn0);
    g_assert (out_rsrq);
    g_assert (out_rsrp);

    /* Response may be e.g.:
     * +CESQ: 99,99,255,255,20,80
     */
    r = g_regex_new ("\\+CESQ: (\\d+), (\\d+), (\\d+), (\\d+), (\\d+), (\\d+)(?:\\r\\n)?", 0, 0, NULL);
    g_assert (r != NULL);

    g_regex_match_full (r, response, strlen (response), 0, 0, &match_info, &inner_error);
    if (!inner_error && g_match_info_matches (match_info)) {
        if (!mm_get_uint_from_match_info (match_info, 1, &rxlev)) {
            inner_error = g_error_new (MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "Couldn't read RXLEV");
            goto out;
        }
        if (!mm_get_uint_from_match_info (match_info, 2, &ber)) {
            inner_error = g_error_new (MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "Couldn't read BER");
            goto out;
        }
        if (!mm_get_uint_from_match_info (match_info, 3, &rscp)) {
            inner_error = g_error_new (MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "Couldn't read RSCP");
            goto out;
        }
        if (!mm_get_uint_from_match_info (match_info, 4, &ecn0)) {
            inner_error = g_error_new (MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "Couldn't read Ec/N0");
            goto out;
        }
        if (!mm_get_uint_from_match_info (match_info, 5, &rsrq)) {
            inner_error = g_error_new (MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "Couldn't read RSRQ");
            goto out;
        }
        if (!mm_get_uint_from_match_info (match_info, 6, &rsrp)) {
            inner_error = g_error_new (MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "Couldn't read RSRP");
            goto out;
        }
        success = TRUE;
    }

    out:
    g_match_info_free (match_info);
    g_regex_unref (r);

    if (inner_error) {
        g_propagate_error (error, inner_error);
        return FALSE;
    }

    if (!success) {
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                     "couldn't parse +CESQ response: %s", response);
        return FALSE;
    }

    *out_rxlev = rxlev;
    *out_ber = ber;
    *out_rscp = rscp;
    *out_ecn0 = ecn0;
    *out_rsrq = rsrq;
    *out_rsrp = rsrp;
    return TRUE;
}


/*****************************************************************************/
/* %WMODE? response parser */

gboolean
mm_unitac_parse_wmode_response (const gchar *response,
                                GError **error,
                                gpointer log,
                                GTask *task)
{
    GRegex                 *r;
    GMatchInfo             *match_info;
    GError                 *inner_error = NULL;

    /* Response may be e.g.:
     * response: 'MODE: bridge-ecm'
     */
    r = g_regex_new ("MODE: (\\w+.*)", 0, 0, NULL);
    g_assert (r != NULL);

    g_regex_match_full (r, response, strlen (response), 0, 0, &match_info, &inner_error);
    if (!inner_error && g_match_info_matches (match_info)) {
        gchar *wmode;

        wmode = mm_get_string_unquoted_from_match_info (match_info, 1);
        if (wmode) {
            mm_obj_dbg (log, "WMODE mode: %s", wmode);
            if (!g_str_equal (wmode, "bridge-ecm"))
                inner_error = g_error_new(MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "wrong wmode for the modem...");
        } else
            inner_error = g_error_new(MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "unable to parse wmode for the modem...");

        g_free (wmode);
    }

    g_match_info_free (match_info);
    g_regex_unref (r);

    if (inner_error) {
        g_propagate_error (error, inner_error);
        return FALSE;
    }

    return TRUE;
}

/*****************************************************************************/
/* Load current bands (Modem interface) */

static MMModemBand
uband_num_to_band (guint num)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (uband_band_config); i++) {
        if (num == uband_band_config[i].num)
            return uband_band_config[i].band;
    }

    return MM_MODEM_BAND_UNKNOWN;
}

static GArray *
uband_num_array_to_band_array (GArray *nums)
{
    GArray *bands = NULL;
    guint  i;

    if (!nums)
        return NULL;

    bands = g_array_sized_new (FALSE, FALSE, sizeof (MMModemBand), nums->len);
    for (i = 0; i < nums->len; i++) {
        MMModemBand band;

        band = uband_num_to_band (g_array_index (nums, guint, i));
        g_array_append_val (bands, band);
    }

    return bands;
}


GArray *
mm_unitac_parse_uband_response (const gchar *response,
                                gpointer log,
                                GError **error)
{
    GRegex     *r;
    GMatchInfo *match_info;
    GError     *inner_error = NULL;
    GArray     *nums        = NULL;
    GArray     *bands       = NULL;

    /*
     * AT+UBAND?
     * !UBAND: 31,20,3,72
     */
    r = g_regex_new ("\\!UBAND: (.*)(?:\\r\\n)?",
                     G_REGEX_DOLLAR_ENDONLY | G_REGEX_RAW, 0, NULL);
    g_assert (r != NULL);

    g_regex_match_full (r, response, strlen (response), 0, 0, &match_info, &inner_error);
    if (!inner_error && g_match_info_matches (match_info)) {
        gchar *bandstr;
        bandstr = mm_get_string_unquoted_from_match_info (match_info, 1);
        mm_obj_dbg (log, "UBAND string: %s", bandstr);
        nums = mm_parse_uint_list (bandstr, &inner_error);
        g_free (bandstr);
    }

    g_match_info_free (match_info);
    g_regex_unref (r);

    if (inner_error) {
        g_propagate_error (error, inner_error);
        return NULL;
    }

    /* Convert to MMModemBand values */
    if (nums) {
        bands = uband_num_array_to_band_array (nums);
        g_array_unref (nums);
    }

    if (!bands)
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                     "no known band selection values matched in !UBAND response: '%s'", response);

    return bands;
}

GArray *
mm_unitac_parse_supported_bands_response(const gchar *response,GError **error)
{
    GRegex     *regex      = NULL;
    GMatchInfo *match_info = NULL;
    gchar      *match_str  = NULL;
    gchar      **split;
    GArray     *bands      = NULL;
    guint      i           = 0;
    guint32     band_value = 0;
    MMModemBand band       = 0;

    static const gchar *load_regex = "\\!UBAND:\\s*\\(([0-9\\-]*)\\)";  //Eg: !UBAND: (31-20-3-7)
    regex = g_regex_new (load_regex, G_REGEX_DOLLAR_ENDONLY | G_REGEX_RAW, 0, NULL);
    g_assert (regex);

    if (!g_regex_match (regex, response, 0, &match_info)) {
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                     "Could not parse response '%s'", response);
        goto out;
    }

    if( g_match_info_matches (match_info) ){
        match_str = g_match_info_fetch(match_info, 1);
        if(match_str){
            split = g_strsplit_set (match_str, "-", -1);
            if (!split){
                g_free(match_str);
                goto out;
            }
            else if( 0 == split[0] ){
                g_strfreev(split);
                g_free(match_str);
                g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                             "Could not split values in matchs");
                goto out;
            }
            bands = g_array_sized_new (FALSE, FALSE, sizeof (MMModemBand), g_strv_length(split));
            for (i = 0; split[i]; i++) {
                band_value = (guint32)strtoul (split[i], NULL, 10);
                band = MM_MODEM_BAND_EUTRAN_1 - 1 + band_value;
                if (band >= MM_MODEM_BAND_EUTRAN_1 && band <= MM_MODEM_BAND_EUTRAN_72){
                    g_array_append_val(bands, band);
               }
            }
            g_strfreev(split);
            g_free(match_str);
        }
    }
    else{
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                     "Could not find matches in response '%s'", response);
    }
out:
    g_match_info_free(match_info);
    g_regex_unref(regex);
    return bands;
}
