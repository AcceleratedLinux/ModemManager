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

#ifndef MM_MODEM_HELPERS_UNITAC_H
#define MM_MODEM_HELPERS_UNITAC_H

#include <glib.h>
#include <ModemManager.h>

/*****************************************************************************/
/* +CGATT? response parser */

gboolean
mm_unitac_parse_cgatt_response (const gchar *response,
                                guint       *attach,
                                GError      **error);

/*****************************************************************************/
/* +CGPADDR=N response parser */

gboolean
mm_unitac_parse_cgpaddr_response (const gchar *response,
                                  gchar       **out_ipv4_address,
                                  gchar       **out_ipv6_address,
                                  GError      **error);

/*****************************************************************************/
/* +CGCONTRDP=N response parser */

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
                                       GError       **error);

/*****************************************************************************/
/* %WMODE? response parser */

gboolean mm_unitac_parse_wmode_response (const gchar *response,
                                         GError **error,
                                         gpointer log,
                                         GTask *task);

/*****************************************************************************/
/* +UBAND? response parser */

typedef struct {
    guint       num;
    MMModemBand band;
} UbandBandConfig;

static const UbandBandConfig uband_band_config[] = {
        /* LTE bands */
        { .num =  3,  .band = MM_MODEM_BAND_EUTRAN_3  },
        { .num =  7,  .band = MM_MODEM_BAND_EUTRAN_7  },
        { .num =  20, .band = MM_MODEM_BAND_EUTRAN_20 },
        { .num =  31, .band = MM_MODEM_BAND_EUTRAN_31 },
        { .num =  72, .band = MM_MODEM_BAND_EUTRAN_72 },
        { .num =  87, .band = MM_MODEM_BAND_EUTRAN_87 },
};

GArray * mm_unitac_parse_uband_response (const gchar *response,
                                         gpointer log,
                                         GError **error);

GArray * mm_unitac_parse_supported_bands_response(const gchar *response,GError **error);



#endif  /* MM_MODEM_HELPERS_UNITAC_H */
