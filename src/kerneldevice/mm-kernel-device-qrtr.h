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
 * Copyright 2020 Google LLC
 */

#ifndef MM_KERNEL_DEVICE_QRTR_H
#define MM_KERNEL_DEVICE_QRTR_H

#include <glib.h>
#include <glib-object.h>
#include <libqrtr-glib.h>

#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-kernel-device.h"

#define MM_TYPE_KERNEL_DEVICE_QRTR            (mm_kernel_device_qrtr_get_type ())
#define MM_KERNEL_DEVICE_QRTR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_KERNEL_DEVICE_QRTR, MMKernelDeviceQrtr))
#define MM_KERNEL_DEVICE_QRTR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MM_TYPE_KERNEL_DEVICE_QRTR, MMKernelDeviceQrtrClass))
#define MM_IS_KERNEL_DEVICE_QRTR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_KERNEL_DEVICE_QRTR))
#define MM_IS_KERNEL_DEVICE_QRTR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MM_TYPE_KERNEL_DEVICE_QRTR))
#define MM_KERNEL_DEVICE_QRTR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MM_TYPE_KERNEL_DEVICE_QRTR, MMKernelDeviceQrtrClass))

typedef struct _MMKernelDeviceQrtr        MMKernelDeviceQrtr;
typedef struct _MMKernelDeviceQrtrClass   MMKernelDeviceQrtrClass;
typedef struct _MMKernelDeviceQrtrPrivate MMKernelDeviceQrtrPrivate;

struct _MMKernelDeviceQrtr {
    MMKernelDevice parent;
    MMKernelDeviceQrtrPrivate *priv;
};

struct _MMKernelDeviceQrtrClass {
    MMKernelDeviceClass parent;
};

QrtrNode       *mm_kernel_device_qrtr_get_node   (MMKernelDeviceQrtr *self);

GType           mm_kernel_device_qrtr_get_type (void);
MMKernelDevice *mm_kernel_device_qrtr_new      (QrtrNode *qrtr_node);

#endif /* MM_KERNEL_DEVICE_QRTR_H */
