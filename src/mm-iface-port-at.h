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
 * Copyright (C) 2024 Google, Inc.
 */

#ifndef MM_IFACE_PORT_AT_H
#define MM_IFACE_PORT_AT_H

#include <glib-object.h>
#include <gio/gio.h>

#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#define MM_TYPE_IFACE_PORT_AT               (mm_iface_port_at_get_type ())
#define MM_IFACE_PORT_AT(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_IFACE_PORT_AT, MMIfacePortAt))
#define MM_IS_IFACE_PORT_AT(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_IFACE_PORT_AT))
#define MM_IFACE_PORT_AT_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), MM_TYPE_IFACE_PORT_AT, MMIfacePortAt))

typedef struct _MMIfacePortAt MMIfacePortAt;

struct _MMIfacePortAt {
    GTypeInterface g_iface;

    gboolean (* check_support) (MMIfacePortAt        *self,
                                gboolean             *out_supported,
                                GError              **error);

    void    (* command)        (MMIfacePortAt        *self,
                                const gchar          *command,
                                guint32               timeout_seconds,
                                gboolean              is_raw,
                                gboolean              allow_cached,
                                GCancellable         *cancellable,
                                GAsyncReadyCallback   callback,
                                gpointer              user_data);
    gchar * (* command_finish) (MMIfacePortAt        *self,
                                GAsyncResult         *res,
                                GError              **error);
};

GType mm_iface_port_at_get_type (void);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (MMIfacePortAt, g_object_unref)

gboolean  mm_iface_port_at_check_support  (MMIfacePortAt        *self,
                                           gboolean             *out_supported,
                                           GError              **error);

void      mm_iface_port_at_command        (MMIfacePortAt        *self,
                                           const gchar          *command,
                                           guint32               timeout_seconds,
                                           gboolean              is_raw,
                                           gboolean              allow_cached,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data);
gchar    *mm_iface_port_at_command_finish (MMIfacePortAt        *self,
                                           GAsyncResult         *res,
                                           GError              **error);

#endif /* MM_IFACE_PORT_AT_H */
