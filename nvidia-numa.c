/*
 * Copyright (c) 2018, NVIDIA CORPORATION.
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
 * This file provides utility functions for onlining and offlining
 * NUMA memory
 */

#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>

#include "common-utils.h"
#include "nv-ioctl-numa.h"
#include "nvidia-numa.h"

#define NV_DEVICE_INFO_PATH_FMT \
    "/proc/driver/nvidia/gpus/%04x:%02x:%02x.%x/information"
#define NV_DEVICE_FILE_NAME          "/dev/nvidia%d"

#define BUF_SIZE                     100
#define BRING_OFFLINE_CMD            "offline"
#define BRING_ONLINE_CMD             "online_movable"
#define MEMORY_PATH_FMT              "/sys/devices/system/memory"
#define MEMORY_HARD_OFFLINE_PATH_FMT MEMORY_PATH_FMT "/hard_offline_page"
#define MEMORY_PROBE_PATH_FMT        MEMORY_PATH_FMT "/probe"
#define MEMBLK_DIR_PATH_FMT          MEMORY_PATH_FMT "/memory%d"
#define MEMBLK_STATE_PATH_FMT        MEMBLK_DIR_PATH_FMT "/state"
#define MEMBLK_VALID_ZONES_PATH_FMT  MEMBLK_DIR_PATH_FMT "/valid_zones"
#define NID_PATH_FMT                 "/sys/devices/system/node/node%d"
#define STATE_ONLINE                 "online"
#define VALID_MOVABLE_STATE          "Movable"

#ifndef NV_IS_ALIGNED
#define NV_IS_ALIGNED(v, gran)       (0 == ((v) & ((gran) - 1)))
#endif

typedef int mem_state_t;

static inline char* mem_state_to_string(mem_state_t state)
{
    switch (state)
    {
        case NV_IOCTL_NUMA_STATUS_DISABLED:
            return "numa_status_disabled";
        case NV_IOCTL_NUMA_STATUS_OFFLINE:
            return "offline";
        case NV_IOCTL_NUMA_STATUS_ONLINE_IN_PROGRESS:
            return "online_in_progress";
        case NV_IOCTL_NUMA_STATUS_ONLINE:
            return "online";
        case NV_IOCTL_NUMA_STATUS_ONLINE_FAILED:
            return "numa_online_failed";
        case NV_IOCTL_NUMA_STATUS_OFFLINE_IN_PROGRESS:
            return "offline_in_progress";
        case NV_IOCTL_NUMA_STATUS_OFFLINE_FAILED:
            return "offline_failed";
        default:
            return "invalid_state";
    }
}

static
int get_gpu_minor_number(int domain, int bus, int slot, int function,
                         int *minor_num)
{
    FILE *fp;
    int status = 0;
    char *last_str = NULL;
    char info_path[BUF_SIZE], read_buf[BUF_SIZE];

    sprintf(info_path, NV_DEVICE_INFO_PATH_FMT, domain, bus, slot, function);

    fp = fopen(info_path, "r");
    if (!fp) {
        syslog(LOG_ERR, "NUMA: Failed to open %s: %s\n",
               info_path, strerror(errno));
        return -errno;
    }

    while (fgets(read_buf, BUF_SIZE, fp)) {
        if (strstr(read_buf, "Device Minor:")) {
            last_str = strrchr(read_buf, ':');
            break;
        }
    }

    /* This will get the value one after the delimiter ':' */
    if (last_str)
        *minor_num = atoi(last_str + 1);
    else {
        syslog(LOG_ERR, "NUMA: Failed to extract device minor number from %s\n",
               info_path);
        status = -EINVAL;
    }

    fclose(fp);
    return status;
}

static
int get_gpu_device_file(int domain, int bus, int slot, int function,
                        char *dev_file)
{
    int status = 0;
    int minor_num = 0;

    status = get_gpu_minor_number(domain, bus, slot, function, &minor_num);
    if (status < 0) {
        syslog(LOG_ERR, "NUMA: Failed to get device minor number\n");
        return status;
    }

    sprintf(dev_file, NV_DEVICE_FILE_NAME, minor_num);

    return status;
}

static
int get_gpu_device_file_fd(int domain, int bus, int slot, int function, int *fd)
{
    int status;
    char dev_file[BUF_SIZE];

    status = get_gpu_device_file(domain, bus, slot, function, dev_file);
    if (status < 0) {
        syslog(LOG_ERR, "NUMA: Failed to get device file\n");
        return status;
    }

    *fd = open(dev_file, O_RDWR);
    if (*fd < 0) {
        syslog(LOG_ERR, "NUMA: Failed to open %s: %s\n", dev_file,
               strerror(errno));
        return -errno;
    }

    return status;
}

static
int get_gpu_numa_info(int fd, nv_ioctl_numa_info_t *numa_info)
{
    int status = 0;
    uint32_t request;

    memset(numa_info, 0, sizeof(*numa_info));
    request = _IOWR(NV_IOCTL_MAGIC, NV_ESC_NUMA_INFO,
                    char[sizeof(*numa_info)]);

    if (ioctl(fd, request, numa_info) < 0) {
        syslog(LOG_ERR, "NUMA: Failed ioctl call to get device NUMA Info: %s\n",
               strerror(errno));
        status = -errno;
    }

    return status;
}

static
int set_gpu_numa_status(int fd, mem_state_t numa_state)
{
    int status = 0;
    uint32_t request;
    nv_ioctl_set_numa_status_t numa_status_params;

    memset(&numa_status_params, 0, sizeof(numa_status_params));
    request = _IOWR(NV_IOCTL_MAGIC, NV_ESC_SET_NUMA_STATUS,
                    char[sizeof(numa_status_params)]);

    numa_status_params.status = numa_state;

    if (ioctl(fd, request, &numa_status_params) < 0) {
        syslog(LOG_ERR,
               "NUMA: Failed ioctl call to set device NUMA status: %s\n",
               strerror(errno));
        status = -errno;
    }

    return status;
}

static
int read_string_from_file(const char *path_to_file, char *read_buffer,
                          size_t read_buffer_size)
{
    int fd;
    int read_count = 0;

    fd = open(path_to_file, O_RDONLY, S_IRUSR);
    if (fd < 0) {
        syslog(LOG_ERR, "NUMA: Failed to open %s: %s\n",
               path_to_file, strerror(errno));
        return -errno;
    }

    memset(read_buffer, 0, read_buffer_size);
    read_count = read(fd, read_buffer, read_buffer_size - 1);

    close(fd);

    if (read_count <= 0) {
        syslog(LOG_ERR, "NUMA: Failed to read %s: %s\n", path_to_file,
               strerror(errno));
        return -errno;
    }

    /* Trim trailing newlines */
    while (read_buffer[read_count - 1] == '\n') {
        read_count--;
    }

    read_buffer[read_count] = '\0';

    return 0;
}

static
int write_string_to_file(const char *path_to_file, const char *write_buffer,
                         size_t strLength)
{
    int fd;
    int write_count;

    fd = open(path_to_file, O_WRONLY | O_TRUNC, S_IWUSR);
    if (fd < 0) {
        syslog(LOG_ERR, "NUMA: Failed to open %s: %s\n",
               path_to_file, strerror(errno));
        return -errno;
    }

    write_count = write(fd, write_buffer, strLength);

    close(fd);

    if (((write_count > 0) && (write_count < strLength)) || (write_count < 0)) {
        return -errno;
    }

    return 0;
}

/*
 * Brings memory block online/offline using the sysfs memory-hotplug interface
 *   https://www.kernel.org/doc/Documentation/memory-hotplug.txt
 */
static
int change_memblock_state(uint32_t mem_block_id, mem_state_t new_state)
{
    int status = 0;
    const char *cmd;
    mem_state_t cur_state;
    char buf[BUF_SIZE];
    char numa_file_path[BUF_SIZE];

    sprintf(numa_file_path, MEMBLK_STATE_PATH_FMT, mem_block_id);

    status = read_string_from_file(numa_file_path, buf, sizeof(buf));
    if (status < 0)
        goto done;

    cur_state = !!strstr(buf, STATE_ONLINE) ?
                NV_IOCTL_NUMA_STATUS_ONLINE : NV_IOCTL_NUMA_STATUS_OFFLINE;

    if (cur_state == new_state)
        goto done;

    switch (new_state)
    {
        case NV_IOCTL_NUMA_STATUS_ONLINE:
            cmd = BRING_ONLINE_CMD;
            break;
        case NV_IOCTL_NUMA_STATUS_OFFLINE:
            cmd = BRING_OFFLINE_CMD;
            break;
        default:
            return -EINVAL;
    }

    status = write_string_to_file(numa_file_path, cmd, strlen(cmd));

done:
    if (status == 0) {
        SYSLOG_VERBOSE(LOG_DEBUG,
                       "NUMA: Successfully changed memblock state of %s to %s\n",
                       numa_file_path, mem_state_to_string(new_state));
    }
    else {
        SYSLOG_VERBOSE(LOG_DEBUG,
                       "NUMA: Failed to change state of %s to %s: %s\n",
                       numa_file_path, mem_state_to_string(new_state),
                       strerror(-status));
    }

    return status;
}

static
inline int get_memblock_id_from_dirname(const char *dirname, uint32_t *block_id)
{
    return (sscanf(dirname, "memory%" PRIu32, block_id) == 1) ? 0 : -EINVAL;
}

/*
 * Looks through NUMA nodes, finding the upper and lower bounds, and returns
 * those. The assumption is that the nodes are physically contiguous, so that
 * the intervening nodes do not need to be explicitly returned.
 */
static
int gather_memblock_ids_for_node(uint32_t node_id, uint32_t *memblock_start_id,
                                 uint32_t *memblock_end_id)
{
    DIR *dir_ptr;
    int status = 0;
    struct dirent *dir_entry;
    char numa_file_path[BUF_SIZE];
    uint32_t start_id = UINT32_MAX;
    uint32_t end_id = 0;

    sprintf(numa_file_path, NID_PATH_FMT, node_id);

    dir_ptr = opendir(numa_file_path);
    if (!dir_ptr) {
        syslog(LOG_ERR, "NUMA: Failed to open directory %s: %s\n",
               numa_file_path, strerror(errno));
        return -errno;
    }

    /* Iterate through the node directory and get the memblock id */
    while ((dir_entry = readdir(dir_ptr)) != NULL) {
        uint32_t memblock_id = 0;

        /* Skip entries that are not a memory node */
        if (get_memblock_id_from_dirname(dir_entry->d_name, &memblock_id) < 0) {
            continue;
        }

        if (memblock_id == 0) {
            syslog(LOG_ERR,
                   "NUMA: Failed to get memblock id while iterating through %s\n",
                   numa_file_path);
            goto cleanup;
        }

        SYSLOG_VERBOSE(LOG_DEBUG, "NUMA: Found memblock entry %"PRIu32"\n",
                       memblock_id);

        /* Record the smallest and largest assigned memblock IDs */
        start_id = (start_id < memblock_id) ? start_id : memblock_id;
        end_id = (end_id > memblock_id) ? end_id : memblock_id;
    }

    /*
     * If the wrong directory was specified, readdir can return success,
     * even though it never iterated any files in the directory. Make that case
     * also an error, by verifying that start_id has been set.
     */
    if (start_id == UINT32_MAX) {
        syslog(LOG_ERR, "NUMA: Failed to find any files in %s", numa_file_path);
        status = -ENOENT;
        goto cleanup;
    }

    *memblock_start_id = start_id;
    *memblock_end_id = end_id;

    SYSLOG_VERBOSE(LOG_DEBUG,
                   "NUMA: Found memblock start id: %"PRIu32
                   " and end id: %"PRIu32"\n",
                   *memblock_start_id, *memblock_end_id);

cleanup:
    closedir(dir_ptr);
    return status;
}

static
int change_numa_node_state(uint32_t node_id, uint64_t region_gpu_size,
                           uint64_t memblock_size, mem_state_t new_state)
{
    uint32_t memblock_id;
    int status = 0, err_status = 0;
    uint64_t blocks_changed = 0;
    uint32_t memblock_start_id = 0;
    uint32_t memblock_end_id = 0;

    status = gather_memblock_ids_for_node(node_id, &memblock_start_id,
                                          &memblock_end_id);
    if (status < 0) {
        syslog(LOG_ERR, "NUMA: Failed to get all memblock ID's for node%d\n",
               node_id);
        return status;
    }

    if (memblock_start_id > memblock_end_id) {
        syslog(LOG_ERR, "NUMA: Invalid memblock IDs were found for node%d\n",
               node_id);
        return -EINVAL;
    }

    SYSLOG_VERBOSE(LOG_DEBUG,
                   "NUMA: memblock ID range: %"PRIu32"-%"PRIu32
                   ", memblock size: 0x%"PRIx64"\n",
                   memblock_start_id, memblock_end_id, memblock_size);

    if (new_state == NV_IOCTL_NUMA_STATUS_ONLINE) {

        /*
         * Online ALL memblocks backwards first to allow placement into zone
         * movable. Issue discussed here:
         * https://patchwork.kernel.org/patch/9625081/
         */
        for (memblock_id = memblock_end_id;
             memblock_id >= memblock_start_id;
             memblock_id--) {
            status = change_memblock_state(memblock_id,
                                           NV_IOCTL_NUMA_STATUS_ONLINE);
            if (status == 0)
                blocks_changed++;
            else
                err_status = status;
        }
    }
    else if (new_state == NV_IOCTL_NUMA_STATUS_OFFLINE) {
        for (memblock_id = memblock_start_id;
             memblock_id <= memblock_end_id;
             memblock_id++) {
            status = change_memblock_state(memblock_id,
                                           NV_IOCTL_NUMA_STATUS_OFFLINE);

            if (status == 0)
                blocks_changed++;
            else
                err_status = status;
        }
    }

    /*
     * If not all of the requested blocks were changed, fail onlining
     */
    if ((blocks_changed * memblock_size) < region_gpu_size) {
        syslog(LOG_ERR,
               "NUMA: Failed to change the state of 0x%"PRIx64
               " blocks of the memory to %s\n",
               (region_gpu_size / memblock_size) - blocks_changed,
               mem_state_to_string(new_state));
        return err_status;
    }

    if (blocks_changed == 0) {
        syslog(LOG_ERR,
               "NUMA: Failed to change the state of numa memory to %s: "
               "No blocks were changed\n",
               mem_state_to_string(new_state));
        return -ENOMEM;
    }

    return 0;
}

static
int offline_blacklisted_pages(nv_offline_addresses_t *blacklist_addresses)
{
    int index;
    int status = 0;
    char blacklisted_addr_str[BUF_SIZE];

    for (index = 0; index < blacklist_addresses->numEntries; index++) {

        sprintf(blacklisted_addr_str, "0x%"PRIx64,
                blacklist_addresses->addresses[index]);

        SYSLOG_VERBOSE(LOG_NOTICE,
                       "NUMA: retiring memory address %s\n",
                       blacklisted_addr_str);

        status = write_string_to_file(MEMORY_HARD_OFFLINE_PATH_FMT,
                                      blacklisted_addr_str,
                                      strlen(blacklisted_addr_str));
        if (status < 0) {
            syslog(LOG_ERR,
                   "NUMA: Failed to retire memory address %s: %s\n",
                   blacklisted_addr_str, strerror(-status));
            return status;
        }
    }

    return status;
}

static
int probe_node_memory(uint64_t probe_base_addr, uint64_t region_gpu_size,
                      uint64_t memblock_size)
{
    int status = 0;
    int memory_num;
    char start_addr_str[BUF_SIZE];
    char memory_file_str[BUF_SIZE];
    uint64_t start_addr, numa_end_addr;

    numa_end_addr = probe_base_addr + region_gpu_size;

    if ((!NV_IS_ALIGNED(probe_base_addr, memblock_size)) ||
        (!NV_IS_ALIGNED(numa_end_addr, memblock_size))) {
        syslog(LOG_ERR, "NUMA: Probe ranges not aligned to memblock size!\n");
        return -EFAULT;
    }

    for (start_addr = probe_base_addr;
         start_addr + memblock_size <= numa_end_addr;
         start_addr += memblock_size) {

        sprintf(start_addr_str, "0x%"PRIx64, start_addr);

        SYSLOG_VERBOSE(LOG_DEBUG, "NUMA: Probing memory address %s\n",
                       start_addr_str);

        status = write_string_to_file(MEMORY_PROBE_PATH_FMT, start_addr_str,
                                      strlen(start_addr_str));

        memory_num = start_addr / memblock_size;
        sprintf(memory_file_str, MEMBLK_DIR_PATH_FMT, memory_num);

        /* Check if memory node was created */
        if (access(memory_file_str, F_OK) != 0) {
            syslog(LOG_ERR,
                   "NUMA: Failed to verify memory node %d was probed: %s\n",
                   memory_num, strerror(errno));
            status = -errno;
            goto done;
        }

        if (status == -EEXIST) {
            SYSLOG_VERBOSE(LOG_INFO,
                           "NUMA: Memory address %s already probed\n",
                           start_addr_str);
            status = 0;
            continue;
        } else if (status < 0) {
            syslog(LOG_ERR,
                   "NUMA: Failed to probe memory address %s: %s\n",
                   start_addr_str, strerror(-status));
            goto done;
        }
    }

done:
    return status;
}

static
int offline_memory(int fd)
{
    int status = 0;
    nv_ioctl_numa_info_t numa_info_params;

    memset(&numa_info_params, 0, sizeof(numa_info_params));

    status = get_gpu_numa_info(fd, &numa_info_params);
    if (status < 0)
    {
        syslog(LOG_ERR, "NUMA: Failed to get device NUMA info\n");
        return status;
    }

    /* Check if state from RM is valid */
    switch (numa_info_params.status)
    {
        case NV_IOCTL_NUMA_STATUS_DISABLED:
        case NV_IOCTL_NUMA_STATUS_OFFLINE:
            return 0;
        /* Allow an offline attempt if onlining/offlining fails */
        case NV_IOCTL_NUMA_STATUS_ONLINE_FAILED:
        case NV_IOCTL_NUMA_STATUS_OFFLINE_FAILED:
        /* This is the expected case */
        case NV_IOCTL_NUMA_STATUS_ONLINE:
        /* Onlining in progress implies some error during onlining */
        case NV_IOCTL_NUMA_STATUS_ONLINE_IN_PROGRESS:
            break;
        case NV_IOCTL_NUMA_STATUS_OFFLINE_IN_PROGRESS:
            syslog(LOG_ERR, "NUMA: NUMA status %s is invalid\n",
                   mem_state_to_string(numa_info_params.status));
            goto driver_fail;
    }

    status = set_gpu_numa_status(fd, NV_IOCTL_NUMA_STATUS_OFFLINE_IN_PROGRESS);
    if (status < 0) {
        syslog(LOG_ERR,
               "NUMA: Failed to set NUMA status to %s\n",
               mem_state_to_string(NV_IOCTL_NUMA_STATUS_OFFLINE_IN_PROGRESS));
        goto driver_fail;
    }

    status = change_numa_node_state(numa_info_params.nid,
                                    numa_info_params.numa_mem_size,
                                    numa_info_params.memblock_size,
                                    NV_IOCTL_NUMA_STATUS_OFFLINE);
    if (status < 0) {
        syslog(LOG_ERR, "NUMA: Changing node%d state to %s failed\n",
               numa_info_params.nid,
               mem_state_to_string(NV_IOCTL_NUMA_STATUS_OFFLINE));
        goto offline_failed;
    }

    status = set_gpu_numa_status(fd, NV_IOCTL_NUMA_STATUS_OFFLINE);
    if (status < 0) {
        syslog(LOG_ERR, "NUMA: Failed to set NUMA status to %s\n",
               mem_state_to_string(NV_IOCTL_NUMA_STATUS_OFFLINE));
        goto driver_fail;
    }

    syslog(LOG_NOTICE, "NUMA: Memory offlining completed!\n");

    return 0;

offline_failed:
    if (set_gpu_numa_status(fd, NV_IOCTL_NUMA_STATUS_OFFLINE_FAILED) < 0) {
        syslog(LOG_ERR, "NUMA: Failed to set NUMA status to %s\n",
               mem_state_to_string(NV_IOCTL_NUMA_STATUS_OFFLINE_FAILED));
    }
driver_fail:
    return status;
}

#define MEMORY_AUTO_ONLINE_WARNING_FMT                                      \
    "NUMA: %s state is online and the default zone is not movable (%s).\n"  \
    "This likely means that some non-NVIDIA software has auto-onlined\n"    \
    "the device memory before nvidia-persistenced could. Please check\n"    \
    "if the CONFIG_MEMORY_HOTPLUG_DEFAULT_ONLINE kernel config option\n"    \
    "is enabled or if udev has a memory auto-online rule enabled under\n"   \
    "/lib/udev/rules.d/."

static
int check_memory_auto_online(uint32_t node_id, NvCfgBool *auto_online_success)
{
    DIR     *dir_ptr;
    int     status = 0;
    struct  dirent *dir_entry;
    char    read_buf[BUF_SIZE];
    char    numa_file_path[BUF_SIZE];
    char    memory_file_path[BUF_SIZE];
    int     num_memory_node_in_dir = 0;
    int     num_memory_online_movable = 0;

    *auto_online_success = NVCFG_FALSE;

    sprintf(numa_file_path, NID_PATH_FMT, node_id);

    dir_ptr = opendir(numa_file_path);
    if (!dir_ptr) {
        syslog(LOG_ERR, "NUMA: Failed to open directory %s: %s\n",
               numa_file_path, strerror(errno));
        return -errno;
    }

    /* Iterate through the node directory */
    while ((dir_entry = readdir(dir_ptr)) != NULL) {
        uint32_t block_id;

        /* Skip entries that are not a memory node */
        if (get_memblock_id_from_dirname(dir_entry->d_name, &block_id) < 0) {
            continue;
        }

        num_memory_node_in_dir++;

        sprintf(memory_file_path, MEMBLK_STATE_PATH_FMT, block_id);

        status = read_string_from_file(memory_file_path,
                                       read_buf, sizeof(read_buf));
        if (status < 0) {
            syslog(LOG_ERR,
                   "NUMA: Failed to read %s state\n", dir_entry->d_name);
            goto cleanup;
        }

        /* Check if state has already been auto onlined */
        if (strstr(read_buf, STATE_ONLINE)) {

            SYSLOG_VERBOSE(LOG_NOTICE,
                           "NUMA: Device NUMA memory is already online\n");

            sprintf(memory_file_path, MEMBLK_VALID_ZONES_PATH_FMT, block_id);

            status = read_string_from_file(memory_file_path,
                                           read_buf, sizeof(read_buf));
            if (status < 0) {
                syslog(LOG_ERR,
                       "NUMA: Failed to read %s valid_zones\n",
                       dir_entry->d_name);
                goto cleanup;
            }

            /* If memory was auto-onlined, check if valid_zones is Movable */
            if (strstr(read_buf, VALID_MOVABLE_STATE) != read_buf) {
                syslog(LOG_NOTICE, MEMORY_AUTO_ONLINE_WARNING_FMT,
                       dir_entry->d_name, read_buf);
                status = -ENOTSUP;
                break;
            } else {
                num_memory_online_movable++;
            }
        }
    }

    /* Check if any memory nodes exist */
    if (num_memory_node_in_dir == 0) {
        syslog(LOG_ERR,
               "NUMA: No memory nodes in node%d directory!\n", node_id);
        status = -ENOENT;
        goto cleanup;
    }

    /* Check if all the memory are set to online movable */
    if (num_memory_online_movable == num_memory_node_in_dir) {
        *auto_online_success = NVCFG_TRUE;
    }

cleanup:
    closedir(dir_ptr);
    return status;
}

/*! @brief
 *  We assume the physical memory has been allocated from RM before calling this
 *  function.
 */
NvPdStatus nvNumaOnlineMemory(NvNumaDevice *numa_info)
{
    int fd;
    int status = 0;
    NvCfgPciDevice *device_pci_info = numa_info->pci_info;
    NvCfgBool auto_online_success;
    nv_ioctl_numa_info_t numa_info_params;

    memset(&numa_info_params, 0, sizeof(numa_info_params));

    status = get_gpu_device_file_fd(device_pci_info->domain,
                                    device_pci_info->bus,
                                    device_pci_info->slot,
                                    device_pci_info->function,
                                    &fd);
    if (status < 0) {
        syslog_device(device_pci_info,
                      LOG_ERR,
                      "NUMA: Failed to get device file descriptor\n");
        return NVPD_ERR_NUMA_FAILURE;
    }

    status = get_gpu_numa_info(fd, &numa_info_params);
    if (status < 0) {
        syslog_device(device_pci_info,
                      LOG_ERR,
                      "NUMA: Failed to get device NUMA info\n");
        goto driver_fail;
    }

    /* Check if numa status from RM is valid */
    switch (numa_info_params.status)
    {
        /* Allow an online attempt if onlining/offlining fails */
        case NV_IOCTL_NUMA_STATUS_ONLINE_FAILED:
        case NV_IOCTL_NUMA_STATUS_OFFLINE_FAILED:
        /* This is the expected case */
        case NV_IOCTL_NUMA_STATUS_OFFLINE:
            break;
        case NV_IOCTL_NUMA_STATUS_DISABLED:
        case NV_IOCTL_NUMA_STATUS_ONLINE:
            goto done;
        case NV_IOCTL_NUMA_STATUS_ONLINE_IN_PROGRESS:
        case NV_IOCTL_NUMA_STATUS_OFFLINE_IN_PROGRESS:
            syslog_device(device_pci_info,
                          LOG_ERR,
                          "NUMA: Device NUMA status %s is invalid\n",
                          mem_state_to_string(numa_info_params.status));
            goto driver_fail;
    }

    /* Check if numa_info_params are valid */
    if ((numa_info_params.nid < 0)            ||
        (numa_info_params.memblock_size == 0) ||
        (numa_info_params.numa_mem_addr == 0) ||
        (numa_info_params.numa_mem_size == 0)) {
        syslog_device(device_pci_info,
                      LOG_ERR,
                      "NUMA: Invalid device NUMA info. Nid: 0x%08x, "
                      "memblock_size: 0x%"PRIx64", numa_mem_addr: 0x%"
                      PRIx64", numa_mem_size: 0x%"PRIx64"\n",
                      numa_info_params.nid,
                      numa_info_params.memblock_size,
                      numa_info_params.numa_mem_addr,
                      numa_info_params.numa_mem_size);
        goto driver_fail;
    }

    status = set_gpu_numa_status(fd, NV_IOCTL_NUMA_STATUS_ONLINE_IN_PROGRESS);
    if (status < 0) {
        syslog_device(device_pci_info,
                      LOG_ERR,
                      "NUMA: Failed to set device NUMA status to %s\n",
                      mem_state_to_string(NV_IOCTL_NUMA_STATUS_ONLINE_IN_PROGRESS));
        goto driver_fail;
    }

    /* Otherwise we'll have a memory leak. */
    if ((!NV_IS_ALIGNED(numa_info_params.numa_mem_addr,
                        numa_info_params.memblock_size)) ||
        (!NV_IS_ALIGNED(numa_info_params.numa_mem_size,
                        numa_info_params.memblock_size))) {
        syslog_device(device_pci_info,
                      LOG_ERR,
                      "NUMA: Onlining range is not aligned to memblock size!\n");
        goto error;
    }

    status = probe_node_memory(numa_info_params.numa_mem_addr,
                               numa_info_params.numa_mem_size,
                               numa_info_params.memblock_size);
    if (status < 0) {
        syslog_device(device_pci_info,
                      LOG_ERR,
                      "NUMA: Probing memory failed: %d\n", status);
        goto online_failed;
    }

    /* Check if probed memory has been auto-onlined */
    status = check_memory_auto_online(numa_info_params.nid,
                                      &auto_online_success);
    if (status < 0) {
        if (status != -ENOTSUP) {
            syslog_device(device_pci_info,
                          LOG_ERR,
                          "NUMA: Failed to check if probed memory has "
                          "been auto-onlined\n");
        }
        goto error;
    }

    /* If memory was auto-onlined to Movable, skip changing node state */
    if (auto_online_success) {
        syslog_device(device_pci_info, LOG_NOTICE,
                      "NUMA: All device NUMA memory onlined and movable\n");
        goto set_driver_status;
    }

    status = change_numa_node_state(numa_info_params.nid,
                                    numa_info_params.numa_mem_size,
                                    numa_info_params.memblock_size,
                                    NV_IOCTL_NUMA_STATUS_ONLINE);
    if (status < 0) {
        syslog_device(device_pci_info,
                      LOG_ERR,
                      "NUMA: Changing node%d state to %s failed\n",
                      numa_info_params.nid,
                      mem_state_to_string(NV_IOCTL_NUMA_STATUS_ONLINE));
        goto online_failed;
    }

set_driver_status:
    status = offline_blacklisted_pages(&numa_info_params.offline_addresses);
    if (status < 0) {
        syslog_device(device_pci_info,
                      LOG_ERR,
                      "NUMA: Offlining blacklisted pages failed\n");
        goto online_failed;
    }

    status = set_gpu_numa_status(fd, NV_IOCTL_NUMA_STATUS_ONLINE);
    if (status < 0) {
        syslog_device(device_pci_info,
                      LOG_ERR,
                      "NUMA: Failed to set device NUMA status to %s\n",
                      mem_state_to_string(NV_IOCTL_NUMA_STATUS_ONLINE));
        goto online_failed;
    }

    syslog(LOG_NOTICE, "NUMA: Memory onlining completed!\n");
done:
    numa_info->fd = fd;
    return NVPD_SUCCESS;

online_failed:
    offline_memory(fd);
error:
    status = set_gpu_numa_status(fd, NV_IOCTL_NUMA_STATUS_ONLINE_FAILED);
    if (status < 0) {
        syslog_device(device_pci_info,
                      LOG_ERR,
                      "NUMA: Failed to set NUMA status to %s\n",
                      mem_state_to_string(NV_IOCTL_NUMA_STATUS_ONLINE_FAILED));
    }
driver_fail:
    close(fd);
    return NVPD_ERR_NUMA_FAILURE;
}

NvPdStatus nvNumaOfflineMemory(NvNumaDevice *numa_info)
{
    int fd = numa_info->fd;
    int status = 0;
    NvCfgPciDevice *device_pci_info = numa_info->pci_info;

    if (fd < 0) {
        syslog_device(device_pci_info,
                      LOG_ERR,
                      "NUMA: no file descriptor\n");
        return NVPD_ERR_NUMA_FAILURE;
    }

    status = offline_memory(fd);
    if (status < 0) {
        syslog_device(device_pci_info,
                      LOG_ERR,
                      "NUMA: Failed to offline memory\n");
        /* Do not close the fd, to avoid shutting down the device */
        return NVPD_ERR_NUMA_FAILURE;
    }

    close(fd);
    numa_info->fd = -1;
    return NVPD_SUCCESS;
}
