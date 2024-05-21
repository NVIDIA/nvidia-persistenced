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

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>

#include "nvidia-persistenced.h"
#include "nvpd_rpc.h"

static NvPdStatus _nvpdIsClientRoot(struct svc_req *req)
{
    struct ucred ucred = { -1, -1, -1 };
    socklen_t ucred_len = sizeof(struct ucred);

    if (getsockopt(req->rq_xprt->xp_sock,
                   SOL_SOCKET, SO_PEERCRED,
                   &ucred, &ucred_len) < 0) {
        return NVPD_ERR_UNKNOWN;
    }

    if (ucred.uid != 0) {
        return NVPD_ERR_PERMISSIONS;
    }

    return NVPD_SUCCESS;
}

/*!
 * nvpdsetpersistencemode_1_svc() - This service is an RPC function
 * implementation to set the persistence mode of a specific device.
 */
NvPdStatus* nvpdsetpersistencemode_1_svc(SetPersistenceModeArgs *args,
                                         struct svc_req *req)
{
    static NvPdStatus result;

    result = _nvpdIsClientRoot(req);
    if (result != NVPD_SUCCESS) {
        return &result;
    }

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


/*!
 * nvpdsetpersistencemodeonly_2_svc() - This service is an RPC function
 * implementation to set the persistence mode of a specific device
 * without affecting the NUMA status of the device.
 */
NvPdStatus* nvpdsetpersistencemodeonly_2_svc(SetPersistenceModeArgs *args,
                                             struct svc_req *req)
{
    static NvPdStatus result;

    result = _nvpdIsClientRoot(req);
    if (result != NVPD_SUCCESS) {
        return &result;
    }

    result = nvPdSetDevicePersistenceModeOnly(args->device.domain,
                                              args->device.bus,
                                              args->device.slot,
                                              args->device.function,
                                              args->mode);

    return &result;
}

/*!
 * nvpdsetnumastatus_2_svc() - This service is an RPC function
 * implementation to set the NUMA status of the device.
 */
NvPdStatus* nvpdsetnumastatus_2_svc(SetNumaStatusArgs *args,
                                    struct svc_req *req)
{
    static NvPdStatus result;

    result = _nvpdIsClientRoot(req);
    if (result != NVPD_SUCCESS) {
        return &result;
    }

    result = nvPdSetDeviceNumaStatus(args->device.domain,
                                     args->device.bus,
                                     args->device.slot,
                                     args->device.function,
                                     args->status);

    return &result;
}
