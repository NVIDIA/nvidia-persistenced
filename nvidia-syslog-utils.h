/*
 * Copyright (c) 2017, NVIDIA CORPORATION.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This file provides utility functions for logging messages through
 * syslog
 */

#ifndef _NVIDIA_SYSLOG_UTILS_H_
#define _NVIDIA_SYSLOG_UTILS_H_

#include <syslog.h>

#include "common-utils.h"
#include "nvidia-cfg.h"

extern int verbose;
extern int log_mask;

void syslog_device(NvCfgPciDevice *device_pci_info, int priority,
                   const char *format, ...);

#define SYSLOG_DEVICE_VERBOSE(device, priority, format, ...) do {       \
    if (verbose)                                                        \
        syslog_device(device, priority, format, ##__VA_ARGS__);         \
} while(0)

#define SYSLOG_VERBOSE(priority, format, ...) do {                      \
    if (verbose)                                                        \
        syslog(priority, format, ##__VA_ARGS__);                        \
} while(0)

#endif /* _NVIDIA_SYSLOG_UTILS_H_ */
