/*
 * nvidia-persistenced: A daemon for maintaining persistent driver state,
 * specifically for use by the NVIDIA Linux driver.
 *
 * Copyright (C) 2013 NVIDIA Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * nvpd_defs.h
 */

#ifndef _NVPD_DEFS_H_
#define _NVPD_DEFS_H_

#include "nvpd_rpc.h"

#define NVPD_DAEMON_NAME            "nvidia-persistenced"

#define NVPD_VAR_RUNTIME_DATA_PATH  "/var/run/" NVPD_DAEMON_NAME
#define NVPD_SOCKET_NAME            "socket"
#define NVPD_SOCKET_PATH            NVPD_VAR_RUNTIME_DATA_PATH "/" \
                                    NVPD_SOCKET_NAME
#define NVPD_MAX_STRLEN             255

/*
 * Useful format for printing PCI devices
 */
#define PCI_DEVICE_FMT              "%04d:%02x:%02x:%x"

#endif /* _NVPD_DEFS_H_ */
