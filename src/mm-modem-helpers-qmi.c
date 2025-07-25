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
 * Copyright (C) 2012-2018 Google, Inc.
 * Copyright (C) 2018 Aleksander Morgado <aleksander@aleksander.es>
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc.
 */

#include <string.h>

#include <ModemManager.h>
#include <mm-errors-types.h>

#include "mm-modem-helpers-qmi.h"
#include "mm-modem-helpers.h"
#include "mm-error-helpers.h"
#include "mm-enums-types.h"
#include "mm-flags-types.h"
#include "mm-log-object.h"

/*****************************************************************************/

MMModemCapability
mm_modem_capability_from_qmi_radio_interface (QmiDmsRadioInterface network,
                                              gpointer             log_object)
{
    switch (network) {
    case QMI_DMS_RADIO_INTERFACE_CDMA20001X:
        return MM_MODEM_CAPABILITY_CDMA_EVDO;
    case QMI_DMS_RADIO_INTERFACE_EVDO:
        return MM_MODEM_CAPABILITY_CDMA_EVDO;
    case QMI_DMS_RADIO_INTERFACE_GSM:
        return MM_MODEM_CAPABILITY_GSM_UMTS;
    case QMI_DMS_RADIO_INTERFACE_UMTS:
        return MM_MODEM_CAPABILITY_GSM_UMTS;
    case QMI_DMS_RADIO_INTERFACE_LTE:
        return MM_MODEM_CAPABILITY_LTE;
    case QMI_DMS_RADIO_INTERFACE_TDS:
        return MM_MODEM_CAPABILITY_TDS;
    case QMI_DMS_RADIO_INTERFACE_5GNR:
        return MM_MODEM_CAPABILITY_5GNR;
    default:
        mm_obj_warn (log_object, "unhandled QMI radio interface '%u'", (guint)network);
        return MM_MODEM_CAPABILITY_NONE;
    }
}

/*****************************************************************************/

MMModemMode
mm_modem_mode_from_qmi_radio_interface (QmiDmsRadioInterface network,
                                        gpointer             log_object)
{
    switch (network) {
    case QMI_DMS_RADIO_INTERFACE_CDMA20001X:
        return MM_MODEM_MODE_2G;
    case QMI_DMS_RADIO_INTERFACE_EVDO:
        return MM_MODEM_MODE_3G;
    case QMI_DMS_RADIO_INTERFACE_GSM:
        return MM_MODEM_MODE_2G;
    case QMI_DMS_RADIO_INTERFACE_UMTS:
        return MM_MODEM_MODE_3G;
    case QMI_DMS_RADIO_INTERFACE_LTE:
        return MM_MODEM_MODE_4G;
    case QMI_DMS_RADIO_INTERFACE_5GNR:
        return MM_MODEM_MODE_5G;
    case QMI_DMS_RADIO_INTERFACE_TDS:
    default:
        mm_obj_warn (log_object, "unhandled QMI radio interface '%u'", (guint)network);
        return MM_MODEM_MODE_NONE;
    }
}

/*****************************************************************************/

/* pin1 TRUE for PIN1, FALSE for PIN2 */
MMModemLock
mm_modem_lock_from_qmi_uim_pin_status (QmiDmsUimPinStatus status,
                                       gboolean pin1)
{
    switch (status) {
    case QMI_DMS_UIM_PIN_STATUS_NOT_INITIALIZED:
        return MM_MODEM_LOCK_UNKNOWN;
    case QMI_DMS_UIM_PIN_STATUS_ENABLED_NOT_VERIFIED:
        return pin1 ? MM_MODEM_LOCK_SIM_PIN : MM_MODEM_LOCK_SIM_PIN2;
    case QMI_DMS_UIM_PIN_STATUS_ENABLED_VERIFIED:
        return MM_MODEM_LOCK_NONE;
    case QMI_DMS_UIM_PIN_STATUS_DISABLED:
        return MM_MODEM_LOCK_NONE;
    case QMI_DMS_UIM_PIN_STATUS_BLOCKED:
        return pin1 ? MM_MODEM_LOCK_SIM_PUK : MM_MODEM_LOCK_SIM_PUK2;
    case QMI_DMS_UIM_PIN_STATUS_PERMANENTLY_BLOCKED:
        return MM_MODEM_LOCK_UNKNOWN;
    case QMI_DMS_UIM_PIN_STATUS_UNBLOCKED:
        /* This state is possibly given when after an Unblock() operation has been performed.
         * We'll assume the PIN is verified after this. */
        return MM_MODEM_LOCK_NONE;
    case QMI_DMS_UIM_PIN_STATUS_CHANGED:
        /* This state is possibly given when after an ChangePin() operation has been performed.
         * We'll assume the PIN is verified after this. */
        return MM_MODEM_LOCK_NONE;
    default:
        return MM_MODEM_LOCK_UNKNOWN;
    }
}

/*****************************************************************************/

gboolean
mm_pin_enabled_from_qmi_uim_pin_status (QmiDmsUimPinStatus status)
{
    switch (status) {
    case QMI_DMS_UIM_PIN_STATUS_ENABLED_NOT_VERIFIED:
    case QMI_DMS_UIM_PIN_STATUS_ENABLED_VERIFIED:
    case QMI_DMS_UIM_PIN_STATUS_BLOCKED:
    case QMI_DMS_UIM_PIN_STATUS_PERMANENTLY_BLOCKED:
    case QMI_DMS_UIM_PIN_STATUS_UNBLOCKED:
    case QMI_DMS_UIM_PIN_STATUS_CHANGED:
        /* assume the PIN to be enabled then */
        return TRUE;

    case QMI_DMS_UIM_PIN_STATUS_DISABLED:
    case QMI_DMS_UIM_PIN_STATUS_NOT_INITIALIZED:
        /* assume the PIN to be disabled then */
        return FALSE;

    default:
        /* by default assume disabled */
        return FALSE;
    }
}

/*****************************************************************************/

QmiDmsUimFacility
mm_3gpp_facility_to_qmi_uim_facility (MMModem3gppFacility mm)
{
    switch (mm) {
    case MM_MODEM_3GPP_FACILITY_PH_SIM:
        /* Not really sure about this one; it may be PH_FSIM? */
        return QMI_DMS_UIM_FACILITY_PF;
    case MM_MODEM_3GPP_FACILITY_NET_PERS:
        return QMI_DMS_UIM_FACILITY_PN;
    case MM_MODEM_3GPP_FACILITY_NET_SUB_PERS:
        return QMI_DMS_UIM_FACILITY_PU;
    case MM_MODEM_3GPP_FACILITY_PROVIDER_PERS:
        return QMI_DMS_UIM_FACILITY_PP;
    case MM_MODEM_3GPP_FACILITY_CORP_PERS:
        return QMI_DMS_UIM_FACILITY_PC;
    case MM_MODEM_3GPP_FACILITY_NONE:
    case MM_MODEM_3GPP_FACILITY_SIM:
    case MM_MODEM_3GPP_FACILITY_FIXED_DIALING:
    case MM_MODEM_3GPP_FACILITY_PH_FSIM:
    default:
        /* Never try to ask for a facility we cannot translate */
        g_assert_not_reached ();
    }
}

/*****************************************************************************/

typedef struct {
    QmiDmsBandCapability qmi_band;
    MMModemBand mm_band;
} DmsBandsMap;

static const DmsBandsMap dms_bands_map [] = {
    /* CDMA bands */
    {
        (QMI_DMS_BAND_CAPABILITY_BC_0_A_SYSTEM | QMI_DMS_BAND_CAPABILITY_BC_0_B_SYSTEM),
        MM_MODEM_BAND_CDMA_BC0
    },
    { QMI_DMS_BAND_CAPABILITY_BC_1_ALL_BLOCKS, MM_MODEM_BAND_CDMA_BC1  },
    { QMI_DMS_BAND_CAPABILITY_BC_2,            MM_MODEM_BAND_CDMA_BC2  },
    { QMI_DMS_BAND_CAPABILITY_BC_3_A_SYSTEM,   MM_MODEM_BAND_CDMA_BC3  },
    { QMI_DMS_BAND_CAPABILITY_BC_4_ALL_BLOCKS, MM_MODEM_BAND_CDMA_BC4  },
    { QMI_DMS_BAND_CAPABILITY_BC_5_ALL_BLOCKS, MM_MODEM_BAND_CDMA_BC5  },
    { QMI_DMS_BAND_CAPABILITY_BC_6,            MM_MODEM_BAND_CDMA_BC6  },
    { QMI_DMS_BAND_CAPABILITY_BC_7,            MM_MODEM_BAND_CDMA_BC7  },
    { QMI_DMS_BAND_CAPABILITY_BC_8,            MM_MODEM_BAND_CDMA_BC8  },
    { QMI_DMS_BAND_CAPABILITY_BC_9,            MM_MODEM_BAND_CDMA_BC9  },
    { QMI_DMS_BAND_CAPABILITY_BC_10,           MM_MODEM_BAND_CDMA_BC10 },
    { QMI_DMS_BAND_CAPABILITY_BC_11,           MM_MODEM_BAND_CDMA_BC11 },
    { QMI_DMS_BAND_CAPABILITY_BC_12,           MM_MODEM_BAND_CDMA_BC12 },
    { QMI_DMS_BAND_CAPABILITY_BC_14,           MM_MODEM_BAND_CDMA_BC14 },
    { QMI_DMS_BAND_CAPABILITY_BC_15,           MM_MODEM_BAND_CDMA_BC15 },
    { QMI_DMS_BAND_CAPABILITY_BC_16,           MM_MODEM_BAND_CDMA_BC16 },
    { QMI_DMS_BAND_CAPABILITY_BC_17,           MM_MODEM_BAND_CDMA_BC17 },
    { QMI_DMS_BAND_CAPABILITY_BC_18,           MM_MODEM_BAND_CDMA_BC18 },
    { QMI_DMS_BAND_CAPABILITY_BC_19,           MM_MODEM_BAND_CDMA_BC19 },

    /* GSM bands */
    { QMI_DMS_BAND_CAPABILITY_GSM_DCS_1800,     MM_MODEM_BAND_DCS  },
    {
        (QMI_DMS_BAND_CAPABILITY_GSM_900_PRIMARY | QMI_DMS_BAND_CAPABILITY_GSM_900_EXTENDED),
        MM_MODEM_BAND_EGSM
    },
    { QMI_DMS_BAND_CAPABILITY_GSM_PCS_1900,     MM_MODEM_BAND_PCS  },
    { QMI_DMS_BAND_CAPABILITY_GSM_850,          MM_MODEM_BAND_G850 },
    { QMI_DMS_BAND_CAPABILITY_GSM_450,          MM_MODEM_BAND_G450 },
    { QMI_DMS_BAND_CAPABILITY_GSM_480,          MM_MODEM_BAND_G480 },
    { QMI_DMS_BAND_CAPABILITY_GSM_750,          MM_MODEM_BAND_G750 },

    /* UMTS/WCDMA bands */
    { QMI_DMS_BAND_CAPABILITY_WCDMA_2100,       MM_MODEM_BAND_UTRAN_1  },
    { QMI_DMS_BAND_CAPABILITY_WCDMA_DCS_1800,   MM_MODEM_BAND_UTRAN_3  },
    { QMI_DMS_BAND_CAPABILITY_WCDMA_PCS_1900,   MM_MODEM_BAND_UTRAN_2  },
    { QMI_DMS_BAND_CAPABILITY_WCDMA_1700_US,    MM_MODEM_BAND_UTRAN_4  },
    { QMI_DMS_BAND_CAPABILITY_WCDMA_800,        MM_MODEM_BAND_UTRAN_6  },
    { QMI_DMS_BAND_CAPABILITY_WCDMA_850_US,     MM_MODEM_BAND_UTRAN_5  },
    { QMI_DMS_BAND_CAPABILITY_WCDMA_900,        MM_MODEM_BAND_UTRAN_8  },
    { QMI_DMS_BAND_CAPABILITY_WCDMA_1700_JAPAN, MM_MODEM_BAND_UTRAN_9  },
    { QMI_DMS_BAND_CAPABILITY_WCDMA_2600,       MM_MODEM_BAND_UTRAN_7  },
    { QMI_DMS_BAND_CAPABILITY_WCDMA_1500,       MM_MODEM_BAND_UTRAN_11 },
    { QMI_DMS_BAND_CAPABILITY_WCDMA_850_JAPAN,  MM_MODEM_BAND_UTRAN_19 },
};

static void
dms_add_qmi_bands (GArray               *mm_bands,
                   QmiDmsBandCapability  qmi_bands,
                   gpointer              log_object)
{
    static QmiDmsBandCapability qmi_bands_expected = 0;
    QmiDmsBandCapability not_expected;
    guint i;

    g_assert (mm_bands != NULL);

    /* Build mask of expected bands only once */
    if (G_UNLIKELY (qmi_bands_expected == 0)) {
        for (i = 0; i < G_N_ELEMENTS (dms_bands_map); i++) {
            qmi_bands_expected |= dms_bands_map[i].qmi_band;
        }
    }

    /* Log about the bands that cannot be represented in ModemManager */
    not_expected = ((qmi_bands_expected ^ qmi_bands) & qmi_bands);
    if (not_expected) {
        g_autofree gchar *aux = NULL;

        aux = qmi_dms_band_capability_build_string_from_mask (not_expected);
        mm_obj_dbg (log_object, "cannot add the following bands: '%s'", aux);
    }

    /* And add the expected ones */
    for (i = 0; i < G_N_ELEMENTS (dms_bands_map); i++) {
        if (qmi_bands & dms_bands_map[i].qmi_band)
            g_array_append_val (mm_bands, dms_bands_map[i].mm_band);
    }
}

typedef struct {
    QmiDmsLteBandCapability qmi_band;
    MMModemBand mm_band;
} DmsLteBandsMap;

static const DmsLteBandsMap dms_lte_bands_map [] = {
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_1,  MM_MODEM_BAND_EUTRAN_1  },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_2,  MM_MODEM_BAND_EUTRAN_2  },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_3,  MM_MODEM_BAND_EUTRAN_3  },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_4,  MM_MODEM_BAND_EUTRAN_4  },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_5,  MM_MODEM_BAND_EUTRAN_5  },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_6,  MM_MODEM_BAND_EUTRAN_6  },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_7,  MM_MODEM_BAND_EUTRAN_7  },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_8,  MM_MODEM_BAND_EUTRAN_8  },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_9,  MM_MODEM_BAND_EUTRAN_9  },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_10, MM_MODEM_BAND_EUTRAN_10 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_11, MM_MODEM_BAND_EUTRAN_11 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_12, MM_MODEM_BAND_EUTRAN_12 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_13, MM_MODEM_BAND_EUTRAN_13 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_14, MM_MODEM_BAND_EUTRAN_14 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_17, MM_MODEM_BAND_EUTRAN_17 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_18, MM_MODEM_BAND_EUTRAN_18 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_19, MM_MODEM_BAND_EUTRAN_19 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_20, MM_MODEM_BAND_EUTRAN_20 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_21, MM_MODEM_BAND_EUTRAN_21 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_24, MM_MODEM_BAND_EUTRAN_24 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_25, MM_MODEM_BAND_EUTRAN_25 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_26, MM_MODEM_BAND_EUTRAN_26 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_27, MM_MODEM_BAND_EUTRAN_27 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_28, MM_MODEM_BAND_EUTRAN_28 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_29, MM_MODEM_BAND_EUTRAN_29 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_30, MM_MODEM_BAND_EUTRAN_30 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_31, MM_MODEM_BAND_EUTRAN_31 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_32, MM_MODEM_BAND_EUTRAN_32 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_33, MM_MODEM_BAND_EUTRAN_33 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_34, MM_MODEM_BAND_EUTRAN_34 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_35, MM_MODEM_BAND_EUTRAN_35 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_36, MM_MODEM_BAND_EUTRAN_36 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_37, MM_MODEM_BAND_EUTRAN_37 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_38, MM_MODEM_BAND_EUTRAN_38 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_39, MM_MODEM_BAND_EUTRAN_39 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_40, MM_MODEM_BAND_EUTRAN_40 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_41, MM_MODEM_BAND_EUTRAN_41 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_42, MM_MODEM_BAND_EUTRAN_42 },
    { QMI_DMS_LTE_BAND_CAPABILITY_EUTRAN_43, MM_MODEM_BAND_EUTRAN_43 }
};

static void
dms_add_qmi_lte_bands (GArray *mm_bands,
                       QmiDmsLteBandCapability qmi_bands)
{
    /* All QMI LTE bands have a counterpart in ModemManager, no need to check
     * for unexpected ones */
    guint i;

    g_assert (mm_bands != NULL);

    for (i = 0; i < G_N_ELEMENTS (dms_lte_bands_map); i++) {
        if (qmi_bands & dms_lte_bands_map[i].qmi_band)
            g_array_append_val (mm_bands, dms_lte_bands_map[i].mm_band);
    }
}

static void
dms_add_extended_qmi_lte_bands (GArray   *mm_bands,
                                GArray   *extended_qmi_bands,
                                gpointer  log_object)
{
    guint i;

    g_assert (mm_bands != NULL);

    if (!extended_qmi_bands)
        return;

    for (i = 0; i < extended_qmi_bands->len; i++) {
        guint16 val;

        val = g_array_index (extended_qmi_bands, guint16, i);

        /* Valid EUTRAN bands:
         *
         * MM_MODEM_BAND_EUTRAN_1 = 31,
         * ...
         * MM_MODEM_BAND_EUTRAN_71 = 101
         * <gap>
         * MM_MODEM_BAND_EUTRAN_85 = 115
         *
         * There's then an offset of 1000 for EUTRAN bands >= 98
         *
         * MM_MODEM_BAND_EUTRAN_106 = 1136
         */
        if ((val >= 1 && val <= 71) ||
            (val == 85) || (val == 106)) {
            MMModemBand band;

            if (val >= 98)
                band = (MMModemBand)(val + 1000 + MM_MODEM_BAND_EUTRAN_1 - 1);
            else
                band = (MMModemBand)(val + MM_MODEM_BAND_EUTRAN_1 - 1);
            g_array_append_val (mm_bands, band);
        } else
            mm_obj_dbg (log_object, "unexpected LTE band supported by module: EUTRAN %u", val);
    }
}

static void
dms_add_qmi_nr5g_bands (GArray   *mm_bands,
                        GArray   *qmi_bands,
                        gpointer  log_object)
{
    guint i;

    g_assert (mm_bands != NULL);

    if (!qmi_bands)
        return;

    for (i = 0; i < qmi_bands->len; i++) {
        guint16 val;

        val = g_array_index (qmi_bands, guint16, i);

        /* MM_MODEM_BAND_NGRAN_1 = 301,
         * ...
         * MM_MODEM_BAND_NGRAN_261 = 561
         */
        if (val < 1 || val > 261)
            mm_obj_dbg (log_object, "unexpected NR5G band supported by module: NGRAN %u", val);
        else {
            MMModemBand band;

            band = (MMModemBand)(val + MM_MODEM_BAND_NGRAN_1 - 1);
            g_array_append_val (mm_bands, band);
        }
    }
}

GArray *
mm_modem_bands_from_qmi_band_capabilities (QmiDmsBandCapability     qmi_bands,
                                           QmiDmsLteBandCapability  qmi_lte_bands,
                                           GArray                  *extended_qmi_lte_bands,
                                           GArray                  *qmi_nr5g_bands,
                                           gpointer                 log_object)
{
    GArray *mm_bands;

    mm_bands = g_array_new (FALSE, FALSE, sizeof (MMModemBand));
    dms_add_qmi_bands (mm_bands, qmi_bands, log_object);

    if (extended_qmi_lte_bands)
        dms_add_extended_qmi_lte_bands (mm_bands, extended_qmi_lte_bands, log_object);
    else
        dms_add_qmi_lte_bands (mm_bands, qmi_lte_bands);

    if (qmi_nr5g_bands)
        dms_add_qmi_nr5g_bands (mm_bands, qmi_nr5g_bands, log_object);

    return mm_bands;
}

/*****************************************************************************/

typedef struct {
    QmiNasBandPreference qmi_band;
    MMModemBand mm_band;
} NasBandsMap;

static const NasBandsMap nas_bands_map [] = {
    /* CDMA bands */
    {
        (QMI_NAS_BAND_PREFERENCE_BC_0_A_SYSTEM | QMI_NAS_BAND_PREFERENCE_BC_0_B_SYSTEM),
        MM_MODEM_BAND_CDMA_BC0
    },
    { QMI_NAS_BAND_PREFERENCE_BC_1_ALL_BLOCKS, MM_MODEM_BAND_CDMA_BC1  },
    { QMI_NAS_BAND_PREFERENCE_BC_2,            MM_MODEM_BAND_CDMA_BC2  },
    { QMI_NAS_BAND_PREFERENCE_BC_3_A_SYSTEM,   MM_MODEM_BAND_CDMA_BC3  },
    { QMI_NAS_BAND_PREFERENCE_BC_4_ALL_BLOCKS, MM_MODEM_BAND_CDMA_BC4  },
    { QMI_NAS_BAND_PREFERENCE_BC_5_ALL_BLOCKS, MM_MODEM_BAND_CDMA_BC5  },
    { QMI_NAS_BAND_PREFERENCE_BC_6,            MM_MODEM_BAND_CDMA_BC6  },
    { QMI_NAS_BAND_PREFERENCE_BC_7,            MM_MODEM_BAND_CDMA_BC7  },
    { QMI_NAS_BAND_PREFERENCE_BC_8,            MM_MODEM_BAND_CDMA_BC8  },
    { QMI_NAS_BAND_PREFERENCE_BC_9,            MM_MODEM_BAND_CDMA_BC9  },
    { QMI_NAS_BAND_PREFERENCE_BC_10,           MM_MODEM_BAND_CDMA_BC10 },
    { QMI_NAS_BAND_PREFERENCE_BC_11,           MM_MODEM_BAND_CDMA_BC11 },
    { QMI_NAS_BAND_PREFERENCE_BC_12,           MM_MODEM_BAND_CDMA_BC12 },
    { QMI_NAS_BAND_PREFERENCE_BC_14,           MM_MODEM_BAND_CDMA_BC14 },
    { QMI_NAS_BAND_PREFERENCE_BC_15,           MM_MODEM_BAND_CDMA_BC15 },
    { QMI_NAS_BAND_PREFERENCE_BC_16,           MM_MODEM_BAND_CDMA_BC16 },
    { QMI_NAS_BAND_PREFERENCE_BC_17,           MM_MODEM_BAND_CDMA_BC17 },
    { QMI_NAS_BAND_PREFERENCE_BC_18,           MM_MODEM_BAND_CDMA_BC18 },
    { QMI_NAS_BAND_PREFERENCE_BC_19,           MM_MODEM_BAND_CDMA_BC19 },

    /* GSM bands */
    { QMI_NAS_BAND_PREFERENCE_GSM_DCS_1800,     MM_MODEM_BAND_DCS  },
    {
        (QMI_NAS_BAND_PREFERENCE_GSM_900_PRIMARY | QMI_NAS_BAND_PREFERENCE_GSM_900_EXTENDED),
        MM_MODEM_BAND_EGSM
    },
    { QMI_NAS_BAND_PREFERENCE_GSM_PCS_1900,     MM_MODEM_BAND_PCS  },
    { QMI_NAS_BAND_PREFERENCE_GSM_850,          MM_MODEM_BAND_G850 },
    { QMI_NAS_BAND_PREFERENCE_GSM_450,          MM_MODEM_BAND_G450 },
    { QMI_NAS_BAND_PREFERENCE_GSM_480,          MM_MODEM_BAND_G480 },
    { QMI_NAS_BAND_PREFERENCE_GSM_750,          MM_MODEM_BAND_G750 },

    /* UMTS/WCDMA bands */
    { QMI_NAS_BAND_PREFERENCE_WCDMA_2100,       MM_MODEM_BAND_UTRAN_1  },
    { QMI_NAS_BAND_PREFERENCE_WCDMA_DCS_1800,   MM_MODEM_BAND_UTRAN_3  },
    { QMI_NAS_BAND_PREFERENCE_WCDMA_PCS_1900,   MM_MODEM_BAND_UTRAN_2  },
    { QMI_NAS_BAND_PREFERENCE_WCDMA_1700_US,    MM_MODEM_BAND_UTRAN_4  },
    { QMI_NAS_BAND_PREFERENCE_WCDMA_800,        MM_MODEM_BAND_UTRAN_6  },
    { QMI_NAS_BAND_PREFERENCE_WCDMA_850_US,     MM_MODEM_BAND_UTRAN_5  },
    { QMI_NAS_BAND_PREFERENCE_WCDMA_900,        MM_MODEM_BAND_UTRAN_8  },
    { QMI_NAS_BAND_PREFERENCE_WCDMA_1700_JAPAN, MM_MODEM_BAND_UTRAN_9  },
    { QMI_NAS_BAND_PREFERENCE_WCDMA_2600,       MM_MODEM_BAND_UTRAN_7  },
    { QMI_NAS_BAND_PREFERENCE_WCDMA_1500,       MM_MODEM_BAND_UTRAN_11 },
    { QMI_NAS_BAND_PREFERENCE_WCDMA_850_JAPAN,  MM_MODEM_BAND_UTRAN_19 },
};

static void
nas_add_qmi_bands (GArray               *mm_bands,
                   QmiNasBandPreference  qmi_bands,
                   gpointer              log_object)
{
    static QmiNasBandPreference qmi_bands_expected = 0;
    QmiNasBandPreference not_expected;
    guint i;

    g_assert (mm_bands != NULL);

    /* Build mask of expected bands only once */
    if (G_UNLIKELY (qmi_bands_expected == 0)) {
        for (i = 0; i < G_N_ELEMENTS (nas_bands_map); i++) {
            qmi_bands_expected |= nas_bands_map[i].qmi_band;
        }
    }

    /* Log about the bands that cannot be represented in ModemManager */
    not_expected = ((qmi_bands_expected ^ qmi_bands) & qmi_bands);
    if (not_expected) {
        g_autofree gchar *aux = NULL;

        aux = qmi_nas_band_preference_build_string_from_mask (not_expected);
        mm_obj_dbg (log_object, "cannot add the following bands: '%s'", aux);
    }

    /* And add the expected ones */
    for (i = 0; i < G_N_ELEMENTS (nas_bands_map); i++) {
        if (qmi_bands & nas_bands_map[i].qmi_band)
            g_array_append_val (mm_bands, nas_bands_map[i].mm_band);
    }
}

typedef struct {
    QmiNasLteBandPreference qmi_band;
    MMModemBand mm_band;
} NasLteBandsMap;

static const NasLteBandsMap nas_lte_bands_map [] = {
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_1,  MM_MODEM_BAND_EUTRAN_1  },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_2,  MM_MODEM_BAND_EUTRAN_2  },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_3,  MM_MODEM_BAND_EUTRAN_3  },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_4,  MM_MODEM_BAND_EUTRAN_4  },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_5,  MM_MODEM_BAND_EUTRAN_5  },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_6,  MM_MODEM_BAND_EUTRAN_6  },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_7,  MM_MODEM_BAND_EUTRAN_7  },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_8,  MM_MODEM_BAND_EUTRAN_8  },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_9,  MM_MODEM_BAND_EUTRAN_9  },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_10, MM_MODEM_BAND_EUTRAN_10 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_11, MM_MODEM_BAND_EUTRAN_11 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_12, MM_MODEM_BAND_EUTRAN_12 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_13, MM_MODEM_BAND_EUTRAN_13 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_14, MM_MODEM_BAND_EUTRAN_14 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_17, MM_MODEM_BAND_EUTRAN_17 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_18, MM_MODEM_BAND_EUTRAN_18 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_19, MM_MODEM_BAND_EUTRAN_19 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_20, MM_MODEM_BAND_EUTRAN_20 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_21, MM_MODEM_BAND_EUTRAN_21 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_24, MM_MODEM_BAND_EUTRAN_24 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_25, MM_MODEM_BAND_EUTRAN_25 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_26, MM_MODEM_BAND_EUTRAN_26 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_27, MM_MODEM_BAND_EUTRAN_27 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_28, MM_MODEM_BAND_EUTRAN_28 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_29, MM_MODEM_BAND_EUTRAN_29 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_30, MM_MODEM_BAND_EUTRAN_30 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_31, MM_MODEM_BAND_EUTRAN_31 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_32, MM_MODEM_BAND_EUTRAN_32 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_33, MM_MODEM_BAND_EUTRAN_33 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_34, MM_MODEM_BAND_EUTRAN_34 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_35, MM_MODEM_BAND_EUTRAN_35 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_36, MM_MODEM_BAND_EUTRAN_36 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_37, MM_MODEM_BAND_EUTRAN_37 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_38, MM_MODEM_BAND_EUTRAN_38 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_39, MM_MODEM_BAND_EUTRAN_39 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_40, MM_MODEM_BAND_EUTRAN_40 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_41, MM_MODEM_BAND_EUTRAN_41 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_42, MM_MODEM_BAND_EUTRAN_42 },
    { QMI_NAS_LTE_BAND_PREFERENCE_EUTRAN_43, MM_MODEM_BAND_EUTRAN_43 }
};

static void
nas_add_qmi_lte_bands (GArray *mm_bands,
                       QmiNasLteBandPreference qmi_bands)
{
    /* All QMI LTE bands have a counterpart in ModemManager, no need to check
     * for unexpected ones */
    guint i;

    g_assert (mm_bands != NULL);

    for (i = 0; i < G_N_ELEMENTS (nas_lte_bands_map); i++) {
        if (qmi_bands & nas_lte_bands_map[i].qmi_band)
            g_array_append_val (mm_bands, nas_lte_bands_map[i].mm_band);
    }
}

static void
nas_add_extended_qmi_lte_bands (GArray        *mm_bands,
                                const guint64 *extended_qmi_lte_bands,
                                guint          extended_qmi_lte_bands_size,
                                gpointer       log_object)
{
    guint i;

    g_assert (mm_bands != NULL);

    for (i = 0; i < extended_qmi_lte_bands_size; i++) {
        guint j;

        for (j = 0; j < 64; j++) {
            guint val;

            if (!(extended_qmi_lte_bands[i] & (((guint64) 1) << j)))
                continue;

            val = 1 + j + (i * 64);

            /* Valid EUTRAN bands:
             *
             * MM_MODEM_BAND_EUTRAN_1 = 31,
             * ...
             * MM_MODEM_BAND_EUTRAN_71 = 101
             * <gap>
             * MM_MODEM_BAND_EUTRAN_85 = 115
             *
             * There's then an offset of 1000 for EUTRAN bands >= 98
             *
             * MM_MODEM_BAND_EUTRAN_106 = 1136
             */
            if ((val >= 1 && val <= 71) ||
                (val == 85) || (val == 106)) {
                MMModemBand band;

                if (val >= 98)
                    band = (val + 1000 + MM_MODEM_BAND_EUTRAN_1 - 1);
                else
                    band = (val + MM_MODEM_BAND_EUTRAN_1 - 1);
                g_array_append_val (mm_bands, band);
            } else
                mm_obj_dbg (log_object, "unexpected LTE band supported by module: EUTRAN %u", val);
        }
    }
}

static void
nas_add_qmi_nr5g_bands (GArray        *mm_bands,
                        const guint64 *qmi_nr5g_bands,
                        guint          qmi_nr5g_bands_size,
                        gpointer       log_object)
{
    guint i;

    g_assert (mm_bands != NULL);

    for (i = 0; i < qmi_nr5g_bands_size; i++) {
        guint j;

        for (j = 0; j < 64; j++) {
            guint val;

            if (!(qmi_nr5g_bands[i] & (((guint64) 1) << j)))
                continue;

            val = 1 + j + (i * 64);

            /* MM_MODEM_BAND_NGRAN_1 = 301,
             * ...
             * MM_MODEM_BAND_NGRAN_261 = 561
             */
            if (val < 1 || val > 261)
                mm_obj_dbg (log_object, "unexpected NR5G band supported by module: NGRAN %u", val);
            else {
                MMModemBand band;

                band = (val + MM_MODEM_BAND_NGRAN_1 - 1);
                g_array_append_val (mm_bands, band);
            }
        }
    }
}

GArray *
mm_modem_bands_from_qmi_band_preference (QmiNasBandPreference     qmi_bands,
                                         QmiNasLteBandPreference  qmi_lte_bands,
                                         const guint64           *extended_qmi_lte_bands,
                                         guint                    extended_qmi_lte_bands_size,
                                         const guint64           *qmi_nr5g_bands,
                                         guint                    qmi_nr5g_bands_size,
                                         gpointer                 log_object)
{
    GArray *mm_bands;

    mm_bands = g_array_new (FALSE, FALSE, sizeof (MMModemBand));
    nas_add_qmi_bands (mm_bands, qmi_bands, log_object);

    if (extended_qmi_lte_bands && extended_qmi_lte_bands_size)
        nas_add_extended_qmi_lte_bands (mm_bands, extended_qmi_lte_bands, extended_qmi_lte_bands_size, log_object);
    else
        nas_add_qmi_lte_bands (mm_bands, qmi_lte_bands);

    if (qmi_nr5g_bands && qmi_nr5g_bands_size)
        nas_add_qmi_nr5g_bands (mm_bands, qmi_nr5g_bands, qmi_nr5g_bands_size, log_object);

    return mm_bands;
}

void
mm_modem_bands_to_qmi_band_preference (GArray                  *mm_bands,
                                       QmiNasBandPreference    *qmi_bands,
                                       QmiNasLteBandPreference *qmi_lte_bands,
                                       guint64                 *extended_qmi_lte_bands,
                                       guint                    extended_qmi_lte_bands_size,
                                       guint64                 *qmi_nr5g_bands,
                                       guint                    qmi_nr5g_bands_size,
                                       gpointer                 log_object)
{
    guint i;

    *qmi_bands = 0;
    *qmi_lte_bands = 0;
    if (extended_qmi_lte_bands)
        memset (extended_qmi_lte_bands, 0, extended_qmi_lte_bands_size * sizeof (guint64));
    if (qmi_nr5g_bands)
        memset (qmi_nr5g_bands, 0, qmi_nr5g_bands_size * sizeof (guint64));

    for (i = 0; i < mm_bands->len; i++) {
        MMModemBand band;

        band = g_array_index (mm_bands, MMModemBand, i);

        if ((band >= MM_MODEM_BAND_EUTRAN_1 && band <= MM_MODEM_BAND_EUTRAN_71) ||
            (band == MM_MODEM_BAND_EUTRAN_85) ||
            (band == MM_MODEM_BAND_EUTRAN_106)) {
            if (extended_qmi_lte_bands && extended_qmi_lte_bands_size) {
                /* Add extended LTE band preference */
                guint val;
                guint j;
                guint k;

                /* Check first if we need to subtract the offset */
                if (band == MM_MODEM_BAND_EUTRAN_106)
                    band -= 1000;
                /* it's really (band - MM_MODEM_BAND_EUTRAN_1 +1 -1), because
                 * we want EUTRAN1 in index 0 */
                val = band - MM_MODEM_BAND_EUTRAN_1;
                j = val / 64;
                g_assert (j < extended_qmi_lte_bands_size);
                k = val % 64;

                extended_qmi_lte_bands[j] |= ((guint64)1 << k);
            } else {
                /* Add LTE band preference */
                guint j;

                for (j = 0; j < G_N_ELEMENTS (nas_lte_bands_map); j++) {
                    if (nas_lte_bands_map[j].mm_band == band) {
                        *qmi_lte_bands |= nas_lte_bands_map[j].qmi_band;
                        break;
                    }
                }

                if (j == G_N_ELEMENTS (nas_lte_bands_map))
                    mm_obj_dbg (log_object, "cannot add the following LTE band: '%s'",
                                mm_modem_band_get_string (band));
            }
        } else if (band >= MM_MODEM_BAND_NGRAN_1 && band <= MM_MODEM_BAND_NGRAN_261) {
            if (qmi_nr5g_bands && qmi_nr5g_bands_size) {
                /* Add NR5G band preference */
                guint val;
                guint j;
                guint k;

                /* it's really (band - MM_MODEM_BAND_NGRAN_1 +1 -1), because
                 * we want NGRAN1 in index 0 */
                val = band - MM_MODEM_BAND_NGRAN_1;
                j = val / 64;
                g_assert (j < qmi_nr5g_bands_size);
                k = val % 64;

                qmi_nr5g_bands[j] |= ((guint64)1 << k);
            }
        } else {
            /* Add non-LTE band preference */
            guint j;

            for (j = 0; j < G_N_ELEMENTS (nas_bands_map); j++) {
                if (nas_bands_map[j].mm_band == band) {
                    *qmi_bands |= nas_bands_map[j].qmi_band;
                    break;
                }
            }

            if (j == G_N_ELEMENTS (nas_bands_map))
                mm_obj_dbg (log_object, "cannot add the following band: '%s'",
                            mm_modem_band_get_string (band));
        }
    }
}

/*****************************************************************************/

typedef struct {
    QmiNasActiveBand qmi_band;
    MMModemBand mm_band;
} ActiveBandsMap;

static const ActiveBandsMap active_bands_map [] = {
    /* CDMA bands */
    { QMI_NAS_ACTIVE_BAND_BC_0,  MM_MODEM_BAND_CDMA_BC0  },
    { QMI_NAS_ACTIVE_BAND_BC_1,  MM_MODEM_BAND_CDMA_BC1  },
    { QMI_NAS_ACTIVE_BAND_BC_2,  MM_MODEM_BAND_CDMA_BC2  },
    { QMI_NAS_ACTIVE_BAND_BC_3,  MM_MODEM_BAND_CDMA_BC3  },
    { QMI_NAS_ACTIVE_BAND_BC_4,  MM_MODEM_BAND_CDMA_BC4  },
    { QMI_NAS_ACTIVE_BAND_BC_5,  MM_MODEM_BAND_CDMA_BC5  },
    { QMI_NAS_ACTIVE_BAND_BC_6,  MM_MODEM_BAND_CDMA_BC6  },
    { QMI_NAS_ACTIVE_BAND_BC_7,  MM_MODEM_BAND_CDMA_BC7  },
    { QMI_NAS_ACTIVE_BAND_BC_8,  MM_MODEM_BAND_CDMA_BC8  },
    { QMI_NAS_ACTIVE_BAND_BC_9,  MM_MODEM_BAND_CDMA_BC9  },
    { QMI_NAS_ACTIVE_BAND_BC_10, MM_MODEM_BAND_CDMA_BC10 },
    { QMI_NAS_ACTIVE_BAND_BC_11, MM_MODEM_BAND_CDMA_BC11 },
    { QMI_NAS_ACTIVE_BAND_BC_12, MM_MODEM_BAND_CDMA_BC12 },
    { QMI_NAS_ACTIVE_BAND_BC_13, MM_MODEM_BAND_CDMA_BC13 },
    { QMI_NAS_ACTIVE_BAND_BC_14, MM_MODEM_BAND_CDMA_BC14 },
    { QMI_NAS_ACTIVE_BAND_BC_15, MM_MODEM_BAND_CDMA_BC15 },
    { QMI_NAS_ACTIVE_BAND_BC_16, MM_MODEM_BAND_CDMA_BC16 },
    { QMI_NAS_ACTIVE_BAND_BC_17, MM_MODEM_BAND_CDMA_BC17 },
    { QMI_NAS_ACTIVE_BAND_BC_18, MM_MODEM_BAND_CDMA_BC18 },
    { QMI_NAS_ACTIVE_BAND_BC_19, MM_MODEM_BAND_CDMA_BC19 },

    /* GSM bands */
    { QMI_NAS_ACTIVE_BAND_GSM_850,          MM_MODEM_BAND_G850 },
    { QMI_NAS_ACTIVE_BAND_GSM_900_EXTENDED, MM_MODEM_BAND_EGSM },
    { QMI_NAS_ACTIVE_BAND_GSM_DCS_1800,     MM_MODEM_BAND_DCS  },
    { QMI_NAS_ACTIVE_BAND_GSM_PCS_1900,     MM_MODEM_BAND_PCS  },
    { QMI_NAS_ACTIVE_BAND_GSM_450,          MM_MODEM_BAND_G450 },
    { QMI_NAS_ACTIVE_BAND_GSM_480,          MM_MODEM_BAND_G480 },
    { QMI_NAS_ACTIVE_BAND_GSM_750,          MM_MODEM_BAND_G750 },

    /* WCDMA bands */
    { QMI_NAS_ACTIVE_BAND_WCDMA_2100,       MM_MODEM_BAND_UTRAN_1  },
    { QMI_NAS_ACTIVE_BAND_WCDMA_PCS_1900,   MM_MODEM_BAND_UTRAN_2  },
    { QMI_NAS_ACTIVE_BAND_WCDMA_DCS_1800,   MM_MODEM_BAND_UTRAN_3  },
    { QMI_NAS_ACTIVE_BAND_WCDMA_1700_US,    MM_MODEM_BAND_UTRAN_4  },
    { QMI_NAS_ACTIVE_BAND_WCDMA_850,        MM_MODEM_BAND_UTRAN_5  },
    { QMI_NAS_ACTIVE_BAND_WCDMA_800,        MM_MODEM_BAND_UTRAN_6  },
    { QMI_NAS_ACTIVE_BAND_WCDMA_2600,       MM_MODEM_BAND_UTRAN_7  },
    { QMI_NAS_ACTIVE_BAND_WCDMA_900,        MM_MODEM_BAND_UTRAN_8  },
    { QMI_NAS_ACTIVE_BAND_WCDMA_1700_JAPAN, MM_MODEM_BAND_UTRAN_9  },
    { QMI_NAS_ACTIVE_BAND_WCDMA_1500_JAPAN, MM_MODEM_BAND_UTRAN_11 },
    { QMI_NAS_ACTIVE_BAND_WCDMA_850_JAPAN,  MM_MODEM_BAND_UTRAN_19 },

    /* LTE bands */
    { QMI_NAS_ACTIVE_BAND_EUTRAN_1,  MM_MODEM_BAND_EUTRAN_1  },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_2,  MM_MODEM_BAND_EUTRAN_2  },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_3,  MM_MODEM_BAND_EUTRAN_3  },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_4,  MM_MODEM_BAND_EUTRAN_4  },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_5,  MM_MODEM_BAND_EUTRAN_5  },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_6,  MM_MODEM_BAND_EUTRAN_6  },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_7,  MM_MODEM_BAND_EUTRAN_7  },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_8,  MM_MODEM_BAND_EUTRAN_8  },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_9,  MM_MODEM_BAND_EUTRAN_9  },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_10, MM_MODEM_BAND_EUTRAN_10 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_11, MM_MODEM_BAND_EUTRAN_11 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_12, MM_MODEM_BAND_EUTRAN_12 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_13, MM_MODEM_BAND_EUTRAN_13 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_14, MM_MODEM_BAND_EUTRAN_14 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_17, MM_MODEM_BAND_EUTRAN_17 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_18, MM_MODEM_BAND_EUTRAN_18 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_19, MM_MODEM_BAND_EUTRAN_19 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_20, MM_MODEM_BAND_EUTRAN_20 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_21, MM_MODEM_BAND_EUTRAN_21 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_24, MM_MODEM_BAND_EUTRAN_24 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_25, MM_MODEM_BAND_EUTRAN_25 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_33, MM_MODEM_BAND_EUTRAN_33 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_34, MM_MODEM_BAND_EUTRAN_34 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_35, MM_MODEM_BAND_EUTRAN_35 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_36, MM_MODEM_BAND_EUTRAN_36 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_37, MM_MODEM_BAND_EUTRAN_37 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_38, MM_MODEM_BAND_EUTRAN_38 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_39, MM_MODEM_BAND_EUTRAN_39 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_40, MM_MODEM_BAND_EUTRAN_40 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_41, MM_MODEM_BAND_EUTRAN_41 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_42, MM_MODEM_BAND_EUTRAN_42 },
    { QMI_NAS_ACTIVE_BAND_EUTRAN_43, MM_MODEM_BAND_EUTRAN_43 }
};

static void
add_active_bands (GArray *mm_bands,
                  QmiNasActiveBand qmi_bands)
{
    guint i;

    g_assert (mm_bands != NULL);

    for (i = 0; i < G_N_ELEMENTS (active_bands_map); i++) {
        if (qmi_bands == active_bands_map[i].qmi_band) {
            guint j;

            /* Avoid adding duplicate band entries */
            for (j = 0; j < mm_bands->len; j++) {
                if (g_array_index (mm_bands, MMModemBand, j) == active_bands_map[i].mm_band)
                    return;
            }

            g_array_append_val (mm_bands, active_bands_map[i].mm_band);
            return;
        }
    }
}

GArray *
mm_modem_bands_from_qmi_rf_band_information_array (GArray *info_array)
{
    GArray *mm_bands;

    mm_bands = g_array_new (FALSE, FALSE, sizeof (MMModemBand));

    if (info_array) {
        guint i;

        for (i = 0; i < info_array->len; i++) {
            QmiMessageNasGetRfBandInformationOutputListElement *item;

            item = &g_array_index (info_array, QmiMessageNasGetRfBandInformationOutputListElement, i);
            add_active_bands (mm_bands, item->active_band_class);
        }
    }

    return mm_bands;
}

/*****************************************************************************/

MMModemAccessTechnology
mm_modem_access_technology_from_qmi_radio_interface (QmiNasRadioInterface interface)
{
    switch (interface) {
    case QMI_NAS_RADIO_INTERFACE_CDMA_1X:
        return MM_MODEM_ACCESS_TECHNOLOGY_1XRTT;
    case QMI_NAS_RADIO_INTERFACE_CDMA_1XEVDO:
        return MM_MODEM_ACCESS_TECHNOLOGY_EVDO0;
    case QMI_NAS_RADIO_INTERFACE_GSM:
        return MM_MODEM_ACCESS_TECHNOLOGY_GSM;
    case QMI_NAS_RADIO_INTERFACE_UMTS:
        return MM_MODEM_ACCESS_TECHNOLOGY_UMTS;
    case QMI_NAS_RADIO_INTERFACE_LTE:
        return MM_MODEM_ACCESS_TECHNOLOGY_LTE;
    case QMI_NAS_RADIO_INTERFACE_5GNR:
        return MM_MODEM_ACCESS_TECHNOLOGY_5GNR;
    case QMI_NAS_RADIO_INTERFACE_UNKNOWN:
    case QMI_NAS_RADIO_INTERFACE_TD_SCDMA:
    case QMI_NAS_RADIO_INTERFACE_AMPS:
    case QMI_NAS_RADIO_INTERFACE_NONE:
    default:
        return MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN;
    }
}

/*****************************************************************************/

MMModemAccessTechnology
mm_modem_access_technologies_from_qmi_radio_interface_array (GArray *radio_interfaces)
{
    MMModemAccessTechnology access_technology = 0;
    guint i;

    for (i = 0; i < radio_interfaces->len; i++) {
        QmiNasRadioInterface iface;

        iface = g_array_index (radio_interfaces, QmiNasRadioInterface, i);
        access_technology |= mm_modem_access_technology_from_qmi_radio_interface (iface);
    }

    return access_technology;
}

/*****************************************************************************/

MMModemAccessTechnology
mm_modem_access_technology_from_qmi_data_capability (QmiNasDataCapability cap)
{
    switch (cap) {
    case QMI_NAS_DATA_CAPABILITY_GPRS:
        return MM_MODEM_ACCESS_TECHNOLOGY_GPRS;
    case QMI_NAS_DATA_CAPABILITY_EDGE:
        return MM_MODEM_ACCESS_TECHNOLOGY_EDGE;
    case QMI_NAS_DATA_CAPABILITY_HSDPA:
        return MM_MODEM_ACCESS_TECHNOLOGY_HSDPA;
    case QMI_NAS_DATA_CAPABILITY_HSUPA:
        return MM_MODEM_ACCESS_TECHNOLOGY_HSUPA;
    case QMI_NAS_DATA_CAPABILITY_WCDMA:
        return MM_MODEM_ACCESS_TECHNOLOGY_UMTS;
    case QMI_NAS_DATA_CAPABILITY_CDMA:
        return MM_MODEM_ACCESS_TECHNOLOGY_1XRTT;
    case QMI_NAS_DATA_CAPABILITY_EVDO_REV_0:
        return MM_MODEM_ACCESS_TECHNOLOGY_EVDO0;
    case QMI_NAS_DATA_CAPABILITY_EVDO_REV_A:
        return MM_MODEM_ACCESS_TECHNOLOGY_EVDOA;
    case QMI_NAS_DATA_CAPABILITY_GSM:
        return MM_MODEM_ACCESS_TECHNOLOGY_GSM;
    case QMI_NAS_DATA_CAPABILITY_EVDO_REV_B:
        return MM_MODEM_ACCESS_TECHNOLOGY_EVDOB;
    case QMI_NAS_DATA_CAPABILITY_LTE:
        return MM_MODEM_ACCESS_TECHNOLOGY_LTE;
    case QMI_NAS_DATA_CAPABILITY_HSDPA_PLUS:
    case QMI_NAS_DATA_CAPABILITY_DC_HSDPA_PLUS:
        return MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS;
    case QMI_NAS_DATA_CAPABILITY_NONE:
    default:
        return MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN;
    }
}

/*****************************************************************************/

MMModemAccessTechnology
mm_modem_access_technologies_from_qmi_data_capability_array (GArray *data_capabilities)
{
    MMModemAccessTechnology access_technology = 0;
    guint i;

    for (i = 0; i < data_capabilities->len; i++) {
        QmiNasDataCapability cap;

        cap = g_array_index (data_capabilities, QmiNasDataCapability, i);
        access_technology |= mm_modem_access_technology_from_qmi_data_capability (cap);
    }

    return access_technology;
}

/*****************************************************************************/

MMModemMode
mm_modem_mode_from_qmi_nas_radio_interface (QmiNasRadioInterface iface)
{
    switch (iface) {
        case QMI_NAS_RADIO_INTERFACE_CDMA_1X:
        case QMI_NAS_RADIO_INTERFACE_GSM:
            return MM_MODEM_MODE_2G;
        case QMI_NAS_RADIO_INTERFACE_CDMA_1XEVDO:
        case QMI_NAS_RADIO_INTERFACE_UMTS:
            return MM_MODEM_MODE_3G;
        case QMI_NAS_RADIO_INTERFACE_LTE:
            return MM_MODEM_MODE_4G;
        case QMI_NAS_RADIO_INTERFACE_5GNR:
            return MM_MODEM_MODE_5G;
        case QMI_NAS_RADIO_INTERFACE_NONE:
        case QMI_NAS_RADIO_INTERFACE_AMPS:
        case QMI_NAS_RADIO_INTERFACE_TD_SCDMA:
        case QMI_NAS_RADIO_INTERFACE_UNKNOWN:
        default:
            return MM_MODEM_MODE_NONE;
    }
}

/*****************************************************************************/

MMModemMode
mm_modem_mode_from_qmi_radio_technology_preference (QmiNasRadioTechnologyPreference qmi)
{
    MMModemMode mode = MM_MODEM_MODE_NONE;

    if (qmi & QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_3GPP2) {
        /* Ignore AMPS, we really don't report CS mode in QMI modems */
        if (qmi & QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_CDMA_OR_WCDMA)
            mode |= MM_MODEM_MODE_2G; /* CDMA */
        if (qmi & QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_HDR)
            mode |= MM_MODEM_MODE_3G; /* EV-DO */
    }

    if (qmi & QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_3GPP) {
        if (qmi & QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_AMPS_OR_GSM)
            mode |= MM_MODEM_MODE_2G; /* GSM */
        if (qmi & QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_CDMA_OR_WCDMA)
            mode |= MM_MODEM_MODE_3G; /* WCDMA */
    }

    if (qmi & QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_LTE)
        mode |= MM_MODEM_MODE_4G;

    return mode;
}

QmiNasRadioTechnologyPreference
mm_modem_mode_to_qmi_radio_technology_preference (MMModemMode mode,
                                                  gboolean is_cdma)
{
    QmiNasRadioTechnologyPreference pref = 0;

    if (is_cdma) {
        pref |= QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_3GPP2;
        if (mode & MM_MODEM_MODE_2G)
            pref |= QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_CDMA_OR_WCDMA; /* CDMA */
        if (mode & MM_MODEM_MODE_3G)
            pref |= QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_HDR; /* EV-DO */
    } else {
        pref |= QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_3GPP;
        if (mode & MM_MODEM_MODE_2G)
            pref |= QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_AMPS_OR_GSM; /* GSM */
        if (mode & MM_MODEM_MODE_3G)
            pref |= QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_CDMA_OR_WCDMA; /* WCDMA */
    }

    if (mode & MM_MODEM_MODE_4G)
        pref |= QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_LTE;

    return pref;
}

/*****************************************************************************/

MMModemMode
mm_modem_mode_from_qmi_rat_mode_preference (QmiNasRatModePreference qmi)
{
    MMModemMode mode = MM_MODEM_MODE_NONE;

    if (qmi & QMI_NAS_RAT_MODE_PREFERENCE_CDMA_1X)
        mode |= MM_MODEM_MODE_2G;

    if (qmi & QMI_NAS_RAT_MODE_PREFERENCE_CDMA_1XEVDO)
        mode |= MM_MODEM_MODE_3G;

    if (qmi & QMI_NAS_RAT_MODE_PREFERENCE_GSM)
        mode |= MM_MODEM_MODE_2G;

    if (qmi & QMI_NAS_RAT_MODE_PREFERENCE_UMTS)
        mode |= MM_MODEM_MODE_3G;

    if (qmi & QMI_NAS_RAT_MODE_PREFERENCE_LTE)
        mode |= MM_MODEM_MODE_4G;

    if (qmi & QMI_NAS_RAT_MODE_PREFERENCE_5GNR)
        mode |= MM_MODEM_MODE_5G;

    return mode;
}

QmiNasRatModePreference
mm_modem_mode_to_qmi_rat_mode_preference (MMModemMode mode,
                                          gboolean is_cdma,
                                          gboolean is_3gpp)
{
    QmiNasRatModePreference pref = 0;

    if (is_cdma) {
        if (mode & MM_MODEM_MODE_2G)
            pref |= QMI_NAS_RAT_MODE_PREFERENCE_CDMA_1X;

        if (mode & MM_MODEM_MODE_3G)
            pref |= QMI_NAS_RAT_MODE_PREFERENCE_CDMA_1XEVDO;
    }

    if (is_3gpp) {
        if (mode & MM_MODEM_MODE_2G)
            pref |= QMI_NAS_RAT_MODE_PREFERENCE_GSM;

        if (mode & MM_MODEM_MODE_3G)
            pref |= QMI_NAS_RAT_MODE_PREFERENCE_UMTS;

        if (mode & MM_MODEM_MODE_4G)
            pref |= QMI_NAS_RAT_MODE_PREFERENCE_LTE;

        if (mode & MM_MODEM_MODE_5G)
            pref |= QMI_NAS_RAT_MODE_PREFERENCE_5GNR;
    }

    return pref;
}

/*****************************************************************************/

MMModemCapability
mm_modem_capability_from_qmi_rat_mode_preference (QmiNasRatModePreference qmi)
{
    MMModemCapability caps = MM_MODEM_CAPABILITY_NONE;

    if (qmi & QMI_NAS_RAT_MODE_PREFERENCE_CDMA_1X)
        caps |= MM_MODEM_CAPABILITY_CDMA_EVDO;

    if (qmi & QMI_NAS_RAT_MODE_PREFERENCE_CDMA_1XEVDO)
        caps |= MM_MODEM_CAPABILITY_CDMA_EVDO;

    if (qmi & QMI_NAS_RAT_MODE_PREFERENCE_GSM)
        caps |= MM_MODEM_CAPABILITY_GSM_UMTS;

    if (qmi & QMI_NAS_RAT_MODE_PREFERENCE_UMTS)
        caps |= MM_MODEM_CAPABILITY_GSM_UMTS;

    if (qmi & QMI_NAS_RAT_MODE_PREFERENCE_LTE)
        caps |= MM_MODEM_CAPABILITY_LTE;

    if (qmi & QMI_NAS_RAT_MODE_PREFERENCE_5GNR)
        caps |= MM_MODEM_CAPABILITY_5GNR;

    return caps;
}

QmiNasRatModePreference
mm_modem_capability_to_qmi_rat_mode_preference (MMModemCapability caps)
{
    QmiNasRatModePreference qmi = 0;

    if (caps & MM_MODEM_CAPABILITY_CDMA_EVDO) {
        qmi |= QMI_NAS_RAT_MODE_PREFERENCE_CDMA_1X;
        qmi |= QMI_NAS_RAT_MODE_PREFERENCE_CDMA_1XEVDO;
    }

    if (caps & MM_MODEM_CAPABILITY_GSM_UMTS) {
        qmi |= QMI_NAS_RAT_MODE_PREFERENCE_GSM;
        qmi |= QMI_NAS_RAT_MODE_PREFERENCE_UMTS;
    }

    if (caps & MM_MODEM_CAPABILITY_LTE)
        qmi |= QMI_NAS_RAT_MODE_PREFERENCE_LTE;

    if (caps & MM_MODEM_CAPABILITY_5GNR)
        qmi |= QMI_NAS_RAT_MODE_PREFERENCE_5GNR;

    return qmi;
}

/*****************************************************************************/

GArray *
mm_modem_capability_to_qmi_acquisition_order_preference (MMModemCapability caps)
{
    GArray               *array;
    QmiNasRadioInterface  value;

    array = g_array_new (FALSE, FALSE, sizeof (QmiNasRadioInterface));

    if (caps & MM_MODEM_CAPABILITY_5GNR) {
        value = QMI_NAS_RADIO_INTERFACE_5GNR;
        g_array_append_val (array, value);
    }

    if (caps & MM_MODEM_CAPABILITY_LTE) {
        value = QMI_NAS_RADIO_INTERFACE_LTE;
        g_array_append_val (array, value);
    }

    if (caps & MM_MODEM_CAPABILITY_GSM_UMTS) {
        value = QMI_NAS_RADIO_INTERFACE_UMTS;
        g_array_append_val (array, value);
        value = QMI_NAS_RADIO_INTERFACE_GSM;
        g_array_append_val (array, value);
    }

    if (caps & MM_MODEM_CAPABILITY_CDMA_EVDO) {
        value = QMI_NAS_RADIO_INTERFACE_CDMA_1XEVDO;
        g_array_append_val (array, value);
        value = QMI_NAS_RADIO_INTERFACE_CDMA_1X;
        g_array_append_val (array, value);
    }

    return array;
}

static gboolean
radio_interface_array_contains (GArray               *array,
                                QmiNasRadioInterface  act)
{
    guint i;

    for (i = 0; i < array->len; i++) {
        QmiNasRadioInterface value;

        value = g_array_index (array, QmiNasRadioInterface, i);
        if (value == act)
            return TRUE;
    }
    return FALSE;
}

static void
radio_interface_array_add_missing (GArray *array,
                                   GArray *all)
{
    guint i;

    for (i = 0; i < all->len; i++) {
        QmiNasRadioInterface value;

        value = g_array_index (all, QmiNasRadioInterface, i);
        if (!radio_interface_array_contains (array, value))
            g_array_append_val (array, value);
    }
}

GArray *
mm_modem_mode_to_qmi_acquisition_order_preference (MMModemMode  allowed,
                                                   MMModemMode  preferred,
                                                   GArray      *all)
{
    GArray               *array;
    QmiNasRadioInterface  preferred_radio = QMI_NAS_RADIO_INTERFACE_UNKNOWN;
    QmiNasRadioInterface  value;

    array = g_array_sized_new (FALSE, FALSE, sizeof (QmiNasRadioInterface), all->len);

#define PROCESS_ALLOWED_PREFERRED_MODE(MODE,RADIO)                      \
    if ((allowed & MODE) && (radio_interface_array_contains (all, RADIO))) { \
        if ((preferred == MODE) && (preferred_radio == QMI_NAS_RADIO_INTERFACE_UNKNOWN)) \
            preferred_radio = RADIO;                                    \
        else {                                                          \
            value = RADIO;                                              \
            g_array_append_val (array, value);                          \
        }                                                               \
    }

    PROCESS_ALLOWED_PREFERRED_MODE (MM_MODEM_MODE_5G, QMI_NAS_RADIO_INTERFACE_5GNR);
    PROCESS_ALLOWED_PREFERRED_MODE (MM_MODEM_MODE_4G, QMI_NAS_RADIO_INTERFACE_LTE);
    PROCESS_ALLOWED_PREFERRED_MODE (MM_MODEM_MODE_3G, QMI_NAS_RADIO_INTERFACE_UMTS);
    PROCESS_ALLOWED_PREFERRED_MODE (MM_MODEM_MODE_3G, QMI_NAS_RADIO_INTERFACE_CDMA_1XEVDO);
    PROCESS_ALLOWED_PREFERRED_MODE (MM_MODEM_MODE_2G, QMI_NAS_RADIO_INTERFACE_GSM);
    PROCESS_ALLOWED_PREFERRED_MODE (MM_MODEM_MODE_2G, QMI_NAS_RADIO_INTERFACE_CDMA_1X);

#undef PROCESS_ALLOWED_PREFERRED_MODE

    if (preferred_radio != QMI_NAS_RADIO_INTERFACE_UNKNOWN)
        g_array_prepend_val (array, preferred_radio);

    /* the acquisition order preference is a TLV that must ALWAYS contain the
     * same list of QmiNasRadioInterface values, just with a different order. */
    radio_interface_array_add_missing (array, all);
    g_assert_cmpuint (array->len, ==, all->len);

    return array;
}

/*****************************************************************************/

MMModemCapability
mm_modem_capability_from_qmi_radio_technology_preference (QmiNasRadioTechnologyPreference qmi)
{
    MMModemCapability caps = MM_MODEM_CAPABILITY_NONE;

    if (qmi & QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_3GPP2) {
        /* Skip AMPS */
        if (qmi & QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_CDMA_OR_WCDMA)
            caps |= MM_MODEM_CAPABILITY_CDMA_EVDO; /* CDMA */
        if (qmi & QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_HDR)
            caps |= MM_MODEM_CAPABILITY_CDMA_EVDO; /* EV-DO */
    }

    if (qmi & QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_3GPP) {
        if (qmi & QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_AMPS_OR_GSM)
            caps |= MM_MODEM_CAPABILITY_GSM_UMTS; /* GSM */
        if (qmi & QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_CDMA_OR_WCDMA)
            caps |= MM_MODEM_CAPABILITY_GSM_UMTS; /* WCDMA */
    }

    if (qmi & QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_LTE)
        caps |= MM_MODEM_CAPABILITY_LTE;

    /* NOTE: no 5GNR defined in Technology Preference */

    return caps;
}

QmiNasRadioTechnologyPreference
mm_modem_capability_to_qmi_radio_technology_preference (MMModemCapability caps)
{
    QmiNasRadioTechnologyPreference qmi = 0;

    if (caps & MM_MODEM_CAPABILITY_GSM_UMTS) {
        qmi |= QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_3GPP;
        qmi |= QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_AMPS_OR_GSM;
        qmi |= QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_CDMA_OR_WCDMA;
    }

    if (caps & MM_MODEM_CAPABILITY_CDMA_EVDO) {
        qmi |= QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_3GPP2;
        qmi |= QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_CDMA_OR_WCDMA;
        qmi |= QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_HDR;
    }

    if (caps & MM_MODEM_CAPABILITY_LTE)
        qmi |= QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_LTE;

    return qmi;
}

/*****************************************************************************/

#define ALL_3GPP2_BANDS                         \
    (QMI_NAS_BAND_PREFERENCE_BC_0_A_SYSTEM   |  \
     QMI_NAS_BAND_PREFERENCE_BC_0_B_SYSTEM   |  \
     QMI_NAS_BAND_PREFERENCE_BC_1_ALL_BLOCKS |  \
     QMI_NAS_BAND_PREFERENCE_BC_2            |  \
     QMI_NAS_BAND_PREFERENCE_BC_3_A_SYSTEM   |  \
     QMI_NAS_BAND_PREFERENCE_BC_4_ALL_BLOCKS |  \
     QMI_NAS_BAND_PREFERENCE_BC_5_ALL_BLOCKS |  \
     QMI_NAS_BAND_PREFERENCE_BC_6            |  \
     QMI_NAS_BAND_PREFERENCE_BC_7            |  \
     QMI_NAS_BAND_PREFERENCE_BC_8            |  \
     QMI_NAS_BAND_PREFERENCE_BC_9            |  \
     QMI_NAS_BAND_PREFERENCE_BC_10           |  \
     QMI_NAS_BAND_PREFERENCE_BC_11           |  \
     QMI_NAS_BAND_PREFERENCE_BC_12           |  \
     QMI_NAS_BAND_PREFERENCE_BC_14           |  \
     QMI_NAS_BAND_PREFERENCE_BC_15           |  \
     QMI_NAS_BAND_PREFERENCE_BC_16           |  \
     QMI_NAS_BAND_PREFERENCE_BC_17           |  \
     QMI_NAS_BAND_PREFERENCE_BC_18           |  \
     QMI_NAS_BAND_PREFERENCE_BC_19)

#define ALL_3GPP_BANDS                          \
    (QMI_NAS_BAND_PREFERENCE_GSM_DCS_1800     | \
     QMI_NAS_BAND_PREFERENCE_GSM_900_EXTENDED | \
     QMI_NAS_BAND_PREFERENCE_GSM_900_PRIMARY  | \
     QMI_NAS_BAND_PREFERENCE_GSM_450          | \
     QMI_NAS_BAND_PREFERENCE_GSM_480          | \
     QMI_NAS_BAND_PREFERENCE_GSM_750          | \
     QMI_NAS_BAND_PREFERENCE_GSM_850          | \
     QMI_NAS_BAND_PREFERENCE_GSM_900_RAILWAYS | \
     QMI_NAS_BAND_PREFERENCE_GSM_PCS_1900     | \
     QMI_NAS_BAND_PREFERENCE_WCDMA_2100       | \
     QMI_NAS_BAND_PREFERENCE_WCDMA_PCS_1900   | \
     QMI_NAS_BAND_PREFERENCE_WCDMA_DCS_1800   | \
     QMI_NAS_BAND_PREFERENCE_WCDMA_1700_US    | \
     QMI_NAS_BAND_PREFERENCE_WCDMA_850_US     | \
     QMI_NAS_BAND_PREFERENCE_WCDMA_800        | \
     QMI_NAS_BAND_PREFERENCE_WCDMA_2600       | \
     QMI_NAS_BAND_PREFERENCE_WCDMA_900        | \
     QMI_NAS_BAND_PREFERENCE_WCDMA_1700_JAPAN)

MMModemCapability
mm_modem_capability_from_qmi_band_preference (QmiNasBandPreference qmi)
{
    MMModemCapability caps = MM_MODEM_CAPABILITY_NONE;

    if (qmi & ALL_3GPP_BANDS)
        caps |= MM_MODEM_CAPABILITY_GSM_UMTS;

    if (qmi & ALL_3GPP2_BANDS)
        caps |= MM_MODEM_CAPABILITY_CDMA_EVDO;

    return caps;
}

/*****************************************************************************/

MMModemMode
mm_modem_mode_from_qmi_gsm_wcdma_acquisition_order_preference (QmiNasGsmWcdmaAcquisitionOrderPreference qmi,
                                                               gpointer                                 log_object)
{
    switch (qmi) {
    case QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_AUTOMATIC:
        return MM_MODEM_MODE_NONE;
    case QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_GSM:
        return MM_MODEM_MODE_2G;
    case QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_WCDMA:
        return MM_MODEM_MODE_3G;
    default:
        mm_obj_dbg (log_object, "unknown acquisition order preference: '%s'",
                    qmi_nas_gsm_wcdma_acquisition_order_preference_get_string (qmi));
        return MM_MODEM_MODE_NONE;
    }
}

QmiNasGsmWcdmaAcquisitionOrderPreference
mm_modem_mode_to_qmi_gsm_wcdma_acquisition_order_preference (MMModemMode mode,
                                                             gpointer    log_object)
{
    g_autofree gchar *str = NULL;

    /* mode is not a mask in this case, only a value */

    switch (mode) {
    case MM_MODEM_MODE_3G:
        return QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_WCDMA;
    case MM_MODEM_MODE_2G:
        return QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_GSM;
    case MM_MODEM_MODE_NONE:
        return QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_AUTOMATIC;
    case MM_MODEM_MODE_CS:
    case MM_MODEM_MODE_4G:
    case MM_MODEM_MODE_5G:
    case MM_MODEM_MODE_ANY:
    default:
        break;
    }

    str = mm_modem_mode_build_string_from_mask (mode);
    mm_obj_dbg (log_object, "unhandled modem mode: '%s'", str);

    return QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_AUTOMATIC;
}

/*****************************************************************************/

MMModem3gppRegistrationState
mm_modem_3gpp_registration_state_from_qmi_registration_state (QmiNasAttachState attach_state,
                                                              QmiNasRegistrationState registration_state,
                                                              gboolean roaming)
{
    if (attach_state == QMI_NAS_ATTACH_STATE_UNKNOWN)
        return MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;

    if (attach_state == QMI_NAS_ATTACH_STATE_DETACHED)
        return MM_MODEM_3GPP_REGISTRATION_STATE_IDLE;

    /* attached */

    switch (registration_state) {
    case QMI_NAS_REGISTRATION_STATE_NOT_REGISTERED:
        return MM_MODEM_3GPP_REGISTRATION_STATE_IDLE;
    case QMI_NAS_REGISTRATION_STATE_REGISTERED:
        return (roaming ? MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING : MM_MODEM_3GPP_REGISTRATION_STATE_HOME);
    case QMI_NAS_REGISTRATION_STATE_NOT_REGISTERED_SEARCHING:
        return MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING;
    case QMI_NAS_REGISTRATION_STATE_REGISTRATION_DENIED:
        return MM_MODEM_3GPP_REGISTRATION_STATE_DENIED;
    case QMI_NAS_REGISTRATION_STATE_UNKNOWN:
    default:
        return MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;
    }
}

/*****************************************************************************/

MMModemCdmaRegistrationState
mm_modem_cdma_registration_state_from_qmi_registration_state (QmiNasRegistrationState registration_state)
{
    switch (registration_state) {
    case QMI_NAS_REGISTRATION_STATE_REGISTERED:
        return MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED;
    case QMI_NAS_REGISTRATION_STATE_NOT_REGISTERED:
    case QMI_NAS_REGISTRATION_STATE_NOT_REGISTERED_SEARCHING:
    case QMI_NAS_REGISTRATION_STATE_REGISTRATION_DENIED:
    case QMI_NAS_REGISTRATION_STATE_UNKNOWN:
    default:
        return MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
    }
}

/*****************************************************************************/

MMModemCdmaActivationState
mm_modem_cdma_activation_state_from_qmi_activation_state (QmiDmsActivationState state)
{
    switch (state) {
    case QMI_DMS_ACTIVATION_STATE_NOT_ACTIVATED:
        return MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED;
    case QMI_DMS_ACTIVATION_STATE_ACTIVATED:
        return MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED;
    case QMI_DMS_ACTIVATION_STATE_CONNECTING:
    case QMI_DMS_ACTIVATION_STATE_CONNECTED:
    case QMI_DMS_ACTIVATION_STATE_OTASP_AUTHENTICATED:
    case QMI_DMS_ACTIVATION_STATE_OTASP_NAM:
    case QMI_DMS_ACTIVATION_STATE_OTASP_MDN:
    case QMI_DMS_ACTIVATION_STATE_OTASP_IMSI:
    case QMI_DMS_ACTIVATION_STATE_OTASP_PRL:
    case QMI_DMS_ACTIVATION_STATE_OTASP_SPC:
        return MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING;
    case QMI_DMS_ACTIVATION_STATE_OTASP_COMMITED:
        return MM_MODEM_CDMA_ACTIVATION_STATE_PARTIALLY_ACTIVATED;

    default:
        return MM_MODEM_CDMA_ACTIVATION_STATE_UNKNOWN;
    }
}

/*****************************************************************************/

static void
process_common_info (const gchar                   *info_name,
                     QmiNasServiceStatus            service_status,
                     gboolean                       domain_valid,
                     QmiNasNetworkServiceDomain     domain,
                     gboolean                       roaming_status_valid,
                     QmiNasRoamingStatus            roaming_status,
                     gboolean                       forbidden_valid,
                     gboolean                       forbidden,
                     gboolean                       lac_valid,
                     guint16                        lac,
                     gboolean                       tac_valid,
                     guint16                        tac,
                     gboolean                       cid_valid,
                     guint32                        cid,
                     gboolean                       network_id_valid,
                     const gchar                   *mcc,
                     const gchar                   *mnc,
                     MMModem3gppRegistrationState  *out_cs_registration_state,
                     MMModem3gppRegistrationState  *out_ps_registration_state,
                     guint16                       *out_lac,
                     guint16                       *out_tac,
                     guint32                       *out_cid,
                     gchar                        **out_operator_id,
                     gpointer                       log_object)
{
#define SET_OUTPUT(OUT, VAL) do { \
        if (OUT)                  \
            *(OUT) = (VAL);       \
    } while (0)

    /* If power save service status reported the modem is not actively searching
     * for a network, therefore "idle". */
    if (service_status == QMI_NAS_SERVICE_STATUS_POWER_SAVE) {
        SET_OUTPUT (out_cs_registration_state, MM_MODEM_3GPP_REGISTRATION_STATE_IDLE);
        SET_OUTPUT (out_ps_registration_state, MM_MODEM_3GPP_REGISTRATION_STATE_IDLE);
        return;
    }

    /* If no service status reported, the modem is actively searching for a
     * network, therefore "searching". */
    if (service_status == QMI_NAS_SERVICE_STATUS_NONE) {
        SET_OUTPUT (out_cs_registration_state, MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING);
        SET_OUTPUT (out_ps_registration_state, MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING);
        return;
    }

    /* When forbidden, the service status is usually reported as 'limited' or
     * 'limited-regional', e.g. allowing only emergency calls. If the
     * forbidden flag is explicitly reported, we report 'denied', otherwise
     * 'emergency-services-only' */
    if (forbidden_valid && forbidden) {
        SET_OUTPUT (out_cs_registration_state, MM_MODEM_3GPP_REGISTRATION_STATE_DENIED);
        SET_OUTPUT (out_ps_registration_state, MM_MODEM_3GPP_REGISTRATION_STATE_DENIED);
    } else if (service_status == QMI_NAS_SERVICE_STATUS_LIMITED ||
               service_status == QMI_NAS_SERVICE_STATUS_LIMITED_REGIONAL) {
        SET_OUTPUT (out_cs_registration_state, MM_MODEM_3GPP_REGISTRATION_STATE_EMERGENCY_ONLY);
        SET_OUTPUT (out_ps_registration_state, MM_MODEM_3GPP_REGISTRATION_STATE_EMERGENCY_ONLY);
    }
    /* On a successful registration (either home or roaming) we require a valid
     * domain and "available" service status */
    else if (domain_valid &&
             domain != QMI_NAS_NETWORK_SERVICE_DOMAIN_CAMPED &&
             domain != QMI_NAS_NETWORK_SERVICE_DOMAIN_NONE &&
             service_status == QMI_NAS_SERVICE_STATUS_AVAILABLE) {
        MMModem3gppRegistrationState tmp_registration_state;

        if (roaming_status_valid && roaming_status == QMI_NAS_ROAMING_STATUS_ON)
            tmp_registration_state = MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING;
        else
            tmp_registration_state = MM_MODEM_3GPP_REGISTRATION_STATE_HOME;

        if (domain == QMI_NAS_NETWORK_SERVICE_DOMAIN_CS) {
            SET_OUTPUT (out_cs_registration_state, tmp_registration_state);
            SET_OUTPUT (out_ps_registration_state, MM_MODEM_3GPP_REGISTRATION_STATE_IDLE);
        } else if (domain == QMI_NAS_NETWORK_SERVICE_DOMAIN_PS) {
            SET_OUTPUT (out_cs_registration_state, MM_MODEM_3GPP_REGISTRATION_STATE_IDLE);
            SET_OUTPUT (out_ps_registration_state, tmp_registration_state);
        } else if (domain == QMI_NAS_NETWORK_SERVICE_DOMAIN_CS_PS) {
            SET_OUTPUT (out_cs_registration_state, tmp_registration_state);
            SET_OUTPUT (out_ps_registration_state, tmp_registration_state);
        }

        /* If we're registered either at home or roaming, try to get LAC/CID */
        if (lac_valid)
            SET_OUTPUT (out_lac, lac);
        if (tac_valid)
            SET_OUTPUT (out_tac, tac);
        if (cid_valid)
            SET_OUTPUT (out_cid, cid);
    } else {
        /* Issue a warning in case this ever happens and this logic needs to be updated */
        mm_obj_warn (log_object, "unexpected %s system info: domain valid %s, domain %s, service status %s",
                     info_name,
                     domain_valid ? "yes" : "no",
                     qmi_nas_network_service_domain_get_string (domain),
                     qmi_nas_service_status_get_string (service_status));
        return;
    }

    /* Network ID report also in the case of registration denial */
    if (network_id_valid) {
        *out_operator_id = g_malloc0 (7);
        memcpy (*out_operator_id, mcc, 3);
        if ((guint8)mnc[2] == 0xFF)
            memcpy (&((*out_operator_id)[3]), mnc, 2);
         else
            memcpy (&((*out_operator_id)[3]), mnc, 3);
    }
}

static void
process_gsm_info (QmiMessageNasGetSystemInfoOutput *response_output,
                  QmiIndicationNasSystemInfoOutput *indication_output,
                  MMModem3gppRegistrationState     *out_cs_registration_state,
                  MMModem3gppRegistrationState     *out_ps_registration_state,
                  guint16                          *out_lac,
                  guint32                          *out_cid,
                  gchar                           **out_operator_id,
                  gpointer                          log_object)
{
    QmiNasServiceStatus         service_status = QMI_NAS_SERVICE_STATUS_NONE;
    gboolean                    domain_valid = FALSE;
    QmiNasNetworkServiceDomain  domain = QMI_NAS_NETWORK_SERVICE_DOMAIN_NONE;
    gboolean                    roaming_status_valid = FALSE;
    QmiNasRoamingStatus         roaming_status = QMI_NAS_ROAMING_STATUS_OFF;
    gboolean                    forbidden_valid = FALSE;
    gboolean                    forbidden = FALSE;
    gboolean                    lac_valid = FALSE;
    guint16                     lac = 0;
    gboolean                    cid_valid = FALSE;
    guint32                     cid = 0;
    gboolean                    network_id_valid = FALSE;
    const gchar                *mcc = NULL;
    const gchar                *mnc = NULL;

    if (response_output) {
        /* if service status info not given, ACT is unsupported */
        if (!qmi_message_nas_get_system_info_output_get_gsm_service_status (
                response_output,
                &service_status,
                NULL, /* true_service_status */
                NULL, /* preferred_data_path */
                NULL))
            return;

        qmi_message_nas_get_system_info_output_get_gsm_system_info_v2 (
            response_output,
            &domain_valid,         &domain,
            NULL, NULL, /* service_capability */
            &roaming_status_valid, &roaming_status,
            &forbidden_valid,      &forbidden,
            &lac_valid,            &lac,
            &cid_valid,            &cid,
            NULL, NULL, NULL, /* registration_reject_info */
            &network_id_valid,     &mcc, &mnc,
            NULL, NULL, /* egprs support */
            NULL, NULL, /* dtm_support */
            NULL);
    } else if (indication_output) {
        /* if service status info not given, ACT is unsupported */
        if (!qmi_indication_nas_system_info_output_get_gsm_service_status (
                indication_output,
                &service_status,
                NULL, /* true_service_status */
                NULL, /* preferred_data_path */
                NULL))
            return;

        qmi_indication_nas_system_info_output_get_gsm_system_info_v2 (
            indication_output,
            &domain_valid,         &domain,
            NULL, NULL, /* service_capability */
            &roaming_status_valid, &roaming_status,
            &forbidden_valid,      &forbidden,
            &lac_valid,            &lac,
            &cid_valid,            &cid,
            NULL, NULL, NULL, /* registration_reject_info */
            &network_id_valid,     &mcc, &mnc,
            NULL, NULL, /* egprs support */
            NULL, NULL, /* dtm_support */
            NULL);
    } else
        g_assert_not_reached ();

    process_common_info ("GSM",
                         service_status,
                         domain_valid,         domain,
                         roaming_status_valid, roaming_status,
                         forbidden_valid,      forbidden,
                         lac_valid,            lac,
                         FALSE,                0,
                         cid_valid,            cid,
                         network_id_valid,     mcc, mnc,
                         out_cs_registration_state,
                         out_ps_registration_state,
                         out_lac,
                         NULL, /* out_tac */
                         out_cid,
                         out_operator_id,
                         log_object);
}

static void
process_wcdma_info (QmiMessageNasGetSystemInfoOutput *response_output,
                    QmiIndicationNasSystemInfoOutput *indication_output,
                    MMModem3gppRegistrationState     *out_cs_registration_state,
                    MMModem3gppRegistrationState     *out_ps_registration_state,
                    guint16                          *out_lac,
                    guint32                          *out_cid,
                    gchar                           **out_operator_id,
                    gpointer                          log_object)
{
    QmiNasServiceStatus         service_status = QMI_NAS_SERVICE_STATUS_NONE;
    gboolean                    domain_valid = FALSE;
    QmiNasNetworkServiceDomain  domain = QMI_NAS_NETWORK_SERVICE_DOMAIN_NONE;
    gboolean                    roaming_status_valid = FALSE;
    QmiNasRoamingStatus         roaming_status = QMI_NAS_ROAMING_STATUS_OFF;
    gboolean                    forbidden_valid = FALSE;
    gboolean                    forbidden = FALSE;
    gboolean                    lac_valid = FALSE;
    guint16                     lac = 0;
    gboolean                    cid_valid = FALSE;
    guint32                     cid = 0;
    gboolean                    network_id_valid = FALSE;
    const gchar                *mcc = NULL;
    const gchar                *mnc = NULL;

    if (response_output) {
        /* if service status info not given, ACT is unsupported */
        if (!qmi_message_nas_get_system_info_output_get_wcdma_service_status (
                response_output,
                &service_status,
                NULL, /* true_service_status */
                NULL, /* preferred_data_path */
                NULL))
            return;

        qmi_message_nas_get_system_info_output_get_wcdma_system_info_v2 (
            response_output,
            &domain_valid,         &domain,
            NULL, NULL, /* service_capability */
            &roaming_status_valid, &roaming_status,
            &forbidden_valid,      &forbidden,
            &lac_valid,            &lac,
            &cid_valid,            &cid,
            NULL, NULL, NULL, /* registration_reject_info */
            &network_id_valid,     &mcc, &mnc,
            NULL, NULL, /* hs_call_status */
            NULL, NULL, /* hs_service */
            NULL, NULL, /* primary_scrambling_code */
            NULL);
    } else if (indication_output) {
        /* if service status info not given, ACT is unsupported */
        if (!qmi_indication_nas_system_info_output_get_wcdma_service_status (
                indication_output,
                &service_status,
                NULL, /* true_service_status */
                NULL, /* preferred_data_path */
                NULL))
            return;

        qmi_indication_nas_system_info_output_get_wcdma_system_info_v2 (
            indication_output,
            &domain_valid,         &domain,
            NULL, NULL, /* service_capability */
            &roaming_status_valid, &roaming_status,
            &forbidden_valid,      &forbidden,
            &lac_valid,            &lac,
            &cid_valid,            &cid,
            NULL, NULL, NULL, /* registration_reject_info */
            &network_id_valid,     &mcc, &mnc,
            NULL, NULL, /* hs_call_status */
            NULL, NULL, /* hs_service */
            NULL, NULL, /* primary_scrambling_code */
            NULL);
    } else
        g_assert_not_reached ();

    process_common_info ("WCDMA",
                         service_status,
                         domain_valid,         domain,
                         roaming_status_valid, roaming_status,
                         forbidden_valid,      forbidden,
                         lac_valid,            lac,
                         FALSE,                0,
                         cid_valid,            cid,
                         network_id_valid,     mcc, mnc,
                         out_cs_registration_state,
                         out_ps_registration_state,
                         out_lac,
                         NULL, /* out_tac */
                         out_cid,
                         out_operator_id,
                         log_object);
}

static void
process_lte_info (QmiMessageNasGetSystemInfoOutput *response_output,
                  QmiIndicationNasSystemInfoOutput *indication_output,
                  MMModem3gppRegistrationState     *out_eps_registration_state,
                  guint16                          *out_tac,
                  guint32                          *out_cid,
                  gchar                           **out_operator_id,
                  gboolean                         *out_endc_available,
                  gpointer                          log_object)
{
    QmiNasServiceStatus         service_status = QMI_NAS_SERVICE_STATUS_NONE;
    gboolean                    domain_valid = FALSE;
    QmiNasNetworkServiceDomain  domain = QMI_NAS_NETWORK_SERVICE_DOMAIN_NONE;
    gboolean                    roaming_status_valid = FALSE;
    QmiNasRoamingStatus         roaming_status = QMI_NAS_ROAMING_STATUS_OFF;
    gboolean                    forbidden_valid = FALSE;
    gboolean                    forbidden = FALSE;
    gboolean                    tac_valid = FALSE;
    guint16                     tac = 0;
    gboolean                    cid_valid = FALSE;
    guint32                     cid = 0;
    gboolean                    network_id_valid = FALSE;
    const gchar                *mcc = NULL;
    const gchar                *mnc = NULL;

    if (response_output) {
        /* if service status info not given, ACT is unsupported */
        if (!qmi_message_nas_get_system_info_output_get_lte_service_status (
                response_output,
                &service_status,
                NULL, /* true_service_status */
                NULL, /* preferred_data_path */
                NULL))
            return;

        qmi_message_nas_get_system_info_output_get_lte_system_info_v2 (
            response_output,
            &domain_valid,         &domain,
            NULL, NULL, /* service_capability */
            &roaming_status_valid, &roaming_status,
            &forbidden_valid,      &forbidden,
            NULL, NULL, /* lac */
            &cid_valid,            &cid,
            NULL, NULL, NULL, /* registration_reject_info */
            &network_id_valid,     &mcc, &mnc,
            &tac_valid,            &tac,
            NULL);

        qmi_message_nas_get_system_info_output_get_eutra_with_nr5g_availability (
            response_output,
            out_endc_available,
            NULL);
    } else if (indication_output) {
        /* if service status info not given, ACT is unsupported */
        if (!qmi_indication_nas_system_info_output_get_lte_service_status (
                indication_output,
                &service_status,
                NULL, /* true_service_status */
                NULL, /* preferred_data_path */
                NULL))
            return;

        qmi_indication_nas_system_info_output_get_lte_system_info_v2 (
            indication_output,
            &domain_valid,         &domain,
            NULL, NULL, /* service_capability */
            &roaming_status_valid, &roaming_status,
            &forbidden_valid,      &forbidden,
            NULL, NULL, /* lac */
            &cid_valid,            &cid,
            NULL, NULL, NULL, /* registration_reject_info */
            &network_id_valid,     &mcc, &mnc,
            &tac_valid,            &tac,
            NULL);

        qmi_indication_nas_system_info_output_get_eutra_with_nr5g_availability (
            indication_output,
            out_endc_available,
            NULL);
    } else
        g_assert_not_reached ();

    process_common_info ("LTE",
                         service_status,
                         domain_valid,         domain,
                         roaming_status_valid, roaming_status,
                         forbidden_valid,      forbidden,
                         FALSE,                0,
                         tac_valid,            tac,
                         cid_valid,            cid,
                         network_id_valid,     mcc, mnc,
                         NULL, /* out_cs_registration_state */
                         out_eps_registration_state,
                         NULL, /* out_lac */
                         out_tac,
                         out_cid,
                         out_operator_id,
                         log_object);
}

static void
process_nr5g_info (QmiMessageNasGetSystemInfoOutput *response_output,
                   QmiIndicationNasSystemInfoOutput *indication_output,
                   MMModem3gppRegistrationState     *out_5gs_registration_state,
                   guint16                          *out_tac,
                   guint32                          *out_cid,
                   gchar                           **out_operator_id,
                   gpointer                          log_object)
{
    QmiNasServiceStatus         service_status = QMI_NAS_SERVICE_STATUS_NONE;
    gboolean                    domain_valid = FALSE;
    QmiNasNetworkServiceDomain  domain = QMI_NAS_NETWORK_SERVICE_DOMAIN_NONE;
    gboolean                    roaming_status_valid = FALSE;
    QmiNasRoamingStatus         roaming_status = QMI_NAS_ROAMING_STATUS_OFF;
    gboolean                    forbidden_valid = FALSE;
    gboolean                    forbidden = FALSE;
    gboolean                    tac_valid = FALSE;
    guint16                     tac = 0;
    gboolean                    cid_valid = FALSE;
    guint32                     cid = 0;
    gboolean                    network_id_valid = FALSE;
    const gchar                *mcc = NULL;
    const gchar                *mnc = NULL;

    if (response_output) {
        /* if service status info not given, ACT is unsupported */
        if (!qmi_message_nas_get_system_info_output_get_nr5g_service_status_info (
                response_output,
                &service_status,
                NULL, /* true_service_status */
                NULL, /* preferred_data_path */
                NULL))
            return;

        qmi_message_nas_get_system_info_output_get_nr5g_system_info (
            response_output,
            &domain_valid,         &domain,
            NULL, NULL, /* service_capability */
            &roaming_status_valid, &roaming_status,
            &forbidden_valid,      &forbidden,
            NULL, NULL, /* lac */
            &cid_valid,            &cid,
            NULL, NULL, NULL, /* registration_reject_info */
            &network_id_valid,     &mcc, &mnc,
            &tac_valid,            &tac,
            NULL);
    } else if (indication_output) {
        /* if service status info not given, ACT is unsupported */
        if (!qmi_indication_nas_system_info_output_get_nr5g_service_status_info (
                indication_output,
                &service_status,
                NULL, /* true_service_status */
                NULL, /* preferred_data_path */
                NULL))
            return;

        qmi_indication_nas_system_info_output_get_nr5g_system_info (
            indication_output,
            &domain_valid,         &domain,
            NULL, NULL, /* service_capability */
            &roaming_status_valid, &roaming_status,
            &forbidden_valid,      &forbidden,
            NULL, NULL, /* lac */
            &cid_valid,            &cid,
            NULL, NULL, NULL, /* registration_reject_info */
            &network_id_valid,     &mcc, &mnc,
            &tac_valid,            &tac,
            NULL);
    } else
        g_assert_not_reached ();

    process_common_info ("NR5G",
                         service_status,
                         domain_valid,         domain,
                         roaming_status_valid, roaming_status,
                         forbidden_valid,      forbidden,
                         FALSE,                0,
                         tac_valid,            tac,
                         cid_valid,            cid,
                         network_id_valid,     mcc, mnc,
                         NULL, /* cs_registration_state */
                         out_5gs_registration_state,
                         NULL, /* out_lac */
                         out_tac,
                         out_cid,
                         out_operator_id,
                         log_object);
}

static MMModem3gppRegistrationState
unregistered_state_fallback (MMModem3gppRegistrationState umts_state,
                             MMModem3gppRegistrationState gsm_state)
{
    /* For 3G and 2G unregistered states, we will select SEARCHING if one of them
     * reports it, otherwise DENIED if one of the reports it, and otherwise
     * the 3G one if not unknown and finally the 2G one otherwise. */
    if (umts_state == MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING || gsm_state == MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING)
        return MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING;
    if (umts_state == MM_MODEM_3GPP_REGISTRATION_STATE_DENIED || gsm_state == MM_MODEM_3GPP_REGISTRATION_STATE_DENIED)
        return MM_MODEM_3GPP_REGISTRATION_STATE_DENIED;
    if (umts_state != MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN)
        return umts_state;
    return gsm_state;
}

void
mm_modem_registration_state_from_qmi_system_info (QmiMessageNasGetSystemInfoOutput *response_output,
                                                  QmiIndicationNasSystemInfoOutput *indication_output,
                                                  MMModem3gppRegistrationState     *out_cs_registration_state,
                                                  MMModem3gppRegistrationState     *out_ps_registration_state,
                                                  MMModem3gppRegistrationState     *out_eps_registration_state,
                                                  MMModem3gppRegistrationState     *out_5gs_registration_state,
                                                  guint16                          *out_lac,
                                                  guint16                          *out_tac,
                                                  guint32                          *out_cid,
                                                  gchar                           **out_operator_id,
                                                  MMModemAccessTechnology          *out_act,
                                                  gpointer                          log_object)
{
    MMModem3gppRegistrationState  gsm_cs_registration_state = MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;
    MMModem3gppRegistrationState  gsm_ps_registration_state = MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;
    MMModem3gppRegistrationState  wcdma_cs_registration_state = MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;
    MMModem3gppRegistrationState  wcdma_ps_registration_state = MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;
    MMModem3gppRegistrationState  lte_eps_registration_state = MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;
    MMModem3gppRegistrationState  nr5g_5gs_registration_state = MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;
    g_autofree gchar             *gsm_operator_id = NULL;
    g_autofree gchar             *wcdma_operator_id = NULL;
    g_autofree gchar             *lte_operator_id = NULL;
    g_autofree gchar             *nr5g_operator_id = NULL;
    guint16                       gsm_lac = 0;
    guint32                       gsm_cid = 0;
    guint16                       wcdma_lac = 0;
    guint32                       wcdma_cid = 0;
    guint16                       lte_tac = 0;
    guint32                       lte_cid = 0;
    guint16                       nr5g_tac = 0;
    guint32                       nr5g_cid = 0;
    gboolean                      endc_available = FALSE;
    gboolean                      reg_info_set = FALSE;

    /* Reset outputs */
    *out_cs_registration_state = MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;
    *out_ps_registration_state = MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;
    *out_eps_registration_state = MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;
    *out_5gs_registration_state = MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;
    *out_lac = 0;
    *out_tac = 0;
    *out_cid = 0;
    *out_operator_id = NULL;
    *out_act = MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN;

    process_nr5g_info (response_output, indication_output,
                       &nr5g_5gs_registration_state,
                       &nr5g_tac, &nr5g_cid, &nr5g_operator_id,
                       log_object);

    process_lte_info (response_output, indication_output,
                      &lte_eps_registration_state,
                      &lte_tac, &lte_cid, &lte_operator_id,
                      &endc_available, log_object);

    process_wcdma_info (response_output, indication_output,
                        &wcdma_cs_registration_state, &wcdma_ps_registration_state,
                        &wcdma_lac, &wcdma_cid, &wcdma_operator_id,
                        log_object);

    process_gsm_info (response_output, indication_output,
                      &gsm_cs_registration_state, &gsm_ps_registration_state,
                      &gsm_lac, &gsm_cid, &gsm_operator_id,
                      log_object);

#define REG_INFO_SET(TAC,LAC,CID,OPERATOR_ID)                \
    if (!reg_info_set) {                                     \
        reg_info_set = TRUE;                                 \
        *out_tac = (TAC);                                    \
        *out_lac = (LAC);                                    \
        *out_cid = (CID);                                    \
        *out_operator_id = g_steal_pointer (&(OPERATOR_ID)); \
    }

    /* Process 5G data */
    *out_5gs_registration_state = nr5g_5gs_registration_state;
    if (mm_modem_3gpp_registration_state_is_registered (nr5g_5gs_registration_state)) {
        REG_INFO_SET (nr5g_tac, 0, nr5g_cid, nr5g_operator_id)
        *out_act |= MM_MODEM_ACCESS_TECHNOLOGY_5GNR;
    }

    /* Process 4G data */
    *out_eps_registration_state = lte_eps_registration_state;
    if (mm_modem_3gpp_registration_state_is_registered (lte_eps_registration_state)) {
        REG_INFO_SET (lte_tac, 0, lte_cid, lte_operator_id)
        *out_act |= MM_MODEM_ACCESS_TECHNOLOGY_LTE;
        if (endc_available)
            *out_act |= MM_MODEM_ACCESS_TECHNOLOGY_5GNR;
    }

    /* Process 2G/3G data */
    if (mm_modem_3gpp_registration_state_is_registered (wcdma_ps_registration_state)) {
        *out_ps_registration_state = wcdma_ps_registration_state;
        *out_act |= MM_MODEM_ACCESS_TECHNOLOGY_UMTS;
        REG_INFO_SET (0, wcdma_lac, wcdma_cid, wcdma_operator_id)
    } else if (mm_modem_3gpp_registration_state_is_registered (gsm_ps_registration_state)) {
        *out_ps_registration_state = gsm_ps_registration_state;
        *out_act |= MM_MODEM_ACCESS_TECHNOLOGY_GPRS;
        REG_INFO_SET (0, gsm_lac, gsm_cid, gsm_operator_id)
    } else {
        *out_ps_registration_state = unregistered_state_fallback (wcdma_ps_registration_state, gsm_ps_registration_state);
    }
    if (mm_modem_3gpp_registration_state_is_registered (wcdma_cs_registration_state)) {
        *out_cs_registration_state = wcdma_cs_registration_state;
        *out_act |= MM_MODEM_ACCESS_TECHNOLOGY_UMTS;
        REG_INFO_SET (0, wcdma_lac, wcdma_cid, wcdma_operator_id)
    } else if (mm_modem_3gpp_registration_state_is_registered (gsm_cs_registration_state)) {
        *out_cs_registration_state = gsm_cs_registration_state;
        *out_act |= MM_MODEM_ACCESS_TECHNOLOGY_GSM;
        REG_INFO_SET (0, gsm_lac, gsm_cid, gsm_operator_id)
    } else {
        *out_cs_registration_state = unregistered_state_fallback (wcdma_cs_registration_state, gsm_cs_registration_state);
    }

#undef REG_INFO_SET
}

/*****************************************************************************/

static const MMNetworkError qmi_mm_nw_errors[] = {
    [QMI_NAS_REJECT_CAUSE_NONE] = MM_NETWORK_ERROR_NONE,
    [QMI_NAS_REJECT_CAUSE_IMSI_UNKNOWN_IN_HLR] = MM_NETWORK_ERROR_IMSI_UNKNOWN_IN_HLR,
    [QMI_NAS_REJECT_CAUSE_ILLEGAL_UE] = MM_NETWORK_ERROR_ILLEGAL_MS,
    [QMI_NAS_REJECT_CAUSE_IMSI_UNKNOWN_IN_VLR] = MM_NETWORK_ERROR_IMSI_UNKNOWN_IN_VLR,
    [QMI_NAS_REJECT_CAUSE_IMEI_NOT_ACCEPTED] = MM_NETWORK_ERROR_IMEI_NOT_ACCEPTED,
    [QMI_NAS_REJECT_CAUSE_ILLEGAL_ME] = MM_NETWORK_ERROR_ILLEGAL_ME,
    [QMI_NAS_REJECT_CAUSE_PS_SERVICES_NOT_ALLOWED] = MM_NETWORK_ERROR_GPRS_NOT_ALLOWED,
    [QMI_NAS_REJECT_CAUSE_PS_AND_NON_PS_SERVICES_NOT_ALLOWED] = MM_NETWORK_ERROR_GPRS_AND_NON_GPRS_NOT_ALLOWED,
    [QMI_NAS_REJECT_CAUSE_UE_IDENTITY_NOT_DERIVED_BY_NETWORK] = MM_NETWORK_ERROR_MS_IDENTITY_NOT_DERIVED_BY_NETWORK,
    [QMI_NAS_REJECT_CAUSE_IMPLICITLY_DETACHED] = MM_NETWORK_ERROR_IMPLICITLY_DETACHED,
    [QMI_NAS_REJECT_CAUSE_PLMN_NOT_ALLOWED] = MM_NETWORK_ERROR_PLMN_NOT_ALLOWED,
    [QMI_NAS_REJECT_CAUSE_LOCATION_AREA_NOT_ALLOWED] = MM_NETWORK_ERROR_LOCATION_AREA_NOT_ALLOWED,
    [QMI_NAS_REJECT_CAUSE_ROAMING_IN_LOCATION_AREA_NOT_ALLOWED] = MM_NETWORK_ERROR_ROAMING_NOT_ALLOWED_IN_LOCATION_AREA,
    [QMI_NAS_REJECT_CAUSE_PS_SERVICES_IN_LOCATION_AREA_NOT_ALLOWED] = MM_NETWORK_ERROR_GPRS_NOT_ALLOWED_IN_PLMN,
    [QMI_NAS_REJECT_CAUSE_NO_SUITABLE_CELLS_IN_LOCATION_AREA] = MM_NETWORK_ERROR_NO_CELLS_IN_LOCATION_AREA,
    [QMI_NAS_REJECT_CAUSE_MSC_TEMPORARILY_NOT_REACHABLE] = MM_NETWORK_ERROR_MSC_TEMPORARILY_NOT_REACHABLE,
    [QMI_NAS_REJECT_CAUSE_NETWORK_FAILURE] = MM_NETWORK_ERROR_NETWORK_FAILURE,
    [QMI_NAS_REJECT_CAUSE_CS_DOMAIN_NOT_AVAILABLE] = MM_NETWORK_ERROR_CS_DOMAIN_NOT_AVAILABLE,
    [QMI_NAS_REJECT_CAUSE_ESM_FAILURE] = MM_NETWORK_ERROR_ESM_FAILURE,
    [QMI_NAS_REJECT_CAUSE_MAC_FAILURE] = MM_NETWORK_ERROR_MAC_FAILURE,
    [QMI_NAS_REJECT_CAUSE_SYNCH_FAILURE] = MM_NETWORK_ERROR_SYNCH_FAILURE,
    [QMI_NAS_REJECT_CAUSE_CONGESTION] = MM_NETWORK_ERROR_CONGESTION,
    [QMI_NAS_REJECT_CAUSE_CSG_NOT_AUTHORIZED] = MM_NETWORK_ERROR_NOT_AUTHORIZED_FOR_CSG,
    [QMI_NAS_REJECT_CAUSE_NON_EPS_AUTHENTICATION_UNACCEPTABLE] = MM_NETWORK_ERROR_GSM_AUTHENTICATION_UNACCEPTABLE,
    [QMI_NAS_REJECT_CAUSE_REDIRECTION_TO_5GCN_REQUIRED] = MM_NETWORK_ERROR_REDIRECTION_TO_5GCN_REQUIRED,
    [QMI_NAS_REJECT_CAUSE_SERVICE_OPTION_NOT_SUPPORTED] = MM_NETWORK_ERROR_SERVICE_OPTION_NOT_SUPPORTED,
    [QMI_NAS_REJECT_CAUSE_REQUESTED_SERVICE_OPTION_NOT_SUBSCRIBED] = MM_NETWORK_ERROR_REQUESTED_SERVICE_OPTION_NOT_SUBSCRIBED,
    [QMI_NAS_REJECT_CAUSE_SERVICE_OPTION_TEMPORARILY_OUT_OF_ORDER] = MM_NETWORK_ERROR_SERVICE_OPTION_TEMPORARILY_OUT_OF_ORDER,
    [QMI_NAS_REJECT_CAUSE_REQUESTED_SERVICE_OPTION_NOT_AUTHORIZED] = MM_NETWORK_ERROR_REQUESTED_SERVICE_OPTION_NOT_AUTHORIZED,
    [QMI_NAS_REJECT_CAUSE_CALL_CANNOT_BE_IDENTIFIED] = MM_NETWORK_ERROR_CALL_CANNOT_BE_IDENTIFIED,
    [QMI_NAS_REJECT_CAUSE_CS_SERVICE_TEMPORARILY_NOT_AVAILABLE] = MM_NETWORK_ERROR_CS_SERVICE_TEMPORARILY_NOT_AVAILABLE,
    [QMI_NAS_REJECT_CAUSE_NO_EPS_BEARER_CONTEXT_ACTIVATED] = MM_NETWORK_ERROR_NO_PDP_CONTEXT_ACTIVATED,
    [QMI_NAS_REJECT_CAUSE_SEVERE_NETWORK_FAILURE] = MM_NETWORK_ERROR_SYNTACTICAL_ERROR_IN_THE_TFT_OPERATION,
    [QMI_NAS_REJECT_CAUSE_SEMANTICALLY_INCORRECT_MESSAGE] = MM_NETWORK_ERROR_SEMANTICALLY_INCORRECT_MESSAGE,
    [QMI_NAS_REJECT_CAUSE_INVALID_MANDATORY_INFORMATION] = MM_NETWORK_ERROR_INVALID_MANDATORY_INFORMATION,
    [QMI_NAS_REJECT_CAUSE_MESSAGE_TYPE_NON_EXISTENT] = MM_NETWORK_ERROR_MESSAGE_TYPE_NON_EXISTENT_OR_NOT_IMPLEMENTED,
    [QMI_NAS_REJECT_CAUSE_MESSAGE_TYPE_NOT_COMPATIBLE] = MM_NETWORK_ERROR_MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE,
    [QMI_NAS_REJECT_CAUSE_INFORMATION_ELEMENT_NON_EXISTENT] = MM_NETWORK_ERROR_INFORMATION_ELEMENT_NON_EXISTENT_OR_NOT_IMPLEMENTED,
    [QMI_NAS_REJECT_CAUSE_CONDITIONAL_INFORMATION_ELEMENT_ERROR] = MM_NETWORK_ERROR_CONDITIONAL_IE_ERROR,
    [QMI_NAS_REJECT_CAUSE_MESSAGE_NOT_COMPATIBLE] = MM_NETWORK_ERROR_MESSAGE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE,
    [QMI_NAS_REJECT_CAUSE_UNSPECIFIED_PROTOCOL_ERROR] = MM_NETWORK_ERROR_PROTOCOL_ERROR_UNSPECIFIED,
};


MMNetworkError
mm_modem_nw_error_from_qmi_nw_error (QmiNasRejectCause nw_error)
{
    if (nw_error < G_N_ELEMENTS (qmi_mm_nw_errors)) {
        /* convert to nw error */
        return qmi_mm_nw_errors[nw_error];
    }

    /* fallback */
    return MM_NETWORK_ERROR_UNKNOWN;
}

/*****************************************************************************/

QmiWmsStorageType
mm_sms_storage_to_qmi_storage_type (MMSmsStorage storage)
{
    switch (storage) {
    case MM_SMS_STORAGE_SM:
        return QMI_WMS_STORAGE_TYPE_UIM;
    case MM_SMS_STORAGE_ME:
        return QMI_WMS_STORAGE_TYPE_NV;
    case MM_SMS_STORAGE_UNKNOWN:
    case MM_SMS_STORAGE_MT:
    case MM_SMS_STORAGE_SR:
    case MM_SMS_STORAGE_BM:
    case MM_SMS_STORAGE_TA:
    default:
        return QMI_WMS_STORAGE_TYPE_NONE;
    }
}

MMSmsStorage
mm_sms_storage_from_qmi_storage_type (QmiWmsStorageType qmi_storage)
{
    switch (qmi_storage) {
    case QMI_WMS_STORAGE_TYPE_UIM:
        return MM_SMS_STORAGE_SM;
    case QMI_WMS_STORAGE_TYPE_NV:
        return MM_SMS_STORAGE_ME;
    case QMI_WMS_STORAGE_TYPE_NONE:
    default:
        return MM_SMS_STORAGE_UNKNOWN;
    }
}

/*****************************************************************************/

MMSmsState
mm_sms_state_from_qmi_message_tag (QmiWmsMessageTagType tag)
{
    switch (tag) {
    case QMI_WMS_MESSAGE_TAG_TYPE_MT_READ:
    case QMI_WMS_MESSAGE_TAG_TYPE_MT_NOT_READ:
        return MM_SMS_STATE_RECEIVED;
    case QMI_WMS_MESSAGE_TAG_TYPE_MO_SENT:
        return MM_SMS_STATE_SENT;
    case QMI_WMS_MESSAGE_TAG_TYPE_MO_NOT_SENT:
        return MM_SMS_STATE_STORED;
    default:
        return MM_SMS_STATE_UNKNOWN;
    }
}

/*****************************************************************************/

MMCbmState
mm_cbm_state_from_qmi_message_tag (QmiWmsMessageTagType tag)
{
    switch (tag) {
    case QMI_WMS_MESSAGE_TAG_TYPE_MT_READ:
    case QMI_WMS_MESSAGE_TAG_TYPE_MT_NOT_READ:
        return MM_CBM_STATE_RECEIVED;
    case QMI_WMS_MESSAGE_TAG_TYPE_MO_SENT:
    case QMI_WMS_MESSAGE_TAG_TYPE_MO_NOT_SENT:
    default:
        return MM_CBM_STATE_UNKNOWN;
    }
}

/*****************************************************************************/

QmiWdsAuthentication
mm_bearer_allowed_auth_to_qmi_authentication (MMBearerAllowedAuth   auth,
                                              gpointer              log_object,
                                              GError              **error)
{
    QmiWdsAuthentication out;

    if (auth == MM_BEARER_ALLOWED_AUTH_UNKNOWN) {
        mm_obj_dbg (log_object, "using default (CHAP) authentication method");
        return QMI_WDS_AUTHENTICATION_CHAP;
    }

    if (auth == MM_BEARER_ALLOWED_AUTH_NONE)
        return QMI_WDS_AUTHENTICATION_NONE;

    /* otherwise find a bitmask that matches the input bitmask */
    out = QMI_WDS_AUTHENTICATION_NONE;
    if (auth & MM_BEARER_ALLOWED_AUTH_PAP)
        out |= QMI_WDS_AUTHENTICATION_PAP;
    if (auth & MM_BEARER_ALLOWED_AUTH_CHAP)
        out |= QMI_WDS_AUTHENTICATION_CHAP;

    /* and if the bitmask cannot be built, error out */
    if (out == QMI_WDS_AUTHENTICATION_NONE) {
        g_autofree gchar *str = NULL;

        str = mm_bearer_allowed_auth_build_string_from_mask (auth);
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_UNSUPPORTED,
                     "Unsupported authentication methods (%s)",
                     str);
    }
    return out;
}

MMBearerAllowedAuth
mm_bearer_allowed_auth_from_qmi_authentication (QmiWdsAuthentication auth)
{
    MMBearerAllowedAuth out = 0;

    /* Exact match for NONE */
    if (auth == QMI_WDS_AUTHENTICATION_NONE)
        return MM_BEARER_ALLOWED_AUTH_NONE;

    if (auth & QMI_WDS_AUTHENTICATION_PAP)
        out |= MM_BEARER_ALLOWED_AUTH_PAP;
    if (auth & QMI_WDS_AUTHENTICATION_CHAP)
        out |= MM_BEARER_ALLOWED_AUTH_CHAP;

    return out;
}

MMBearerIpFamily
mm_bearer_ip_family_from_qmi_ip_support_type (QmiWdsIpSupportType ip_support_type)
{
    switch (ip_support_type) {
    case QMI_WDS_IP_SUPPORT_TYPE_IPV4:
        return MM_BEARER_IP_FAMILY_IPV4;
    case QMI_WDS_IP_SUPPORT_TYPE_IPV6:
        return MM_BEARER_IP_FAMILY_IPV6;
    case QMI_WDS_IP_SUPPORT_TYPE_IPV4V6:
        return MM_BEARER_IP_FAMILY_IPV4V6;
    default:
        return MM_BEARER_IP_FAMILY_NONE;
    }
}

MMBearerIpFamily
mm_bearer_ip_family_from_qmi_pdp_type (QmiWdsPdpType pdp_type)
{
    switch (pdp_type) {
    case QMI_WDS_PDP_TYPE_IPV4:
        return MM_BEARER_IP_FAMILY_IPV4;
    case QMI_WDS_PDP_TYPE_IPV6:
        return MM_BEARER_IP_FAMILY_IPV6;
    case QMI_WDS_PDP_TYPE_IPV4_OR_IPV6:
        return MM_BEARER_IP_FAMILY_IPV4V6;
    case QMI_WDS_PDP_TYPE_PPP:
    default:
        return MM_BEARER_IP_FAMILY_NONE;
    }
}

gboolean
mm_bearer_ip_family_to_qmi_pdp_type (MMBearerIpFamily  ip_family,
                                     QmiWdsPdpType    *out_pdp_type)
{
    switch (ip_family) {
    case MM_BEARER_IP_FAMILY_IPV4:
        *out_pdp_type = QMI_WDS_PDP_TYPE_IPV4;
        return TRUE;
    case MM_BEARER_IP_FAMILY_IPV6:
        *out_pdp_type =  QMI_WDS_PDP_TYPE_IPV6;
        return TRUE;
    case MM_BEARER_IP_FAMILY_IPV4V6:
        *out_pdp_type =  QMI_WDS_PDP_TYPE_IPV4_OR_IPV6;
        return TRUE;
    case MM_BEARER_IP_FAMILY_NON_IP:
    case MM_BEARER_IP_FAMILY_NONE:
    case MM_BEARER_IP_FAMILY_ANY:
    default:
        /* there is no valid conversion, so just return FALSE to indicate it */
        return FALSE;
    }
}

QmiWdsApnTypeMask
mm_bearer_apn_type_to_qmi_apn_type (MMBearerApnType apn_type,
                                    gpointer        log_object)
{
    guint64 value = 0;

    if (apn_type == MM_BEARER_APN_TYPE_NONE) {
        mm_obj_dbg (log_object, "using default (internet) APN type");
        return QMI_WDS_APN_TYPE_MASK_DEFAULT;
    }

    if (apn_type & MM_BEARER_APN_TYPE_DEFAULT)
        value |= QMI_WDS_APN_TYPE_MASK_DEFAULT;
    if (apn_type & MM_BEARER_APN_TYPE_IMS)
        value |= QMI_WDS_APN_TYPE_MASK_IMS;
    if (apn_type & MM_BEARER_APN_TYPE_MMS)
        value |= QMI_WDS_APN_TYPE_MASK_MMS;
    if (apn_type & MM_BEARER_APN_TYPE_TETHERING)
        value |= QMI_WDS_APN_TYPE_MASK_DUN;
    if (apn_type & MM_BEARER_APN_TYPE_MANAGEMENT)
        value |= QMI_WDS_APN_TYPE_MASK_FOTA;
    if (apn_type & MM_BEARER_APN_TYPE_INITIAL)
        value |= QMI_WDS_APN_TYPE_MASK_IA;
    if (apn_type & MM_BEARER_APN_TYPE_EMERGENCY)
        value |= QMI_WDS_APN_TYPE_MASK_EMERGENCY;
    return value;
}

MMBearerApnType
mm_bearer_apn_type_from_qmi_apn_type (QmiWdsApnTypeMask apn_type)
{
    MMBearerApnType value = MM_BEARER_APN_TYPE_NONE;

    if (apn_type & QMI_WDS_APN_TYPE_MASK_DEFAULT)
        value |= MM_BEARER_APN_TYPE_DEFAULT;
    if (apn_type & QMI_WDS_APN_TYPE_MASK_IMS)
        value |= MM_BEARER_APN_TYPE_IMS;
    if (apn_type & QMI_WDS_APN_TYPE_MASK_MMS)
        value |= MM_BEARER_APN_TYPE_MMS;
    if (apn_type & QMI_WDS_APN_TYPE_MASK_DUN)
        value |= MM_BEARER_APN_TYPE_TETHERING;
    if (apn_type & QMI_WDS_APN_TYPE_MASK_FOTA)
        value |= MM_BEARER_APN_TYPE_MANAGEMENT;
    if (apn_type & QMI_WDS_APN_TYPE_MASK_IA)
        value |= MM_BEARER_APN_TYPE_INITIAL;
    if (apn_type & QMI_WDS_APN_TYPE_MASK_EMERGENCY)
        value |= MM_BEARER_APN_TYPE_EMERGENCY;
    return value;
}

/*****************************************************************************/
/* QMI/WDA to MM translations */

QmiDataEndpointType
mm_port_net_driver_to_qmi_endpoint_type (const gchar *net_driver)
{
    if (!g_strcmp0 (net_driver, "qmi_wwan"))
        return QMI_DATA_ENDPOINT_TYPE_HSUSB;
    if (!g_strcmp0 (net_driver, "mhi_net"))
        return QMI_DATA_ENDPOINT_TYPE_PCIE;
    if (!g_strcmp0 (net_driver, "ipa"))
        return QMI_DATA_ENDPOINT_TYPE_EMBEDDED;
    if (!g_strcmp0 (net_driver, "bam-dmux"))
        return QMI_DATA_ENDPOINT_TYPE_BAM_DMUX;

    return QMI_DATA_ENDPOINT_TYPE_UNKNOWN;
}

/*****************************************************************************/

/**
 * The only case where we need to apply some logic to decide what the current
 * capabilities are is when we have a multimode CDMA/EVDO+GSM/UMTS device, in
 * which case we'll check the SSP and TP current values to decide which
 * capabilities are present and which have been disabled.
 *
 * For all the other cases, the DMS capabilities are exactly the current ones,
 * as there would be no capability switching support.
 */
MMModemCapability
mm_current_capability_from_qmi_current_capabilities_context (MMQmiCurrentCapabilitiesContext *ctx,
                                                             gpointer                         log_object)
{
    MMModemCapability tmp = MM_MODEM_CAPABILITY_NONE;
    g_autofree gchar *nas_ssp_mode_preference_str = NULL;
    g_autofree gchar *nas_tp_str = NULL;
    g_autofree gchar *dms_capabilities_str = NULL;
    g_autofree gchar *tmp_str = NULL;

    /* If not a multimode device, we're done */
    if (!ctx->multimode) {
        if (ctx->dms_capabilities != MM_MODEM_CAPABILITY_NONE)
            tmp = ctx->dms_capabilities;
        /* SSP logic to gather capabilities uses the Mode Preference TLV if available */
        else if (ctx->nas_ssp_mode_preference_mask)
            tmp = mm_modem_capability_from_qmi_rat_mode_preference (ctx->nas_ssp_mode_preference_mask);
        /* If no value retrieved from SSP, check TP. We only process TP
         * values if not 'auto' (0). */
        else if (ctx->nas_tp_mask != QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_AUTO)
            tmp = mm_modem_capability_from_qmi_radio_technology_preference (ctx->nas_tp_mask);
    } else {
        /* We have a multimode CDMA/EVDO+GSM/UMTS device, check SSP and TP */

        /* SSP logic to gather capabilities uses the Mode Preference TLV if available */
        if (ctx->nas_ssp_mode_preference_mask)
            tmp = mm_modem_capability_from_qmi_rat_mode_preference (ctx->nas_ssp_mode_preference_mask);
        /* If no value retrieved from SSP, check TP. We only process TP
         * values if not 'auto' (0). */
        else if (ctx->nas_tp_mask != QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_AUTO)
            tmp = mm_modem_capability_from_qmi_radio_technology_preference (ctx->nas_tp_mask);

        /* Final capabilities are the union of the active multimode capability
         * (GSM/UMTS or CDMA/EVDO or both or none) in TP or SSP and other supported device's capabilities.
         * If the Technology Preference was "auto" or unknown we just fall back
         * to the Get Capabilities response.
         */
        if (tmp == MM_MODEM_CAPABILITY_NONE)
            tmp = ctx->dms_capabilities;
        else
            tmp = (tmp & MM_MODEM_CAPABILITY_MULTIMODE) | (MM_MODEM_CAPABILITY_MULTIMODE ^ ctx->dms_capabilities);
    }

    /* Log about the logic applied */
    nas_ssp_mode_preference_str = qmi_nas_rat_mode_preference_build_string_from_mask (ctx->nas_ssp_mode_preference_mask);
    nas_tp_str = qmi_nas_radio_technology_preference_build_string_from_mask (ctx->nas_tp_mask);
    dms_capabilities_str = mm_modem_capability_build_string_from_mask (ctx->dms_capabilities);
    tmp_str = mm_modem_capability_build_string_from_mask (tmp);
    mm_obj_dbg (log_object,
                "Current capabilities built: '%s'\n"
                "  SSP mode preference: '%s'\n"
                "  TP: '%s'\n"
                "  DMS Capabilities: '%s'",
                tmp_str,
                nas_ssp_mode_preference_str ? nas_ssp_mode_preference_str : "unknown",
                nas_tp_str ? nas_tp_str : "unknown",
                dms_capabilities_str);

    return tmp;
}

/*****************************************************************************/
/* Utility to build list of supported capabilities */

GArray *
mm_supported_capabilities_from_qmi_supported_capabilities_context (MMQmiSupportedCapabilitiesContext *ctx,
                                                                   gpointer                           log_object)
{
    GArray *supported_combinations;

    supported_combinations = g_array_sized_new (FALSE, FALSE, sizeof (MMModemCapability), 4);

    /* Add all possible supported capability combinations.
     * In order to avoid unnecessary modem reboots, we will only implement capabilities
     * switching only when switching GSM/UMTS+CDMA/EVDO multimode devices, and only if
     * we have support for the commands doing it.
     */
    if ((ctx->nas_tp_supported || ctx->nas_ssp_supported) && ctx->multimode) {
        MMModemCapability single;

        /* Multimode GSM/UMTS+CDMA/EVDO+(LTE/5GNR) device switched to GSM/UMTS+(LTE/5GNR) device */
        single = MM_MODEM_CAPABILITY_GSM_UMTS | (MM_MODEM_CAPABILITY_MULTIMODE ^ ctx->dms_capabilities);
        g_array_append_val (supported_combinations, single);
        /* Multimode GSM/UMTS+CDMA/EVDO+(LTE/5GNR) device switched to CDMA/EVDO+(LTE/5GNR) device */
        single = MM_MODEM_CAPABILITY_CDMA_EVDO | (MM_MODEM_CAPABILITY_MULTIMODE ^ ctx->dms_capabilities);
        g_array_append_val (supported_combinations, single);
        /*
         * Multimode GSM/UMTS+CDMA/EVDO+(LTE/5GNR) device switched to (LTE/5GNR) device
         *
         * This case is required because we use the same methods and operations to
         * switch capabilities and modes.
         */
        if ((single = (MM_MODEM_CAPABILITY_MULTIMODE ^ ctx->dms_capabilities)))
            g_array_append_val (supported_combinations, single);
    }

    /* Add the full mask itself */
    g_array_append_val (supported_combinations, ctx->dms_capabilities);

    return supported_combinations;
}

/*****************************************************************************/
/* Utility to build list of supported modes */

GArray *
mm_supported_modes_from_qmi_supported_modes_context (MMQmiSupportedModesContext  *ctx,
                                                     gpointer                     log_object,
                                                     GError                     **error)
{
    g_autoptr(GArray)       combinations = NULL;
    g_autoptr(GArray)       all = NULL;
    MMModemModeCombination  mode;

    if (ctx->all == MM_MODEM_MODE_NONE) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "No supported modes reported");
        return NULL;
    }

    /* Start with a mode including ALL */
    mode.allowed = ctx->all;
    mode.preferred = MM_MODEM_MODE_NONE;
    all = g_array_sized_new (FALSE, FALSE, sizeof (MMModemModeCombination), 1);
    g_array_append_val (all, mode);

    /* If SSP and TP are not supported, ignore supported mode management */
    if (!ctx->nas_ssp_supported && !ctx->nas_tp_supported)
        return g_steal_pointer (&all);

    combinations = g_array_new (FALSE, FALSE, sizeof (MMModemModeCombination));

#define ADD_MODE_PREFERENCE(MODE1, MODE2, MODE3, MODE4) do {            \
        mode.allowed = MODE1;                                           \
        if (MODE2 != MM_MODEM_MODE_NONE) {                              \
            mode.allowed |= MODE2;                                      \
            if (MODE3 != MM_MODEM_MODE_NONE) {                          \
                mode.allowed |= MODE3;                                  \
                if (MODE4 != MM_MODEM_MODE_NONE)                        \
                    mode.allowed |= MODE4;                              \
            }                                                           \
            if (ctx->nas_ssp_supported) {                               \
                if (MODE3 != MM_MODEM_MODE_NONE) {                      \
                    if (MODE4 != MM_MODEM_MODE_NONE) {                  \
                        mode.preferred = MODE4;                         \
                        g_array_append_val (combinations, mode);        \
                    }                                                   \
                    mode.preferred = MODE3;                             \
                    g_array_append_val (combinations, mode);            \
                }                                                       \
                mode.preferred = MODE2;                                 \
                g_array_append_val (combinations, mode);                \
                mode.preferred = MODE1;                                 \
                g_array_append_val (combinations, mode);                \
            } else {                                                    \
                mode.preferred = MM_MODEM_MODE_NONE;                    \
                g_array_append_val (combinations, mode);                \
            }                                                           \
        } else {                                                        \
            mode.allowed = MODE1;                                       \
            mode.preferred = MM_MODEM_MODE_NONE;                        \
            g_array_append_val (combinations, mode);                    \
        }                                                               \
    } while (0)

    /* 2G-only, 3G-only */
    ADD_MODE_PREFERENCE (MM_MODEM_MODE_2G, MM_MODEM_MODE_NONE, MM_MODEM_MODE_NONE, MM_MODEM_MODE_NONE);
    ADD_MODE_PREFERENCE (MM_MODEM_MODE_3G, MM_MODEM_MODE_NONE, MM_MODEM_MODE_NONE, MM_MODEM_MODE_NONE);

    /*
     * This case is required because we use the same methods and operations to
     * switch capabilities and modes. For the LTE capability there is a direct
     * related 4G mode, and so we cannot select a '4G only' mode in this device
     * because we wouldn't be able to know the full list of current capabilities
     * if the device was rebooted, as we would only see LTE capability. So,
     * handle this special case so that the LTE/4G-only mode can exclusively be
     * selected as capability switching in this kind of devices.
     */
    if (!ctx->multimode || !(ctx->current_capabilities & MM_MODEM_CAPABILITY_MULTIMODE)) {
        /* 4G-only */
        ADD_MODE_PREFERENCE (MM_MODEM_MODE_4G, MM_MODEM_MODE_NONE, MM_MODEM_MODE_NONE, MM_MODEM_MODE_NONE);
    }

    /* 2G, 3G, 4G combinations */
    ADD_MODE_PREFERENCE (MM_MODEM_MODE_2G, MM_MODEM_MODE_3G, MM_MODEM_MODE_NONE, MM_MODEM_MODE_NONE);
    ADD_MODE_PREFERENCE (MM_MODEM_MODE_2G, MM_MODEM_MODE_4G, MM_MODEM_MODE_NONE, MM_MODEM_MODE_NONE);
    ADD_MODE_PREFERENCE (MM_MODEM_MODE_3G, MM_MODEM_MODE_4G, MM_MODEM_MODE_NONE, MM_MODEM_MODE_NONE);
    ADD_MODE_PREFERENCE (MM_MODEM_MODE_2G, MM_MODEM_MODE_3G, MM_MODEM_MODE_4G,   MM_MODEM_MODE_NONE);

    /* 5G related mode combinations are only supported when NAS SSP is supported,
     * as there is no 5G support in NAS TP. */
    if (ctx->nas_ssp_supported) {
        /* Same reasoning as for the special 4G-only case above */
        if (!ctx->multimode || !(ctx->current_capabilities & MM_MODEM_CAPABILITY_MULTIMODE)) {
            ADD_MODE_PREFERENCE (MM_MODEM_MODE_5G, MM_MODEM_MODE_NONE, MM_MODEM_MODE_NONE, MM_MODEM_MODE_NONE);
            ADD_MODE_PREFERENCE (MM_MODEM_MODE_4G, MM_MODEM_MODE_5G,   MM_MODEM_MODE_NONE, MM_MODEM_MODE_NONE);
        }
        ADD_MODE_PREFERENCE (MM_MODEM_MODE_2G, MM_MODEM_MODE_5G,   MM_MODEM_MODE_NONE, MM_MODEM_MODE_NONE);
        ADD_MODE_PREFERENCE (MM_MODEM_MODE_3G, MM_MODEM_MODE_5G,   MM_MODEM_MODE_NONE, MM_MODEM_MODE_NONE);
        ADD_MODE_PREFERENCE (MM_MODEM_MODE_2G, MM_MODEM_MODE_3G,   MM_MODEM_MODE_5G,   MM_MODEM_MODE_NONE);
        ADD_MODE_PREFERENCE (MM_MODEM_MODE_2G, MM_MODEM_MODE_4G,   MM_MODEM_MODE_5G,   MM_MODEM_MODE_NONE);
        ADD_MODE_PREFERENCE (MM_MODEM_MODE_3G, MM_MODEM_MODE_4G,   MM_MODEM_MODE_5G,   MM_MODEM_MODE_NONE);
        ADD_MODE_PREFERENCE (MM_MODEM_MODE_2G, MM_MODEM_MODE_3G,   MM_MODEM_MODE_4G,   MM_MODEM_MODE_5G);
    }

    /* Filter out unsupported modes */
    return mm_filter_supported_modes (all, combinations, log_object);
}

/*****************************************************************************/

MMOmaSessionType
mm_oma_session_type_from_qmi_oma_session_type (QmiOmaSessionType qmi_session_type)
{
    switch (qmi_session_type) {
    case QMI_OMA_SESSION_TYPE_CLIENT_INITIATED_DEVICE_CONFIGURE:
        return MM_OMA_SESSION_TYPE_CLIENT_INITIATED_DEVICE_CONFIGURE;
    case QMI_OMA_SESSION_TYPE_CLIENT_INITIATED_PRL_UPDATE:
        return MM_OMA_SESSION_TYPE_CLIENT_INITIATED_PRL_UPDATE;
    case QMI_OMA_SESSION_TYPE_CLIENT_INITIATED_HANDS_FREE_ACTIVATION:
        return MM_OMA_SESSION_TYPE_CLIENT_INITIATED_HANDS_FREE_ACTIVATION;
    case QMI_OMA_SESSION_TYPE_DEVICE_INITIATED_HANDS_FREE_ACTIVATION:
        return MM_OMA_SESSION_TYPE_DEVICE_INITIATED_HANDS_FREE_ACTIVATION;
    case QMI_OMA_SESSION_TYPE_NETWORK_INITIATED_PRL_UPDATE:
        return MM_OMA_SESSION_TYPE_NETWORK_INITIATED_PRL_UPDATE;
    case QMI_OMA_SESSION_TYPE_NETWORK_INITIATED_DEVICE_CONFIGURE:
        return MM_OMA_SESSION_TYPE_NETWORK_INITIATED_DEVICE_CONFIGURE;
    case QMI_OMA_SESSION_TYPE_DEVICE_INITIATED_PRL_UPDATE:
        return MM_OMA_SESSION_TYPE_DEVICE_INITIATED_PRL_UPDATE;
    default:
        return MM_OMA_SESSION_TYPE_UNKNOWN;
    }
}

QmiOmaSessionType
mm_oma_session_type_to_qmi_oma_session_type (MMOmaSessionType mm_session_type)
{
    switch (mm_session_type) {
    case MM_OMA_SESSION_TYPE_CLIENT_INITIATED_DEVICE_CONFIGURE:
        return QMI_OMA_SESSION_TYPE_CLIENT_INITIATED_DEVICE_CONFIGURE;
    case MM_OMA_SESSION_TYPE_CLIENT_INITIATED_PRL_UPDATE:
        return QMI_OMA_SESSION_TYPE_CLIENT_INITIATED_PRL_UPDATE;
    case MM_OMA_SESSION_TYPE_CLIENT_INITIATED_HANDS_FREE_ACTIVATION:
        return QMI_OMA_SESSION_TYPE_CLIENT_INITIATED_HANDS_FREE_ACTIVATION;
    case MM_OMA_SESSION_TYPE_DEVICE_INITIATED_HANDS_FREE_ACTIVATION:
        return QMI_OMA_SESSION_TYPE_DEVICE_INITIATED_HANDS_FREE_ACTIVATION;
    case MM_OMA_SESSION_TYPE_NETWORK_INITIATED_PRL_UPDATE:
        return QMI_OMA_SESSION_TYPE_NETWORK_INITIATED_PRL_UPDATE;
    case MM_OMA_SESSION_TYPE_NETWORK_INITIATED_DEVICE_CONFIGURE:
        return QMI_OMA_SESSION_TYPE_NETWORK_INITIATED_DEVICE_CONFIGURE;
    case MM_OMA_SESSION_TYPE_DEVICE_INITIATED_PRL_UPDATE:
        return QMI_OMA_SESSION_TYPE_DEVICE_INITIATED_PRL_UPDATE;
    case MM_OMA_SESSION_TYPE_UNKNOWN:
    default:
        g_assert_not_reached ();
    }
}

MMOmaSessionState
mm_oma_session_state_from_qmi_oma_session_state (QmiOmaSessionState qmi_session_state)
{
    /* Note: MM_OMA_SESSION_STATE_STARTED is not a state received from the modem */

    switch (qmi_session_state) {
    case QMI_OMA_SESSION_STATE_COMPLETE_INFORMATION_UPDATED:
    case QMI_OMA_SESSION_STATE_COMPLETE_UPDATED_INFORMATION_UNAVAILABLE:
        return MM_OMA_SESSION_STATE_COMPLETED;
    case QMI_OMA_SESSION_STATE_FAILED:
        return MM_OMA_SESSION_STATE_FAILED;
    case QMI_OMA_SESSION_STATE_RETRYING:
        return MM_OMA_SESSION_STATE_RETRYING;
    case QMI_OMA_SESSION_STATE_CONNECTING:
        return MM_OMA_SESSION_STATE_CONNECTING;
    case QMI_OMA_SESSION_STATE_CONNECTED:
        return MM_OMA_SESSION_STATE_CONNECTED;
    case QMI_OMA_SESSION_STATE_AUTHENTICATED:
        return MM_OMA_SESSION_STATE_AUTHENTICATED;
    case QMI_OMA_SESSION_STATE_MDN_DOWNLOADED:
        return MM_OMA_SESSION_STATE_MDN_DOWNLOADED;
    case QMI_OMA_SESSION_STATE_MSID_DOWNLOADED:
        return MM_OMA_SESSION_STATE_MSID_DOWNLOADED;
    case QMI_OMA_SESSION_STATE_PRL_DOWNLOADED:
        return MM_OMA_SESSION_STATE_PRL_DOWNLOADED;
    case QMI_OMA_SESSION_STATE_MIP_PROFILE_DOWNLOADED:
        return MM_OMA_SESSION_STATE_MIP_PROFILE_DOWNLOADED;
    default:
        return MM_OMA_SESSION_STATE_UNKNOWN;
    }
}

/*****************************************************************************/

MMOmaSessionStateFailedReason
mm_oma_session_state_failed_reason_from_qmi_oma_session_failed_reason (QmiOmaSessionFailedReason qmi_session_failed_reason)
{
    switch (qmi_session_failed_reason) {
    case QMI_OMA_SESSION_FAILED_REASON_UNKNOWN:
        return MM_OMA_SESSION_STATE_FAILED_REASON_UNKNOWN;
    case QMI_OMA_SESSION_FAILED_REASON_NETWORK_UNAVAILABLE:
        return MM_OMA_SESSION_STATE_FAILED_REASON_NETWORK_UNAVAILABLE;
    case QMI_OMA_SESSION_FAILED_REASON_SERVER_UNAVAILABLE:
        return MM_OMA_SESSION_STATE_FAILED_REASON_SERVER_UNAVAILABLE;
    case QMI_OMA_SESSION_FAILED_REASON_AUTHENTICATION_FAILED:
        return MM_OMA_SESSION_STATE_FAILED_REASON_AUTHENTICATION_FAILED;
    case QMI_OMA_SESSION_FAILED_REASON_MAX_RETRY_EXCEEDED:
        return MM_OMA_SESSION_STATE_FAILED_REASON_MAX_RETRY_EXCEEDED;
    case QMI_OMA_SESSION_FAILED_REASON_SESSION_CANCELLED:
        return MM_OMA_SESSION_STATE_FAILED_REASON_SESSION_CANCELLED;
    default:
        return MM_OMA_SESSION_STATE_FAILED_REASON_UNKNOWN;
    }
}

/*****************************************************************************/
/* Convert between firmware unique ID (string) and QMI unique ID (16 bytes)
 *
 * The unique ID coming in the QMI message is a fixed-size 16 byte array, and its
 * format really depends on the manufacturer. But, if the manufacturer is nice enough
 * to use ASCII for this field, just use it ourselves as well, no need to obfuscate
 * the information we expose in our interfaces.
 *
 * We also need to do the conversion in the other way around, because when
 * selecting a new image to run we need to provide the QMI unique ID.
 */

#define EXPECTED_QMI_UNIQUE_ID_LENGTH 16

gchar *
mm_qmi_unique_id_to_firmware_unique_id (GArray  *qmi_unique_id,
                                        GError **error)
{
    guint    i;
    gboolean expect_nul_byte = FALSE;

    if (qmi_unique_id->len != EXPECTED_QMI_UNIQUE_ID_LENGTH) {
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                     "unexpected QMI unique ID length: %u (expected: %u)",
                     qmi_unique_id->len, EXPECTED_QMI_UNIQUE_ID_LENGTH);
        return NULL;
    }

    for (i = 0; i < qmi_unique_id->len; i++) {
        guint8 val;

        val = g_array_index (qmi_unique_id, guint8, i);

        /* Check for ASCII chars */
        if (g_ascii_isprint ((gchar) val)) {
            /* Halt iteration if we found an ASCII char after a NUL byte */
            if (expect_nul_byte)
                break;

            /* good char */
            continue;
        }

        /* Allow NUL bytes at the end of the array */
        if (val == '\0' && i > 0) {
            if (!expect_nul_byte)
                expect_nul_byte = TRUE;
            continue;
        }

        /* Halt iteration, not something we can build as ASCII */
        break;
    }

    if (i != qmi_unique_id->len)
        return mm_utils_bin2hexstr ((const guint8 *)qmi_unique_id->data, qmi_unique_id->len);

    return g_strndup ((const gchar *)qmi_unique_id->data, qmi_unique_id->len);
}

GArray *
mm_firmware_unique_id_to_qmi_unique_id (const gchar  *unique_id,
                                        GError      **error)
{
    guint   len;
    GArray *qmi_unique_id;

    len = strlen (unique_id);

    /* The length will be exactly EXPECTED_QMI_UNIQUE_ID_LENGTH*2 if given in HEX */
    if (len == (2 * EXPECTED_QMI_UNIQUE_ID_LENGTH)) {
        g_autofree guint8 *tmp = NULL;
        gsize              tmp_len;

        tmp_len = 0;
        tmp = mm_utils_hexstr2bin (unique_id, -1, &tmp_len, error);
        if (!tmp) {
            g_prefix_error (error, "Unexpected character found in unique id: ");
            return NULL;
        }
        g_assert (tmp_len == EXPECTED_QMI_UNIQUE_ID_LENGTH);

        qmi_unique_id = g_array_sized_new (FALSE, FALSE, sizeof (guint8), tmp_len);
        g_array_insert_vals (qmi_unique_id, 0, tmp, tmp_len);
        return qmi_unique_id;
    }

    /* The length will be EXPECTED_QMI_UNIQUE_ID_LENGTH or less if given in ASCII */
    if (len > 0 && len <= EXPECTED_QMI_UNIQUE_ID_LENGTH) {
        guint i;

        for (i = 0; i < len; i++) {
            if (!g_ascii_isprint (unique_id[i])) {
                g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                             "Unexpected character found in unique id (not ASCII): %c", unique_id[i]);
                return NULL;
            }
        }

        qmi_unique_id = g_array_sized_new (FALSE, FALSE, sizeof (guint8), EXPECTED_QMI_UNIQUE_ID_LENGTH);
        g_array_set_size (qmi_unique_id, EXPECTED_QMI_UNIQUE_ID_LENGTH);
        memcpy (&qmi_unique_id->data[0], unique_id, len);
        if (len < EXPECTED_QMI_UNIQUE_ID_LENGTH)
            memset (&qmi_unique_id->data[len], 0, EXPECTED_QMI_UNIQUE_ID_LENGTH - len);
        return qmi_unique_id;
    }

    g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                 "Unexpected unique id length: %u", len);
    return NULL;
}

/*****************************************************************************/

gboolean
mm_qmi_uim_get_card_status_output_parse (gpointer                           log_object,
                                         QmiMessageUimGetCardStatusOutput  *output,
                                         MMModemLock                       *o_lock,
                                         QmiUimPinState                    *o_pin1_state,
                                         guint                             *o_pin1_retries,
                                         guint                             *o_puk1_retries,
                                         QmiUimPinState                    *o_pin2_state,
                                         guint                             *o_pin2_retries,
                                         guint                             *o_puk2_retries,
                                         guint                             *o_pers_retries,
                                         GError                           **error)
{
    QmiMessageUimGetCardStatusOutputCardStatusCardsElement                      *card;
    QmiMessageUimGetCardStatusOutputCardStatusCardsElementApplicationsElementV2 *app;
    GArray      *cards;
    guint16      index_gw_primary = 0xFFFF;
    guint8       gw_primary_slot_i = 0;
    guint8       gw_primary_application_i = 0;
    MMModemLock  lock = MM_MODEM_LOCK_UNKNOWN;

    /* This command supports MULTIPLE cards with MULTIPLE applications each. For our
     * purposes, we're going to consider as the SIM to use the one identified as
     * 'primary GW' exclusively. We don't really support Dual Sim Dual Standby yet. */

    qmi_message_uim_get_card_status_output_get_card_status (
        output,
        &index_gw_primary,
        NULL, /* index_1x_primary */
        NULL, /* index_gw_secondary */
        NULL, /* index_1x_secondary */
        &cards,
        NULL);

    if (cards->len == 0) {
        g_set_error (error, QMI_CORE_ERROR, QMI_CORE_ERROR_FAILED,
                     "No cards reported");
        return FALSE;
    }

    /* Look for the primary GW slot and application.
     * If we don't have valid GW primary slot index and application index, assume
     * we're missing the SIM altogether */
    gw_primary_slot_i        = ((index_gw_primary & 0xFF00) >> 8);
    gw_primary_application_i = ((index_gw_primary & 0x00FF));

    if (gw_primary_slot_i == 0xFF) {
        g_set_error (error,
                     MM_MOBILE_EQUIPMENT_ERROR,
                     MM_MOBILE_EQUIPMENT_ERROR_SIM_NOT_INSERTED,
                     "GW primary session index unknown");
        return FALSE;
    }
    mm_obj_dbg (log_object, "GW primary session index: %u",     gw_primary_slot_i);

    if (gw_primary_application_i == 0xFF) {
        g_set_error (error,
                     MM_MOBILE_EQUIPMENT_ERROR,
                     MM_MOBILE_EQUIPMENT_ERROR_SIM_NOT_INSERTED,
                     "GW primary application index unknown");
        return FALSE;
    }
    mm_obj_dbg (log_object, "GW primary application index: %u", gw_primary_application_i);

    /* Validate slot index */
    if (gw_primary_slot_i >= cards->len) {
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                     "Invalid GW primary session index: %u",
                     gw_primary_slot_i);
        return FALSE;
    }

    /* Get card at slot */
    card = &g_array_index (cards, QmiMessageUimGetCardStatusOutputCardStatusCardsElement, gw_primary_slot_i);

    if (card->card_state == QMI_UIM_CARD_STATE_ABSENT) {
        g_set_error (error,
                     MM_MOBILE_EQUIPMENT_ERROR,
                     MM_MOBILE_EQUIPMENT_ERROR_SIM_NOT_INSERTED,
                     "No card found");
        return FALSE;
    }

    if (card->card_state == QMI_UIM_CARD_STATE_ERROR) {
        const gchar *card_error;

        card_error = qmi_uim_card_error_get_string (card->error_code);
        g_set_error (error,
                     MM_MOBILE_EQUIPMENT_ERROR,
                     MM_MOBILE_EQUIPMENT_ERROR_SIM_WRONG,
                     "Card error: %s", card_error ? card_error : "unknown error");
        return FALSE;
    }

    if (card->card_state != QMI_UIM_CARD_STATE_PRESENT) {
        g_set_error (error,
                     MM_MOBILE_EQUIPMENT_ERROR,
                     MM_MOBILE_EQUIPMENT_ERROR_SIM_WRONG,
                     "Card error: unexpected card state: 0x%x", card->card_state);
        return FALSE;
    }

    /* Card is present */

    if (card->applications->len == 0) {
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                     "No applications reported in card");
        return FALSE;
    }

    /* Validate application index */
    if (gw_primary_application_i >= card->applications->len) {
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                     "Invalid GW primary application index: %u",
                     gw_primary_application_i);
        return FALSE;
    }

    app = &g_array_index (card->applications, QmiMessageUimGetCardStatusOutputCardStatusCardsElementApplicationsElementV2, gw_primary_application_i);
    if ((app->type != QMI_UIM_CARD_APPLICATION_TYPE_SIM) && (app->type != QMI_UIM_CARD_APPLICATION_TYPE_USIM)) {
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                     "Unsupported application type found in GW primary application index: %s",
                     qmi_uim_card_application_type_get_string (app->type));
        return FALSE;
    }

    /* Illegal application state is fatal, consider it as a failed SIM right
     * away and don't even attempt to retry */
    if (app->state == QMI_UIM_CARD_APPLICATION_STATE_ILLEGAL) {
        g_set_error (error,
                     MM_MOBILE_EQUIPMENT_ERROR,
                     MM_MOBILE_EQUIPMENT_ERROR_SIM_WRONG,
                     "Illegal SIM/USIM application state");
        return FALSE;
    }

    /* If card not ready yet, return RETRY error.
     * If the application state reports needing PIN/PUk, consider that ready as
     * well, and let the logic fall down to check PIN1/PIN2. */
    if (app->state != QMI_UIM_CARD_APPLICATION_STATE_READY &&
        app->state != QMI_UIM_CARD_APPLICATION_STATE_PIN1_OR_UPIN_PIN_REQUIRED &&
        app->state != QMI_UIM_CARD_APPLICATION_STATE_PUK1_OR_UPIN_PUK_REQUIRED &&
        app->state != QMI_UIM_CARD_APPLICATION_STATE_CHECK_PERSONALIZATION_STATE &&
        app->state != QMI_UIM_CARD_APPLICATION_STATE_PIN1_BLOCKED) {
        mm_obj_dbg (log_object, "neither SIM nor USIM are ready");
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_RETRY,
                     "SIM not ready yet (retry)");
        return FALSE;
    }

    /* Report state and retries if requested to do so */
    if (o_pin1_state)
        *o_pin1_state = app->pin1_state;
    if (o_pin1_retries)
        *o_pin1_retries = app->pin1_retries;
    if (o_puk1_retries)
        *o_puk1_retries = app->puk1_retries;
    if (o_pin2_state)
        *o_pin2_state = app->pin2_state;
    if (o_pin2_retries)
        *o_pin2_retries = app->pin2_retries;
    if (o_puk2_retries)
        *o_puk2_retries = app->puk2_retries;

    /* Early bail out if lock status isn't wanted at this point, so that we
     * don't fail with an error the unlock retries check */
    if (!o_lock)
        return TRUE;

    /* Card is ready, what's the lock status? */

    /* PIN1 */
    switch (app->pin1_state) {
    case QMI_UIM_PIN_STATE_NOT_INITIALIZED:
        g_set_error (error,
                     MM_MOBILE_EQUIPMENT_ERROR,
                     MM_MOBILE_EQUIPMENT_ERROR_SIM_WRONG,
                     "SIM PIN/PUK status not known yet");
        return FALSE;

    case QMI_UIM_PIN_STATE_PERMANENTLY_BLOCKED:
        g_set_error (error,
                     MM_MOBILE_EQUIPMENT_ERROR,
                     MM_MOBILE_EQUIPMENT_ERROR_SIM_WRONG,
                     "SIM PIN/PUK permanently blocked");
        return FALSE;

    case QMI_UIM_PIN_STATE_ENABLED_NOT_VERIFIED:
        lock = MM_MODEM_LOCK_SIM_PIN;
        break;

    case QMI_UIM_PIN_STATE_BLOCKED:
        lock = MM_MODEM_LOCK_SIM_PUK;
        break;

    case QMI_UIM_PIN_STATE_DISABLED:
    case QMI_UIM_PIN_STATE_ENABLED_VERIFIED:
        lock = MM_MODEM_LOCK_NONE;
        break;

    default:
        g_set_error (error,
                     MM_MOBILE_EQUIPMENT_ERROR,
                     MM_MOBILE_EQUIPMENT_ERROR_SIM_WRONG,
                     "Unknown SIM PIN/PUK status");
        return FALSE;
    }

    /* Personalization */
    if (lock == MM_MODEM_LOCK_NONE &&
        app->state == QMI_UIM_CARD_APPLICATION_STATE_CHECK_PERSONALIZATION_STATE) {
        if (app->personalization_state == QMI_UIM_CARD_APPLICATION_PERSONALIZATION_STATE_IN_PROGRESS ||
            app->personalization_state == QMI_UIM_CARD_APPLICATION_PERSONALIZATION_STATE_UNKNOWN) {
            g_set_error (error,
                         MM_CORE_ERROR,
                         MM_CORE_ERROR_RETRY,
                         "Personalization check in progress");
            return FALSE;
        }
        if (app->personalization_state == QMI_UIM_CARD_APPLICATION_PERSONALIZATION_STATE_CODE_REQUIRED ||
            app->personalization_state == QMI_UIM_CARD_APPLICATION_PERSONALIZATION_STATE_PUK_CODE_REQUIRED) {
            gboolean pin;

            pin = app->personalization_state == QMI_UIM_CARD_APPLICATION_PERSONALIZATION_STATE_CODE_REQUIRED;

            switch (app->personalization_feature) {
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_GW_NETWORK:
                lock = (pin ? MM_MODEM_LOCK_PH_NET_PIN : MM_MODEM_LOCK_PH_NET_PUK);
                break;
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_GW_NETWORK_SUBSET:
                lock = (pin ? MM_MODEM_LOCK_PH_NETSUB_PIN : MM_MODEM_LOCK_PH_NETSUB_PUK);
                break;
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_GW_SERVICE_PROVIDER:
                lock = (pin ? MM_MODEM_LOCK_PH_SP_PIN : MM_MODEM_LOCK_PH_SP_PUK);
                break;
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_GW_CORPORATE:
                lock = (pin ? MM_MODEM_LOCK_PH_CORP_PIN : MM_MODEM_LOCK_PH_CORP_PUK);
                break;
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_GW_UIM:
                if (pin) {
                    lock = MM_MODEM_LOCK_PH_SIM_PIN;
                    break;
                }
                /* fall through */
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_1X_NETWORK_TYPE_1:
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_1X_NETWORK_TYPE_2:
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_1X_HRPD:
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_1X_SERVICE_PROVIDER:
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_1X_CORPORATE:
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_1X_RUIM:
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_UNKNOWN:
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_GW_SERVICE_PROVIDER_NAME:
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_GW_SP_EHPLMN:
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_GW_ICCID:
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_GW_IMPI:
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_GW_NETWORK_SUBSET_SERVICE_PROVIDER:
            case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_STATUS_GW_CARRIER:
            default:
                g_set_error (error,
                             MM_MOBILE_EQUIPMENT_ERROR,
                             MM_MOBILE_EQUIPMENT_ERROR_SIM_WRONG,
                             "Unsupported personalization feature");
                return FALSE;
            }

            if (o_pers_retries)
                *o_pers_retries = app->personalization_retries;
        }
    }

    /* PIN2 */
    if (lock == MM_MODEM_LOCK_NONE) {
        switch (app->pin2_state) {
        case QMI_UIM_PIN_STATE_NOT_INITIALIZED:
            mm_obj_warn (log_object, "SIM PIN2/PUK2 status not known yet");
            break;

        case QMI_UIM_PIN_STATE_ENABLED_NOT_VERIFIED:
            lock = MM_MODEM_LOCK_SIM_PIN2;
            break;

        case QMI_UIM_PIN_STATE_PERMANENTLY_BLOCKED:
            mm_obj_warn (log_object, "PUK2 permanently blocked");
            /* Fall through */
        case QMI_UIM_PIN_STATE_BLOCKED:
            lock = MM_MODEM_LOCK_SIM_PUK2;
            break;

        case QMI_UIM_PIN_STATE_DISABLED:
        case QMI_UIM_PIN_STATE_ENABLED_VERIFIED:
            break;

        default:
            mm_obj_warn (log_object, "unknown SIM PIN2/PUK2 status");
            break;
        }
    }

    *o_lock = lock;
    return TRUE;
}

/*****************************************************************************/

gboolean
mm_qmi_uim_get_configuration_output_parse (gpointer                              log_object,
                                           QmiMessageUimGetConfigurationOutput  *output,
                                           MMModem3gppFacility                  *o_lock,
                                           GError                              **error)
{
    QmiMessageUimGetConfigurationOutputPersonalizationStatusElement *element;
    GArray *elements;
    guint idx;

    *o_lock = MM_MODEM_3GPP_FACILITY_NONE;

    if (!qmi_message_uim_get_configuration_output_get_personalization_status (output, &elements, error)) {
        g_prefix_error (error, "UIM Get Personalization Status failed: ");
        return FALSE;
    }

    for (idx = 0; idx < elements->len; idx++) {
        element = &g_array_index (elements,
                                  QmiMessageUimGetConfigurationOutputPersonalizationStatusElement,
                                  idx);
        switch (element->feature) {
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_NETWORK:
            *o_lock |= MM_MODEM_3GPP_FACILITY_NET_PERS;
            break;
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_NETWORK_SUBSET:
            *o_lock |= MM_MODEM_3GPP_FACILITY_NET_SUB_PERS;
            break;
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_SERVICE_PROVIDER:
            *o_lock |= MM_MODEM_3GPP_FACILITY_PROVIDER_PERS;
            break;
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_CORPORATE:
            *o_lock |= MM_MODEM_3GPP_FACILITY_CORP_PERS;
            break;
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_UIM:
            *o_lock |= MM_MODEM_3GPP_FACILITY_PH_SIM;
            break;
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_1X_NETWORK_TYPE_1:
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_1X_NETWORK_TYPE_2:
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_1X_HRPD:
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_1X_SERVICE_PROVIDER:
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_1X_CORPORATE:
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_1X_RUIM:
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_SERVICE_PROVIDER_NAME:
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_SP_EHPLMN:
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_ICCID:
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_IMPI:
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_NETWORK_SUBSET_SERVICE_PROVIDER:
        case QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_CARRIER:
            mm_obj_dbg (log_object, "ignoring lock in UIM feature: %s",
                        qmi_uim_card_application_personalization_feature_get_string (element->feature));
            break;
        default:
            mm_obj_dbg (log_object, "ignoring lock in unhandled UIM feature: 0x%x",
                        (guint)element->feature);
        }
    }
    return TRUE;
}

/*****************************************************************************/

gboolean
qmi_personalization_feature_from_mm_modem_3gpp_facility (MMModem3gppFacility                          facility,
                                                         QmiUimCardApplicationPersonalizationFeature *o_feature)
{
    switch (facility) {
    case MM_MODEM_3GPP_FACILITY_NET_PERS:
        *o_feature = QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_NETWORK;
        return TRUE;
    case MM_MODEM_3GPP_FACILITY_NET_SUB_PERS:
        *o_feature = QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_NETWORK_SUBSET;
        return TRUE;
    case MM_MODEM_3GPP_FACILITY_PROVIDER_PERS:
        *o_feature = QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_SERVICE_PROVIDER;
        return TRUE;
    case MM_MODEM_3GPP_FACILITY_CORP_PERS:
        *o_feature = QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_CORPORATE;
        return TRUE;
    case MM_MODEM_3GPP_FACILITY_PH_SIM:
        *o_feature = QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_UIM;
        return TRUE;
    case MM_MODEM_3GPP_FACILITY_NONE:
    case MM_MODEM_3GPP_FACILITY_SIM:
    case MM_MODEM_3GPP_FACILITY_FIXED_DIALING:
    case MM_MODEM_3GPP_FACILITY_PH_FSIM:
    default:
        return FALSE;
    }
}

/*****************************************************************************/

typedef struct {
    QmiWdsVerboseCallEndReasonInternal vcer;
    gboolean                           is_core_error; /* TRUE if MM_CORE_ERROR, FALSE if MM_MOBILE_EQUIPMENT_ERROR */
    guint                              error_code;
} InternalErrorMap;

static const InternalErrorMap internal_error_map[] = {
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_ERROR */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_CALL_ENDED */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_UNKNOWN_INTERNAL_CAUSE */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_UNKNOWN_CAUSE */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_CLOSE_IN_PROGRESS */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_NETWORK_INITIATED_TERMINATION */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_APP_PREEMPTED */
    { QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_PDN_IPV4_CALL_DISALLOWED, FALSE, MM_MOBILE_EQUIPMENT_ERROR_IPV6_ONLY_ALLOWED },
    { QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_PDN_IPV4_CALL_THROTTLED, TRUE, MM_CORE_ERROR_THROTTLED },
    { QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_PDN_IPV6_CALL_DISALLOWED, FALSE, MM_MOBILE_EQUIPMENT_ERROR_IPV4_ONLY_ALLOWED },
    { QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_PDN_IPV6_CALL_THROTTLED, TRUE, MM_CORE_ERROR_THROTTLED },
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_MODEM_RESTART */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_PDP_PPP_NOT_SUPPORTED */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_UNPREFERRED_RAT */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_PHYSICAL_LINK_CLOSE_IN_PROGRESS */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_APN_PENDING_HANDOVER */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_PROFILE_BEARER_INCOMPATIBLE */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_MMGDSI_CARD_EVENT */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_LPM_OR_POWER_DOWN */
    { QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_APN_DISABLED, FALSE, MM_MOBILE_EQUIPMENT_ERROR_MISSING_OR_UNKNOWN_APN },
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_MPIT_EXPIRED */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_IPV6_ADDRESS_TRANSFER_FAILED */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_TRAT_SWAP_FAILED */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_EHRPD_TO_HRPD_FALLBACK */
    { QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_MANDATORY_APN_DISABLED, FALSE, MM_MOBILE_EQUIPMENT_ERROR_MISSING_OR_UNKNOWN_APN },
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_MIP_CONFIG_FAILURE */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_PDN_INACTIVITY_TIMER_EXPIRED */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_MAX_V4_CONNECTIONS */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_MAX_V6_CONNECTIONS */
    { QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_APN_MISMATCH, FALSE, MM_MOBILE_EQUIPMENT_ERROR_MISSING_OR_UNKNOWN_APN },
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_IP_VERSION_MISMATCH: NOTE: this one is treated in a special way */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_DUN_CALL_DISALLOWED */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_INVALID_PROFILE */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_EPC_NONEPC_TRANSITION */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_INVALID_PROFILE_ID */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_CALL_ALREADY_PRESENT */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_INTERFACE_IN_USE */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_IP_PDP_MISMATCH */
    { QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_APN_DISALLOWED_ON_ROAMING, FALSE, MM_MOBILE_EQUIPMENT_ERROR_MISSING_OR_UNKNOWN_APN },
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_APN_PARAMETER_CHANGE */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_INTERFACE_IN_USE_CONFIG_MATCH */
    { QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_NULL_APN_DISALLOWED, FALSE, MM_MOBILE_EQUIPMENT_ERROR_MISSING_OR_UNKNOWN_APN },
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_THERMAL_MITIGATION */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_SUBS_ID_MISMATCH */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_DATA_SETTINGS_DISABLED */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_DATA_ROAMING_SETTINGS_DISABLED */
    { QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_APN_FORMAT_INVALID, FALSE, MM_MOBILE_EQUIPMENT_ERROR_MISSING_OR_UNKNOWN_APN },
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_DDS_CALL_ABORT */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_VALIDATION_FAILURE */
};

static GError *
error_from_wds_verbose_call_end_reason_internal (QmiWdsVerboseCallEndReasonInternal  vcer_reason,
                                                 const gchar                        *vcer_reason_str,
                                                 MMBearerIpFamily                    ip_type,
                                                 gpointer                            log_object)
{
    guint i;

    /* Try to normalize the "IP version mismatch" error based on the IP type being connected. But
     * leave the original reason string to clearly show that we did this translation. */
    if (vcer_reason == QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_IP_VERSION_MISMATCH) {
        if (ip_type == MM_BEARER_IP_FAMILY_IPV4)
            vcer_reason = QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_PDN_IPV4_CALL_DISALLOWED;
        else if (ip_type == MM_BEARER_IP_FAMILY_IPV6)
            vcer_reason = QMI_WDS_VERBOSE_CALL_END_REASON_INTERNAL_PDN_IPV6_CALL_DISALLOWED;
    }

    for (i = 0; i < G_N_ELEMENTS (internal_error_map); i++) {
        if (internal_error_map[i].vcer == vcer_reason) {
            GError *error;

            g_assert (vcer_reason_str);
            if (internal_error_map[i].is_core_error)
                error = g_error_new_literal (MM_CORE_ERROR, internal_error_map[i].error_code, vcer_reason_str);
            else {
                error = mm_mobile_equipment_error_for_code (internal_error_map[i].error_code, log_object);
                g_prefix_error (&error, "%s: ", vcer_reason_str);
            }
            return error;
        }
    }

    return NULL;
}

/*****************************************************************************/

static const MMMobileEquipmentError qmi_vcer_3gpp_errors[] = {
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_OPERATOR_DETERMINED_BARRING] = MM_MOBILE_EQUIPMENT_ERROR_OPERATOR_DETERMINED_BARRING,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_INSUFFICIENT_RESOURCES] = MM_MOBILE_EQUIPMENT_ERROR_INSUFFICIENT_RESOURCES,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_UNKNOWN_APN] = MM_MOBILE_EQUIPMENT_ERROR_MISSING_OR_UNKNOWN_APN,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_UNKNOWN_PDP] = MM_MOBILE_EQUIPMENT_ERROR_UNKNOWN_PDP_ADDRESS_OR_TYPE,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_AUTHENTICATION_FAILED] = MM_MOBILE_EQUIPMENT_ERROR_USER_AUTHENTICATION_FAILED,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_GGSN_REJECT] = MM_MOBILE_EQUIPMENT_ERROR_ACTIVATION_REJECTED_BY_GGSN_OR_GW,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_ACTIVATION_REJECT] = MM_MOBILE_EQUIPMENT_ERROR_ACTIVATION_REJECTED_UNSPECIFIED,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_OPTION_NOT_SUPPORTED] = MM_MOBILE_EQUIPMENT_ERROR_SERVICE_OPTION_NOT_SUPPORTED,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_OPTION_UNSUBSCRIBED] = MM_MOBILE_EQUIPMENT_ERROR_SERVICE_OPTION_NOT_SUBSCRIBED,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_OPTION_TEMPORARILY_OUT_OF_ORDER] = MM_MOBILE_EQUIPMENT_ERROR_SERVICE_OPTION_OUT_OF_ORDER,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_NSAPI_ALREADY_USED] = MM_MOBILE_EQUIPMENT_ERROR_NSAPI_OR_PTI_ALREADY_IN_USE,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_REGULAR_DEACTIVATION] =  MM_MOBILE_EQUIPMENT_ERROR_REGULAR_DEACTIVATION,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_QOS_NOT_ACCEPTED] = MM_MOBILE_EQUIPMENT_ERROR_QOS_NOT_ACCEPTED,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_NETWORK_FAILURE] = MM_MOBILE_EQUIPMENT_ERROR_NETWORK_FAILURE_ATTACH,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_REATTACH_REQUIRED] = MM_MOBILE_EQUIPMENT_ERROR_NETWORK_FAILURE_ATTACH,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_FEATURE_NOT_SUPPORTED] = MM_MOBILE_EQUIPMENT_ERROR_FEATURE_NOT_SUPPORTED,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_TFT_SEMANTIC_ERROR] = MM_MOBILE_EQUIPMENT_ERROR_SEMANTIC_ERROR_IN_TFT_OPERATION,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_TFT_SYNTAX_ERROR] = MM_MOBILE_EQUIPMENT_ERROR_SYNTACTICAL_ERROR_IN_TFT_OPERATION,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_UNKNOWN_PDP_CONTEXT] = MM_MOBILE_EQUIPMENT_ERROR_UNKNOWN_PDP_CONTEXT,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_FILTER_SEMANTIC_ERROR] = MM_MOBILE_EQUIPMENT_ERROR_SEMANTIC_ERRORS_IN_PACKET_FILTER,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_FILTER_SYNTAX_ERROR] = MM_MOBILE_EQUIPMENT_ERROR_SYNTACTICAL_ERROR_IN_PACKET_FILTER,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_PDP_WITHOUT_ACTIVE_TFT] = MM_MOBILE_EQUIPMENT_ERROR_PDP_CONTEXT_WITHOUT_TFT_ALREADY_ACTIVATED,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_IPV4_ONLY_ALLOWED] = MM_MOBILE_EQUIPMENT_ERROR_IPV4_ONLY_ALLOWED,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_IPV6_ONLY_ALLOWED] = MM_MOBILE_EQUIPMENT_ERROR_IPV6_ONLY_ALLOWED,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_SINGLE_ADDRESS_BEARER_ONLY] = MM_MOBILE_EQUIPMENT_ERROR_SINGLE_ADDRESS_BEARERS_ONLY_ALLOWED,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_ESM_INFO_NOT_RECEIVED] = MM_MOBILE_EQUIPMENT_ERROR_ESM_INFORMATION_NOT_RECEIVED,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_PDN_CONNECTION_DOES_NOT_EXIST] = MM_MOBILE_EQUIPMENT_ERROR_PDN_CONNECTION_NONEXISTENT,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_MULTIPLE_CONNECTION_TO_SAME_PDN_NOT_ALLOWED] = MM_MOBILE_EQUIPMENT_ERROR_MULTIPLE_PDN_CONNECTION_SAME_APN_NOT_ALLOWED,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_INVALID_TRANSACTION_ID] = MM_MOBILE_EQUIPMENT_ERROR_INVALID_TRANSACTION_ID_VALUE,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_MESSAGE_INCORRECT_SEMANTIC] = MM_MOBILE_EQUIPMENT_ERROR_SEMANTICALLY_INCORRECT_MESSAGE,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_INVALID_MANDATORY_INFO] = MM_MOBILE_EQUIPMENT_ERROR_INVALID_MANDATORY_INFORMATION,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_MESSAGE_TYPE_UNSUPPORTED] = MM_MOBILE_EQUIPMENT_ERROR_MESSAGE_TYPE_NOT_IMPLEMENTED,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_MESSAGE_TYPE_NONCOMPATIBLE_STATE] = MM_MOBILE_EQUIPMENT_ERROR_MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_UNKNOWN_INFO_ELEMENT] = MM_MOBILE_EQUIPMENT_ERROR_IE_NOT_IMPLEMENTED,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_CONDITIONAL_IE_ERROR] = MM_MOBILE_EQUIPMENT_ERROR_CONDITIONAL_IE_ERROR,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_MESSAGE_AND_PROTOCOL_STATE_UNCOMPATIBLE] = MM_MOBILE_EQUIPMENT_ERROR_MESSAGE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE,
    [QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_PROTOCOL_ERROR] = MM_MOBILE_EQUIPMENT_ERROR_UNSPECIFIED_PROTOCOL_ERROR,
    /* unmapped errors */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_LLC_SNDCP_FAILURE */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_APN_TYPE_CONFLICT */
    /* QMI_WDS_VERBOSE_CALL_END_REASON_3GPP_INVALID_PROXY_CALL_SESSION_CONTROL_FUNCTION_ADDRESS */
};

static GError *
error_from_wds_verbose_call_end_reason_3gpp (QmiWdsVerboseCallEndReason3gpp  vcer_reason,
                                             const gchar                    *vcer_reason_str,
                                             gpointer                        log_object)
{
    MMMobileEquipmentError  error_code;
    GError                 *error;

    error_code = (vcer_reason < G_N_ELEMENTS (qmi_vcer_3gpp_errors)) ? qmi_vcer_3gpp_errors[vcer_reason] : 0;
    if (!error_code)
        return NULL;

    g_assert (vcer_reason_str);
    error = mm_mobile_equipment_error_for_code (error_code, log_object);
    g_prefix_error (&error, "%s: ", vcer_reason_str);
    return error;
}

GError *
mm_error_from_wds_verbose_call_end_reason (QmiWdsVerboseCallEndReasonType vcer_type,
                                           guint                          vcer_reason,
                                           MMBearerIpFamily               ip_type,
                                           gpointer                       log_object)
{
    GError      *error = NULL;
    const gchar *vcer_type_str;
    const gchar *vcer_reason_str;

    vcer_type_str = qmi_wds_verbose_call_end_reason_type_get_string (vcer_type);
    vcer_reason_str = qmi_wds_verbose_call_end_reason_get_string (vcer_type, vcer_reason);
    mm_obj_msg (log_object, "verbose call end reason (%u,%d): [%s] %s",
                vcer_type, vcer_reason, vcer_type_str, vcer_reason_str);

    switch (vcer_type) {
        case QMI_WDS_VERBOSE_CALL_END_REASON_TYPE_INTERNAL:
            error = error_from_wds_verbose_call_end_reason_internal (vcer_reason, vcer_reason_str, ip_type, log_object);
            break;
        case QMI_WDS_VERBOSE_CALL_END_REASON_TYPE_3GPP:
            error = error_from_wds_verbose_call_end_reason_3gpp (vcer_reason, vcer_reason_str, log_object);
            break;
        case QMI_WDS_VERBOSE_CALL_END_REASON_TYPE_MIP:
        case QMI_WDS_VERBOSE_CALL_END_REASON_TYPE_CM:
        case QMI_WDS_VERBOSE_CALL_END_REASON_TYPE_PPP:
        case QMI_WDS_VERBOSE_CALL_END_REASON_TYPE_EHRPD:
        case QMI_WDS_VERBOSE_CALL_END_REASON_TYPE_IPV6:
        default:
            break;
    }

    if (error)
        return error;

    return g_error_new (MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_UNKNOWN,
                        "Call failed: %s error: %s",
                        vcer_type_str ? vcer_type_str : "unknown",
                        vcer_reason_str ? vcer_reason_str : "unknown");
}

/*****************************************************************************/

gboolean
mm_error_from_qmi_loc_indication_status (QmiLocIndicationStatus   status,
                                         GError                 **error)
{
    switch (status) {
    case QMI_LOC_INDICATION_STATUS_SUCCESS:
        return TRUE;
    case QMI_LOC_INDICATION_STATUS_GENERAL_FAILURE:
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "LOC service: general failure");
        return FALSE;
    case QMI_LOC_INDICATION_STATUS_UNSUPPORTED:
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_UNSUPPORTED, "LOC service: unsupported");
        return FALSE;
    case QMI_LOC_INDICATION_STATUS_INVALID_PARAMETER:
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_INVALID_ARGS, "LOC service: invalid parameter");
        return FALSE;
    case QMI_LOC_INDICATION_STATUS_ENGINE_BUSY:
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_IN_PROGRESS, "LOC service: engine busy");
        return FALSE;
    case QMI_LOC_INDICATION_STATUS_PHONE_OFFLINE:
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_WRONG_STATE, "LOC service: phone offline");
        return FALSE;
    case QMI_LOC_INDICATION_STATUS_TIMEOUT:
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_ABORTED, "LOC service: timeout");
        return FALSE;
    default:
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "LOC service: unknown failure");
        return FALSE;
    }
}

/*****************************************************************************/

void
mm_register_qmi_errors (void)
{
    static gsize qmi_errors_registered = 0;

    if (!g_once_init_enter (&qmi_errors_registered))
        return;

    /* QMI core errors */
    mm_register_error_mapping (QMI_CORE_ERROR, QMI_CORE_ERROR_FAILED,             MM_CORE_ERROR, MM_CORE_ERROR_FAILED);
    mm_register_error_mapping (QMI_CORE_ERROR, QMI_CORE_ERROR_WRONG_STATE,        MM_CORE_ERROR, MM_CORE_ERROR_WRONG_STATE);
    mm_register_error_mapping (QMI_CORE_ERROR, QMI_CORE_ERROR_TIMEOUT,            MM_CORE_ERROR, MM_CORE_ERROR_TIMEOUT);
    mm_register_error_mapping (QMI_CORE_ERROR, QMI_CORE_ERROR_INVALID_ARGS,       MM_CORE_ERROR, MM_CORE_ERROR_INVALID_ARGS);
    mm_register_error_mapping (QMI_CORE_ERROR, QMI_CORE_ERROR_INVALID_MESSAGE,    MM_CORE_ERROR, MM_CORE_ERROR_INVALID_ARGS);
    mm_register_error_mapping (QMI_CORE_ERROR, QMI_CORE_ERROR_TLV_NOT_FOUND,      MM_CORE_ERROR, MM_CORE_ERROR_NOT_FOUND);
    mm_register_error_mapping (QMI_CORE_ERROR, QMI_CORE_ERROR_TLV_TOO_LONG,       MM_CORE_ERROR, MM_CORE_ERROR_INVALID_ARGS);
    mm_register_error_mapping (QMI_CORE_ERROR, QMI_CORE_ERROR_UNSUPPORTED,        MM_CORE_ERROR, MM_CORE_ERROR_UNSUPPORTED);
    mm_register_error_mapping (QMI_CORE_ERROR, QMI_CORE_ERROR_TLV_EMPTY,          MM_CORE_ERROR, MM_CORE_ERROR_INVALID_ARGS);
    mm_register_error_mapping (QMI_CORE_ERROR, QMI_CORE_ERROR_UNEXPECTED_MESSAGE, MM_CORE_ERROR, MM_CORE_ERROR_WRONG_STATE);
    mm_register_error_mapping (QMI_CORE_ERROR, QMI_CORE_ERROR_INVALID_DATA,       MM_CORE_ERROR, MM_CORE_ERROR_INVALID_ARGS);

    /* QMI protocol errors */

    /* This should never happen, because QMI operations won't fail on this type of
     * error. But still, just in case, treat it as an error during normalization. */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NONE, MM_CORE_ERROR, MM_CORE_ERROR_FAILED);

    /* This is not really an error in an operation, it is the modem reporting that the
     * operation was a no-op. For the purposes of returning errors to the user, we will
     * normalize it either way. */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NO_EFFECT, MM_CORE_ERROR, MM_CORE_ERROR_FAILED);

    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_MALFORMED_MESSAGE,                MM_CORE_ERROR,             MM_CORE_ERROR_PROTOCOL);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NO_MEMORY,                        MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_MEMORY_FULL);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INTERNAL,                         MM_CORE_ERROR,             MM_CORE_ERROR_FAILED);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_ABORTED,                          MM_CORE_ERROR,             MM_CORE_ERROR_ABORTED);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_CLIENT_IDS_EXHAUSTED,             MM_CORE_ERROR,             MM_CORE_ERROR_PROTOCOL);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_UNABORTABLE_TRANSACTION,          MM_CORE_ERROR,             MM_CORE_ERROR_PROTOCOL);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INVALID_CLIENT_ID,                MM_CORE_ERROR,             MM_CORE_ERROR_PROTOCOL);
    /* QMI_PROTOCOL_ERROR_NO_THRESHOLDS_PROVIDED */
    /* QMI_PROTOCOL_ERROR_INVALID_HANDLE */
    /* QMI_PROTOCOL_ERROR_INVALID_PROFILE */
    /* QMI_PROTOCOL_ERROR_INVALID_PIN_ID */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INCORRECT_PIN,                    MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_PIN);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NO_NETWORK_FOUND,                 MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NO_NETWORK);
    /* QMI_PROTOCOL_ERROR_CALL_FAILED */
    /* QMI_PROTOCOL_ERROR_OUT_OF_CALL */
    /* QMI_PROTOCOL_ERROR_NOT_PROVISIONED */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_MISSING_ARGUMENT,                 MM_CORE_ERROR,             MM_CORE_ERROR_PROTOCOL);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_ARGUMENT_TOO_LONG,                MM_CORE_ERROR,             MM_CORE_ERROR_PROTOCOL);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INVALID_TRANSACTION_ID,           MM_CORE_ERROR,             MM_CORE_ERROR_PROTOCOL);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_DEVICE_IN_USE,                    MM_CORE_ERROR,             MM_CORE_ERROR_RETRY);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NETWORK_UNSUPPORTED,              MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_SUPPORTED);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_DEVICE_UNSUPPORTED,               MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_SUPPORTED);
    /* QMI_PROTOCOL_ERROR_NO_FREE_PROFILE */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INVALID_PDP_TYPE,                 MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_UNKNOWN_PDP_ADDRESS_OR_TYPE);
    /* QMI_PROTOCOL_ERROR_INVALID_TECHNOLOGY_PREFERENCE */
    /* QMI_PROTOCOL_ERROR_INVALID_PROFILE_TYPE */
    /* QMI_PROTOCOL_ERROR_INVALID_SERVICE_TYPE */
    /* QMI_PROTOCOL_ERROR_INVALID_REGISTER_ACTION */
    /* QMI_PROTOCOL_ERROR_INVALID_PS_ATTACH_ACTION */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_AUTHENTICATION_FAILED,            MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_USER_AUTHENTICATION_FAILED);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_PIN_BLOCKED,                      MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_PUK);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_PIN_ALWAYS_BLOCKED,               MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_FAILURE);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_UIM_UNINITIALIZED,                MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_FAILURE);
    /* QMI_PROTOCOL_ERROR_MAXIMUM_QOS_REQUESTS_IN_USE */
    /* QMI_PROTOCOL_ERROR_INCORRECT_FLOW_FILTER */
    /* QMI_PROTOCOL_ERROR_NETWORK_QOS_UNAWARE */
    /* QMI_PROTOCOL_ERROR_INVALID_QOS_ID */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_REQUESTED_NUMBER_UNSUPPORTED,     MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_SUPPORTED);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INTERFACE_NOT_FOUND,              MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_FOUND);
    /* QMI_PROTOCOL_ERROR_FLOW_SUSPENDED */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INVALID_DATA_FORMAT,              MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_PHONE_FAILURE);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_GENERAL_ERROR,                    MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_PHONE_FAILURE);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_UNKNOWN_ERROR,                    MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_UNKNOWN);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INVALID_ARGUMENT,                 MM_CORE_ERROR,             MM_CORE_ERROR_INVALID_ARGS);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INVALID_INDEX,                    MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_INVALID_INDEX);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NO_ENTRY,                         MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_FOUND);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_DEVICE_STORAGE_FULL,              MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_MEMORY_FULL);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_DEVICE_NOT_READY,                 MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_PHONE_FAILURE);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NETWORK_NOT_READY,                MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NO_NETWORK);
    /* QMI_PROTOCOL_ERROR_WMS_CAUSE_CODE */
    /* QMI_PROTOCOL_ERROR_WMS_MESSAGE_NOT_SENT */
    /* QMI_PROTOCOL_ERROR_WMS_MESSAGE_DELIVERY_FAILURE */
    /* QMI_PROTOCOL_ERROR_WMS_INVALID_MESSAGE_ID */
    /* QMI_PROTOCOL_ERROR_WMS_ENCODING */
    /* QMI_PROTOCOL_ERROR_AUTHENTICATION_LOCK */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INVALID_TRANSITION,               MM_CORE_ERROR, MM_CORE_ERROR_WRONG_STATE);
    /* QMI_PROTOCOL_ERROR_NOT_MCAST_INTERFACE */
    /* QMI_PROTOCOL_ERROR_MAXIMUM_MCAST_REQUESTS_IN_USE */
    /* QMI_PROTOCOL_ERROR_INVALID_MCAST_HANDLE */
    /* QMI_PROTOCOL_ERROR_INVALID_IP_FAMILY_PREFERENCE */
    /* QMI_PROTOCOL_ERROR_SESSION_INACTIVE */
    /* QMI_PROTOCOL_ERROR_SESSION_INVALID */
    /* QMI_PROTOCOL_ERROR_SESSION_OWNERSHIP */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INSUFFICIENT_RESOURCES,           MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_INSUFFICIENT_RESOURCES);
    /* QMI_PROTOCOL_ERROR_DISABLED */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INVALID_OPERATION,                MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_SUPPORTED);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INVALID_QMI_COMMAND,              MM_CORE_ERROR,  MM_CORE_ERROR_PROTOCOL);
    /* QMI_PROTOCOL_ERROR_WMS_T_PDU_TYPE */
    /* QMI_PROTOCOL_ERROR_WMS_SMSC_ADDRESS */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INFORMATION_UNAVAILABLE,          MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_INVALID_MANDATORY_INFORMATION);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_SEGMENT_TOO_LONG,                 MM_CORE_ERROR,  MM_CORE_ERROR_PROTOCOL);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_SEGMENT_ORDER,                    MM_CORE_ERROR,  MM_CORE_ERROR_PROTOCOL);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_BUNDLING_NOT_SUPPORTED,           MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_SUPPORTED);
    /* QMI_PROTOCOL_ERROR_OPERATION_PARTIAL_FAILURE */
    /* QMI_PROTOCOL_ERROR_POLICY_MISMATCH */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_SIM_FILE_NOT_FOUND,               MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_FOUND);
    /* QMI_PROTOCOL_ERROR_EXTENDED_INTERNAL */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_ACCESS_DENIED,                    MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_ALLOWED);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_HARDWARE_RESTRICTED,              MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_ALLOWED);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_ACK_NOT_SENT,                     MM_CORE_ERROR,  MM_CORE_ERROR_WRONG_STATE);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INJECT_TIMEOUT,                   MM_CORE_ERROR,  MM_CORE_ERROR_TIMEOUT);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INCOMPATIBLE_STATE,               MM_CORE_ERROR,  MM_CORE_ERROR_WRONG_STATE);
    /* QMI_PROTOCOL_ERROR_FDN_RESTRICT */
    /* QMI_PROTOCOL_ERROR_SUPS_FAILURE_CASE */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NO_RADIO,                         MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_ALLOWED);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NOT_SUPPORTED,                    MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_SUPPORTED);
    /* QMI_PROTOCOL_ERROR_NO_SUBSCRIPTION */
    /* QMI_PROTOCOL_ERROR_CARD_CALL_CONTROL_FAILED */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NETWORK_ABORTED,                  MM_CORE_ERROR,  MM_CORE_ERROR_ABORTED);
    /* QMI_PROTOCOL_ERROR_MSG_BLOCKED */
    /* QMI_PROTOCOL_ERROR_INVALID_SESSION_TYPE */
    /* QMI_PROTOCOL_ERROR_INVALID_PB_TYPE */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NO_SIM,                           MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_NOT_INSERTED);
    /* QMI_PROTOCOL_ERROR_PB_NOT_READY */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_PIN_RESTRICTION,                  MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_PIN);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_PIN2_RESTRICTION,                 MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_PIN2);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_PUK_RESTRICTION,                  MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_PUK);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_PUK2_RESTRICTION,                 MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_PUK2);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_PB_ACCESS_RESTRICTED,             MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_ALLOWED);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_PB_DELETE_IN_PROGRESS,            MM_CORE_ERROR,  MM_CORE_ERROR_IN_PROGRESS);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_PB_TEXT_TOO_LONG,                 MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_TEXT_TOO_LONG);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_PB_NUMBER_TOO_LONG,               MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_DIAL_STRING_TOO_LONG);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_PB_HIDDEN_KEY_RESTRICTION,        MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_ALLOWED);
    /* QMI_PROTOCOL_ERROR_PB_NOT_AVAILABLE */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_DEVICE_MEMORY_ERROR,              MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_MEMORY_FAILURE);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NO_PERMISSION,                    MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_ALLOWED);
    /* QMI_PROTOCOL_ERROR_TOO_SOON */
    /* QMI_PROTOCOL_ERROR_TIME_NOT_ACQUIRED */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_OPERATION_IN_PROGRESS,            MM_CORE_ERROR, MM_CORE_ERROR_IN_PROGRESS);
    /* QMI_PROTOCOL_ERROR_FW_WRITE_FAILED */
    /* QMI_PROTOCOL_ERROR_FW_INFO_READ_FAILED */
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_FW_FILE_NOT_FOUND,                MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_FOUND);
    mm_register_error_mapping (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_FW_DIR_NOT_FOUND,                 MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_FOUND);
    /* QMI_PROTOCOL_ERROR_FW_ALREADY_ACTIVATED */
    /* QMI_PROTOCOL_ERROR_FW_CANNOT_GENERIC_IMAGE */
    /* QMI_PROTOCOL_ERROR_FW_FILE_OPEN_FAILED */
    /* QMI_PROTOCOL_ERROR_FW_UPDATE_DISCONTINUOUS_FRAME */
    /* QMI_PROTOCOL_ERROR_FW_UPDATE_FAILED */
    /* QMI_PROTOCOL_ERROR_CAT_EVENT_REGISTRATION_FAILED */
    /* QMI_PROTOCOL_ERROR_CAT_INVALID_TERMINAL_RESPONSE */
    /* QMI_PROTOCOL_ERROR_CAT_INVALID_ENVELOPE_COMMAND */
    /* QMI_PROTOCOL_ERROR_CAT_ENVELOPE_COMMAND_BUSY */
    /* QMI_PROTOCOL_ERROR_CAT_ENVELOPE_COMMAND_FAILED */

    g_once_init_leave (&qmi_errors_registered, 1);
}
