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
 * nvidia-persistenced.h
 */

#ifndef _NVIDIA_PERSISTENCED_H_
#define _NVIDIA_PERSISTENCED_H_

#include <sys/types.h>

#include "nvpd_rpc.h"

/* Daemon Options */
typedef struct {
    NvPersistenceMode persistence_mode;
    char *nvidia_cfg_path;
    int verbose;
    uid_t uid;
    gid_t gid;
} NvPdOptions;

/* Command Implementations */
NvPdStatus nvPdSetDevicePersistenceMode(int domain, int bus, int slot,
                                        int function, NvPersistenceMode mode);
NvPdStatus nvPdGetDevicePersistenceMode(int domain, int bus, int slot,
                                        int function,
                                        NvPersistenceMode *mode);

/* RPC Service Routine */
extern void nvpd_prog_1(struct svc_req *rqstp, register SVCXPRT *transp);

/* Commandline Parsing */
extern void parse_options(int argc, char *argv[], NvPdOptions *);

#endif /* _NVIDIA_PERSISTENCED_H_ */
