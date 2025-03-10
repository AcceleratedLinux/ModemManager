/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Copyright (C) 2008 - 2009 Novell, Inc.
 * Copyright (C) 2009 - 2012 Red Hat, Inc.
 * Copyright (C) 2012 Aleksander Morgado <aleksander@gnu.org>
 */

#include <string.h>
#include <gmodule.h>

#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-plugin-common.h"
#include "mm-common-novatel.h"
#include "mm-private-boxed-types.h"
#include "mm-broadband-modem-novatel.h"
#include "mm-log-object.h"

#if defined WITH_QMI
#include "mm-broadband-modem-qmi.h"
#endif

#define MM_TYPE_PLUGIN_NOVATEL mm_plugin_novatel_get_type ()
MM_DEFINE_PLUGIN (NOVATEL, novatel, Novatel)

/*****************************************************************************/

static MMBaseModem *
create_modem (MMPlugin *self,
              const gchar *uid,
              const gchar *physdev,
              const gchar **drivers,
              guint16 vendor,
              guint16 product,
              guint16 subsystem_vendor,
              guint16 subsystem_device,
              GList *probes,
              GError **error)
{
#if defined WITH_QMI
    if (mm_port_probe_list_has_qmi_port (probes)) {
        mm_obj_dbg (self, "QMI-powered Novatel modem found...");
        return MM_BASE_MODEM (mm_broadband_modem_qmi_new (uid,
                                                          physdev,
                                                          drivers,
                                                          mm_plugin_get_name (self),
                                                          vendor,
                                                          product));
    }
#endif

    return MM_BASE_MODEM (mm_broadband_modem_novatel_new (uid,
                                                          physdev,
                                                          drivers,
                                                          mm_plugin_get_name (self),
                                                          vendor,
                                                          product));
}

/*****************************************************************************/

MM_PLUGIN_NAMED_CREATOR_SCOPE MMPlugin *
mm_plugin_create_novatel (void)
{
    static const gchar *subsystems[] = { "tty", "net", "usbmisc", NULL };
    static const guint16 vendors[] = { 0x1410, 0 };
    static const mm_uint16_pair forbidden_products[] = { { 0x1410, 0x9010 }, /* Novatel E362 */
                                                         { 0, 0 } };
    static const MMAsyncMethod custom_init = {
        .async  = G_CALLBACK (mm_common_novatel_custom_init),
        .finish = G_CALLBACK (mm_common_novatel_custom_init_finish),
    };

    return MM_PLUGIN (
        g_object_new (MM_TYPE_PLUGIN_NOVATEL,
                      MM_PLUGIN_NAME,                  MM_MODULE_NAME,
                      MM_PLUGIN_ALLOWED_SUBSYSTEMS,    subsystems,
                      MM_PLUGIN_ALLOWED_VENDOR_IDS,    vendors,
                      MM_PLUGIN_FORBIDDEN_PRODUCT_IDS, forbidden_products,
                      MM_PLUGIN_ALLOWED_AT,            TRUE,
                      MM_PLUGIN_CUSTOM_INIT,           &custom_init,
                      MM_PLUGIN_REQUIRED_QCDM,         TRUE,
                      MM_PLUGIN_ALLOWED_QMI,           TRUE,
                      NULL));
}

static void
mm_plugin_novatel_init (MMPluginNovatel *self)
{
}

static void
mm_plugin_novatel_class_init (MMPluginNovatelClass *klass)
{
    MMPluginClass *plugin_class = MM_PLUGIN_CLASS (klass);

    plugin_class->create_modem = create_modem;
}
