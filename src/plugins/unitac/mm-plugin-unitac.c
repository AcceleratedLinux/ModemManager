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

#include <string.h>
#include <gmodule.h>

#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-private-boxed-types.h"
#include "mm-broadband-modem-unitac.h"
#include "mm-plugin-common.h"

#define MM_TYPE_PLUGIN_UNITAC mm_plugin_unitac_get_type ()
MM_DEFINE_PLUGIN (UNITAC, unitac, Unitac)

/*****************************************************************************/

static MMBaseModem *
create_modem (MMPlugin *self,
              const gchar  *sysfs_path,
              const gchar  *physdev,
              const gchar **drivers,
              guint16       vendor,
              guint16       product,
              guint16       subsystem_vendor,
              GList        *probes,
              GError      **error)
{
    return MM_BASE_MODEM (mm_broadband_modem_unitac_new (sysfs_path,
                                                         physdev,
                                                         drivers,
                                                         mm_plugin_get_name (self),
                                                         vendor,
                                                         product));
}

MM_PLUGIN_NAMED_CREATOR_SCOPE MMPlugin *
mm_plugin_create_unitac (void)
{
    static const gchar *subsystems[] = { "tty", "net", "usb", NULL };
    static const guint16 vendor_ids[] = { 0x1076, 0 };

    return MM_PLUGIN (
        g_object_new (MM_TYPE_PLUGIN_UNITAC,
                      MM_PLUGIN_NAME,               MM_MODULE_NAME,
                      MM_PLUGIN_ALLOWED_SUBSYSTEMS, subsystems,
                      MM_PLUGIN_ALLOWED_VENDOR_IDS, vendor_ids,
                      MM_PLUGIN_ALLOWED_AT,         TRUE,
                      MM_PLUGIN_ALLOWED_QCDM,       FALSE,
                      MM_PLUGIN_ALLOWED_QMI,        FALSE,
                      MM_PLUGIN_ALLOWED_MBIM,       FALSE,
                      NULL));
}

static void
mm_plugin_unitac_init (MMPluginUnitac *self)
{
}

static void
mm_plugin_unitac_class_init (MMPluginUnitacClass *klass)
{
    MMPluginClass *plugin_class = MM_PLUGIN_CLASS (klass);

    plugin_class->create_modem = create_modem;
}