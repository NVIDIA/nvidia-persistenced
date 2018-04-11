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

#include "nvidia-syslog-utils.h"

int verbose = 0;
int log_mask = 0;

/*
 * syslog_device() - This function prints the device info along with the
 * specified message to syslog. If there is a failure to allocate memory along
 * the way, it will simply fail with an error to syslog().
 */
void syslog_device(NvCfgPciDevice *device_pci_info, int priority,
                   const char *format, ...)
{
    char *device_str = NULL;
    char *str = NULL;

    /* First check if this message will even show up. */
    if ((log_mask & priority) == 0) {
        return;
    }

    /* First fill in the format string as usual. */
    NV_VSNPRINTF(str, format);
    if (str == NULL) {
        syslog(LOG_ERR, "Failed to create formatted message.");
        return;
    }

    /* Then prefix it with the device info. */
    device_str = nvasprintf("device %04x:%02x:%02x.%x - %s",
                            device_pci_info->domain, device_pci_info->bus,
                            device_pci_info->slot, device_pci_info->function,
                            str);
    nvfree(str);
    if (device_str == NULL) {
        syslog(LOG_ERR, "Failed to create device message.");
        return;
    }

    syslog(priority, "%s", device_str);

    nvfree(device_str);
}
