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
 */

#ifndef MM_BASE_MANAGER_H
#define MM_BASE_MANAGER_H

#include <config.h>

#include <glib-object.h>
#include <gio/gio.h>

#include "mm-filter.h"
#include "mm-gdbus-manager.h"
#include "mm-sleep-context.h"

#define MM_TYPE_BASE_MANAGER            (mm_base_manager_get_type ())
#define MM_BASE_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_BASE_MANAGER, MMBaseManager))
#define MM_BASE_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MM_TYPE_BASE_MANAGER, MMBaseManagerClass))
#define MM_IS_BASE_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_BASE_MANAGER))
#define MM_IS_BASE_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), MM_TYPE_BASE_MANAGER))
#define MM_BASE_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MM_TYPE_BASE_MANAGER, MMBaseManagerClass))

#define MM_BASE_MANAGER_CONNECTION            "connection"            /* Construct-only */
#define MM_BASE_MANAGER_AUTO_SCAN             "auto-scan"             /* Construct-only */
#define MM_BASE_MANAGER_FILTER_POLICY         "filter-policy"         /* Construct-only */
#define MM_BASE_MANAGER_PLUGIN_DIR            "plugin-dir"            /* Construct-only */
#define MM_BASE_MANAGER_INITIAL_KERNEL_EVENTS "initial-kernel-events" /* Construct-only */
#if defined WITH_TESTS
#define MM_BASE_MANAGER_ENABLE_TEST           "enable-test"           /* Construct-only */
#endif

typedef struct _MMBaseManagerPrivate MMBaseManagerPrivate;

typedef struct {
    MmGdbusOrgFreedesktopModemManager1Skeleton parent;
    MMBaseManagerPrivate *priv;
} MMBaseManager;

typedef struct {
    MmGdbusOrgFreedesktopModemManager1SkeletonClass parent;
} MMBaseManagerClass;

GType mm_base_manager_get_type (void);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (MMBaseManager, g_object_unref)

MMBaseManager   *mm_base_manager_new         (GDBusConnection  *bus,
#if !defined WITH_BUILTIN_PLUGINS
                                              const gchar      *plugin_dir,
#endif
                                              gboolean          auto_scan,
                                              MMFilterRule      filter_policy,
                                              const gchar      *initial_kernel_events,
#if defined WITH_TESTS
                                              gboolean          enable_test,
#endif
                                              GError          **error);

void             mm_base_manager_start       (MMBaseManager *manager,
                                              gboolean manual_scan);

typedef enum {
    MM_BASE_MANAGER_CLEANUP_NONE      = 0x0,
    MM_BASE_MANAGER_CLEANUP_DISABLE   = 0x1,
    MM_BASE_MANAGER_CLEANUP_LOW_POWER = 0x2,
    MM_BASE_MANAGER_CLEANUP_REMOVE    = 0x4,
    MM_BASE_MANAGER_CLEANUP_TERSE     = 0x8,
} MMBaseManagerCleanupFlags;

void             mm_base_manager_cleanup     (MMBaseManager             *manager,
                                              MMBaseManagerCleanupFlags  flags,
                                              MMSleepContext            *ctx);

#if defined WITH_SUSPEND_RESUME
void             mm_base_manager_sync        (MMBaseManager *manager);
#endif

guint32          mm_base_manager_num_modems  (MMBaseManager *manager);

#endif /* MM_BASE_MANAGER_H */
