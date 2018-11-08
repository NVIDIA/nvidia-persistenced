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
 * command_server.c
 */

#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "nvidia-persistenced.h"
#include "nvpd_rpc.h"

/*!
 * nvpdsetpersistencemode_1_svc() - This service is an RPC function
 * implementation to set the persistence mode of a specific device.
 */
NvPdStatus* nvpdsetpersistencemode_1_svc(SetPersistenceModeArgs *args,
                                         struct svc_req *req)
{
    static NvPdStatus result;

    result = nvPdSetDevicePersistenceMode(args->device.domain,
                                          args->device.bus,
                                          args->device.slot,
                                          args->device.function,
                                          args->mode);

    return &result;
}

/*!
 * nvpdsetpersistencemode_1_svc() - This service is an RPC function
 * implementation to get the persistence mode of a specific device.
 */
GetPersistenceModeRes* nvpdgetpersistencemode_1_svc(GetPersistenceModeArgs *args,
                                                    struct svc_req *req)
{
    static GetPersistenceModeRes result;
    NvPersistenceMode *mode = &result.GetPersistenceModeRes_u.mode;

    result.status = nvPdGetDevicePersistenceMode(args->device.domain,
                                                 args->device.bus,
                                                 args->device.slot,
                                                 args->device.function,
                                                 mode);

    return &result;
}
