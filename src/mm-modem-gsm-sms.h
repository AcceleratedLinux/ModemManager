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
 * Copyright (C) 2009 Novell, Inc.
 */

#ifndef MM_MODEM_GSM_SMS_H
#define MM_MODEM_GSM_SMS_H

#include <mm-modem.h>

#define MM_TYPE_MODEM_GSM_SMS      (mm_modem_gsm_sms_get_type ())
#define MM_MODEM_GSM_SMS(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_MODEM_GSM_SMS, MMModemGsmSms))
#define MM_IS_MODEM_GSM_SMS(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_MODEM_GSM_SMS))
#define MM_MODEM_GSM_SMS_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), MM_TYPE_MODEM_GSM_SMS, MMModemGsmSms))

typedef struct _MMModemGsmSms MMModemGsmSms;

typedef void (*MMModemGsmSmsGetFn) (MMModemGsmSms *modem,
                                    GHashTable *properties,
                                    GError *error,
                                    gpointer user_data);

typedef void (*MMModemGsmSmsListFn) (MMModemGsmSms *modem,
                                     GPtrArray *resultlist,
                                     GError *error,
                                     gpointer user_data);

typedef void (*MMModemGsmSmsSendFn) (MMModemGsmSms *modem,
                                     GArray *indexes,
                                     GError *error,
                                     gpointer user_data);

struct _MMModemGsmSms {
    GTypeInterface g_iface;

    /* Methods */
    void (*send) (MMModemGsmSms *modem,
                  const char *number,
                  const char *text,
                  const char *smsc,
                  guint validity,
                  guint class,
                  MMModemGsmSmsSendFn callback,
                  gpointer user_data);

    void (*get) (MMModemGsmSms *modem,
                 guint32 index,
                 MMModemGsmSmsGetFn callback,
                 gpointer user_data);

    void (*delete) (MMModemGsmSms *modem,
                    guint32 index,
                    MMModemFn callback,
                    gpointer user_data);

    void (*list) (MMModemGsmSms *modem,
                  MMModemGsmSmsListFn callback,
                  gpointer user_data);

    /* Signals */
    void (*sms_received) (MMModemGsmSms *self,
                          guint32 index,
                          gboolean completed);

    void (*completed)    (MMModemGsmSms *self,
                          guint32 index,
                          gboolean completed);
};

GType mm_modem_gsm_sms_get_type (void);

void mm_modem_gsm_sms_send (MMModemGsmSms *self,
                            const char *number,
                            const char *text,
                            const char *smsc,
                            guint validity,
                            guint class,
                            MMModemGsmSmsSendFn callback,
                            gpointer user_data);

void mm_modem_gsm_sms_get (MMModemGsmSms *self,
                           guint idx,
                           MMModemGsmSmsGetFn callback,
                           gpointer user_data);

void mm_modem_gsm_sms_delete (MMModemGsmSms *self,
                              guint idx,
                              MMModemFn callback,
                              gpointer user_data);

void mm_modem_gsm_sms_list (MMModemGsmSms *self,
                            MMModemGsmSmsListFn callback,
                            gpointer user_data);

void mm_modem_gsm_sms_received (MMModemGsmSms *self,
                                guint idx,
                                gboolean complete);

void mm_modem_gsm_sms_completed (MMModemGsmSms *self,
                                guint idx,
                                gboolean complete);


#endif /* MM_MODEM_GSM_SMS_H */
