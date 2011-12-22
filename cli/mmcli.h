/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * mmcli -- Control modem status & access information from the command line
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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
 * Copyright (C) 2011 Aleksander Morgado <aleksander@gnu.org>
 */

#include <glib.h>

#ifndef __MMCLI_H__
#define __MMCLI_H__

/* Common */
void          mmcli_async_operation_done  (void);
void          mmcli_force_async_operation (void);
void          mmcli_force_sync_operation  (void);

/* Manager group */
GOptionGroup *mmcli_manager_get_option_group (void);
gboolean      mmcli_manager_options_enabled  (void);
void          mmcli_manager_run_asynchronous (GDBusConnection *connection,
                                              GCancellable    *cancellable);
void          mmcli_manager_run_synchronous  (GDBusConnection *connection);
void          mmcli_manager_shutdown         (void);

/* Modem group */
GOptionGroup *mmcli_modem_get_option_group   (void);
gboolean      mmcli_modem_options_enabled    (void);
void          mmcli_modem_run_asynchronous   (GDBusConnection *connection,
                                              GCancellable    *cancellable);
void          mmcli_modem_run_synchronous    (GDBusConnection *connection);
void          mmcli_modem_shutdown           (void);

/* 3GPP group */
GOptionGroup *mmcli_modem_3gpp_get_option_group   (void);
gboolean      mmcli_modem_3gpp_options_enabled    (void);
void          mmcli_modem_3gpp_run_asynchronous   (GDBusConnection *connection,
                                                   GCancellable    *cancellable);
void          mmcli_modem_3gpp_run_synchronous    (GDBusConnection *connection);
void          mmcli_modem_3gpp_shutdown           (void);

/* Simple group */
GOptionGroup *mmcli_modem_simple_get_option_group   (void);
gboolean      mmcli_modem_simple_options_enabled    (void);
void          mmcli_modem_simple_run_asynchronous   (GDBusConnection *connection,
                                                     GCancellable    *cancellable);
void          mmcli_modem_simple_run_synchronous    (GDBusConnection *connection);
void          mmcli_modem_simple_shutdown           (void);

/* Bearer group */
GOptionGroup *mmcli_bearer_get_option_group   (void);
gboolean      mmcli_bearer_options_enabled    (void);
void          mmcli_bearer_run_asynchronous   (GDBusConnection *connection,
                                              GCancellable    *cancellable);
void          mmcli_bearer_run_synchronous    (GDBusConnection *connection);
void          mmcli_bearer_shutdown           (void);

/* SIM group */
GOptionGroup *mmcli_sim_get_option_group   (void);
gboolean      mmcli_sim_options_enabled    (void);
void          mmcli_sim_run_asynchronous   (GDBusConnection *connection,
                                            GCancellable    *cancellable);
void          mmcli_sim_run_synchronous    (GDBusConnection *connection);
void          mmcli_sim_shutdown           (void);

#endif /* __MMCLI_H__ */
