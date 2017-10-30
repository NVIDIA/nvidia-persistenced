/*******************************************************************************
    Copyright (c) 2015-2017 NVIDIA Corporation

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

        The above copyright notice and this permission notice shall be
        included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

*******************************************************************************/
#ifndef _NV_GPU_NUMA_H_
#define _NV_GPU_NUMA_H_

#include "nvpd_rpc.h"

typedef enum 
{
    NV_IOCTL_NUMA_STATUS_DISABLED             = 0,
    NV_IOCTL_NUMA_STATUS_OFFLINE              = 1,
    NV_IOCTL_NUMA_STATUS_ONLINE_IN_PROGRESS   = 2,
    NV_IOCTL_NUMA_STATUS_ONLINE               = 3,
    NV_IOCTL_NUMA_STATUS_ONLINE_FAILED        = 4,
    NV_IOCTL_NUMA_STATUS_OFFLINE_IN_PROGRESS  = 5,
    NV_IOCTL_NUMA_STATUS_OFFLINE_FAILED       = 6
} mem_state_t;

/* system parameters that the kernel driver may use for configuration */
typedef struct nv_ioctl_sys_params
{
    uint64_t memblock_size;
} nv_ioctl_sys_params_t;

/* per-device NUMA memory info as assigned by the system */
typedef struct nv_ioctl_numa_info
{
    int nid;
    int status;
    uint64_t memblock_size;
    uint64_t numa_mem_addr;
    uint64_t numa_mem_size;
} nv_ioctl_numa_info_t;

/* set the status of the device NUMA memory */
typedef struct nv_ioctl_set_numa_status
{
    int status;
} nv_ioctl_set_numa_status_t;

NvPdStatus nvNumaOnlineMemory(int domain, int bus, int slot, int function);

NvPdStatus nvNumaOfflineMemory(int domain, int bus, int slot, int function);

#endif
