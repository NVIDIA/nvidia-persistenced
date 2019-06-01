/*
 * nvidia-persistenced: A daemon for maintaining persistent driver state,
 * specifically for use by the NVIDIA Linux driver.
 *
 * Copyright (C) 2013-2018 NVIDIA Corporation
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
 * nvidia-persistenced.c
 */

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "nvidia-persistenced.h"
#include "nvpd_defs.h"
#include "nvpd_rpc.h"
#include "nvidia-numa.h"
#include "nvidia-syslog-utils.h"
/*
 * Local Definitions
 */
#define NVPD_PID_FILE   NVPD_VAR_RUNTIME_DATA_PATH "/" NVPD_DAEMON_NAME ".pid"
#define NVIDIA_CFG_LIB  "libnvidia-cfg.so.1"

typedef struct
{
    NvCfgDeviceHandle nv_cfg_handle;
    NvCfgPciDevice pci_info;
    NvPersistenceMode mode;
    NvNumaStatus numa_status;
    NvNumaDevice numa_info;
} NvPdDevice;

/*
 * Static Variables
 */
static void *libnvidia_cfg = NULL;
static pid_t pid = 0;
static int pid_fd = -1;
static int socket_fd = -1;
static NvPdDevice *devices = NULL;
static int num_devices = 0;
static int remove_dir = 0;

static struct {
    NvCfgBool (*get_pci_devices)(int *, NvCfgPciDevice **);
    NvCfgBool (*open_pci_device)(int, int, int, int, NvCfgDeviceHandle *);
    NvCfgBool (*close_device)(NvCfgDeviceHandle);
} nv_cfg_api;

/*
 * Local Functions
 */
static int daemonize(uid_t uid, gid_t gid);
static int load_nvidia_cfg_sym(void **sym_ptr, const char *sym_name);
static NvPdDevice *get_device(int domain, int bus, int slot);
static NvPdStatus setup_nvidia_cfg_api(const char *nvidia_cfg_path);
static NvPdStatus setup_devices(NvPersistenceMode default_mode);
static NvPdStatus setup_rpc(void);
static NvPdStatus set_device_mode(NvPdDevice *device, NvPersistenceMode mode);
static NvPdStatus set_device_numa_status(NvPdDevice *device,
                                         NvNumaStatus numa_status);

/*
 * nvPdSetDevicePersistenceMode() - This function implements the daemon
 * command to set the persistence mode of the device at the specified PCI
 * location.
 *
 * The function parameter is ignored for the time being, and provided for
 * completeness of the API.
 */
NvPdStatus nvPdSetDevicePersistenceMode(int domain, int bus, int slot,
                                        int function, NvPersistenceMode mode)
{
    NvPdStatus ret;
    NvPersistenceMode old_mode;
    NvPdDevice *device = get_device(domain, bus, slot);

    if (device == NULL) {
        return NVPD_ERR_DEVICE_NOT_FOUND;
    }

    old_mode = device->mode;

    /*
     * Set the device mode always before changing the NUMA state.
     * For onlining, this is needed since libnvidia-cfg needs to create the
     * device nodes before nvidia-numa can interact with them.
     * For offlining, this is needed since the libnvidia-cfg device handle
     * will need to be released for nvidia-numa to proceed with the offlining.
     */
    ret = set_device_mode(device, mode);
    if (ret == NVPD_SUCCESS) {
        NvNumaStatus status = (mode == NV_PERSISTENCE_MODE_ENABLED) ?
                                NV_NUMA_STATUS_ONLINE : NV_NUMA_STATUS_OFFLINE;
        ret = set_device_numa_status(device, status);
        if ((ret != NVPD_SUCCESS) && (old_mode != mode)) {
            (void) set_device_mode(device, old_mode);
        }
    }

    return ret;
}

/*
 * nvPdSetDevicePersistenceModeOnly() - This function implements the daemon
 * command to set the persistence mode of the device at the specified PCI
 * location, without affecting the NUMA status of the device.
 *
 * The function parameter is ignored for the time being, and provided for
 * completeness of the API.
 */
NvPdStatus nvPdSetDevicePersistenceModeOnly(int domain, int bus, int slot,
                                            int function,
                                            NvPersistenceMode mode)
{
    NvPdDevice *device = get_device(domain, bus, slot);

    if (device == NULL) {
        return NVPD_ERR_DEVICE_NOT_FOUND;
    }

    return set_device_mode(device, mode);
}

/*
 * nvPdSetDeviceNumaStatus() - This function implements the daemon command to
 * set the NUMA status of the device at the specified PCI location, without
 * affecting the persistence mode of the device.
 *
 * The function parameter is ignored for the time being, and provided for
 * completeness of the API.
 */
NvPdStatus nvPdSetDeviceNumaStatus(int domain, int bus, int slot, int function,
                                   NvNumaStatus status)
{
    NvPdDevice *device = get_device(domain, bus, slot);

    if (device == NULL) {
        return NVPD_ERR_DEVICE_NOT_FOUND;
    }

    return set_device_numa_status(device, status);
}

/*
 * nvPdGetDevicePersistenceMode() - This function implements the daemon
 * command to get the persistence mode of the device at the specified PCI
 * location.
 *
 * The function parameter is ignored for the time being, and provided for
 * completeness of the API.
 */
NvPdStatus nvPdGetDevicePersistenceMode(int domain, int bus, int slot,
                                        int function, NvPersistenceMode *mode)
{
    NvPdDevice *device = get_device(domain, bus, slot);

    if (device == NULL) {
        return NVPD_ERR_DEVICE_NOT_FOUND;
    }

    *mode = device->mode;
    return NVPD_SUCCESS;
}

/*
 * get_device() - looks up and returns a pointer to the NvPdDevice structure
 * for the device at the specified PCI location.
 */
static NvPdDevice *get_device(int domain, int bus, int slot)
{
    int i;

    if (devices != NULL) {
        for (i = 0; i < num_devices; i++) {
            if ((devices[i].pci_info.domain == domain) &&
                (devices[i].pci_info.bus == bus) &&
                (devices[i].pci_info.slot == slot)) {
                return &devices[i];
            }
        }
    }

    return NULL;
}

/*
 * init_complete() - called by the child (daemon) process to signal to the
 * parent process, via the init pipe created during daemonize(), that
 * initialization has completed successfully.
 */
static NvPdStatus init_complete(int pipe_write_fd)
{
    unsigned char success = 1;
    int bytes;
    
    bytes = write(pipe_write_fd, &success, sizeof(success));
    
    close(pipe_write_fd);

    if (bytes < 0) {
        fprintf(stderr, "Failed to write init pipe: %s\n", strerror(errno));
        return NVPD_ERR_IO;
    }

    return NVPD_SUCCESS;
}

/*
 * wait_for_init_complete() - called by the parent process to block on the
 * init pipe and wait for the child process to signal its successful
 * initialization.
 *
 * The init pipe will be closed upon return from this function.
 */
static NvPdStatus wait_for_init_complete(int pipe_read_fd)
{
    unsigned char success = 0;
    int bytes;

    bytes = read(pipe_read_fd, &success, sizeof(success));

    close(pipe_read_fd);    

    if (bytes < 0) {
        fprintf(stderr, "Failed to read init pipe: %s\n", strerror(errno));
        return NVPD_ERR_IO;
    }

    if (bytes != sizeof(success) || !success) {
        fprintf(stderr, "nvidia-persistenced failed to initialize. "
                        "Check syslog for more details.\n");
        return NVPD_ERR_UNKNOWN;
    }

    return NVPD_SUCCESS;
}

/*
 * set_device_mode() - This function performs the heavy lifting in enabling or
 * disabling device mode for a given device by performing mode checks and
 * calling libnvidia-cfg to open and close the device.
 */
static NvPdStatus set_device_mode(NvPdDevice *device, NvPersistenceMode mode)
{
    NvPdStatus status = NVPD_SUCCESS;
    NvCfgBool success;

    /* If the device is already in the mode specified, just abort */
    if (mode == device->mode) {
        SYSLOG_DEVICE_VERBOSE(&device->pci_info, LOG_NOTICE,
                              "already in requested persistence mode.");
        return status;
    }

    switch (mode) {

    case NV_PERSISTENCE_MODE_DISABLED:

        /* If the new mode is disabled, we must close the device. */
        success = nv_cfg_api.close_device(device->nv_cfg_handle);
        if (!success) {
            syslog_device(&device->pci_info, LOG_ERR, "failed to close.");
            status = NVPD_ERR_DRIVER;
        } else {
            device->nv_cfg_handle = NULL;
        }

        break;

    case NV_PERSISTENCE_MODE_ENABLED:

        /* If the new mode is enabled, we must open the device. */
        success = nv_cfg_api.open_pci_device(device->pci_info.domain,
                                             device->pci_info.bus,
                                             device->pci_info.slot,
                                             device->pci_info.function,
                                             &device->nv_cfg_handle);
        if (!success) {
            syslog_device(&device->pci_info, LOG_ERR, "failed to open.");
            status = NVPD_ERR_DRIVER;
        }

        break;

    default:

        syslog_device(&device->pci_info, LOG_ERR,
                      "requested invalid mode %d", mode);
        status = NVPD_ERR_INVALID_ARGUMENT;
        break;

    }

    if (status == NVPD_SUCCESS) {
        device->mode = mode;
        SYSLOG_DEVICE_VERBOSE(&device->pci_info, LOG_NOTICE,
                              "persistence mode %s.",
                              (mode == NV_PERSISTENCE_MODE_ENABLED) ?
                                "enabled" : "disabled");
    }

    return status;
}

/*
 * set_device_numa_status() - This function invokes the nvidia-numa functions
 * for onlining or offlining device NUMA memory for a given device.
 */
static NvPdStatus set_device_numa_status(NvPdDevice *device,
                                         NvNumaStatus numa_status)
{
    NvPdStatus status = NVPD_SUCCESS;

    /* If the device is already in the state specified, just abort */
    if (numa_status == device->numa_status) {
        SYSLOG_DEVICE_VERBOSE(&device->pci_info, LOG_NOTICE,
                              "NUMA memory already in requested state.");
        return status;
    }

    switch (numa_status) {

    case NV_NUMA_STATUS_OFFLINE:

        status = nvNumaOfflineMemory(&device->numa_info);
        if (status != NVPD_SUCCESS) {
            syslog_device(&device->pci_info, LOG_ERR,
                          "failed to offline memory.\n");
        }

        break;

    case NV_NUMA_STATUS_ONLINE:

        status = nvNumaOnlineMemory(&device->numa_info);
        if (status != NVPD_SUCCESS) {
            syslog_device(&device->pci_info, LOG_ERR,
                          "failed to online memory.\n");
        }

        break;

    default:

        syslog_device(&device->pci_info, LOG_ERR,
                      "requested invalid NUMA status %d", numa_status);
        status = NVPD_ERR_INVALID_ARGUMENT;
        break;

    }

    if (status == NVPD_SUCCESS) {
        device->numa_status = numa_status;
        SYSLOG_DEVICE_VERBOSE(&device->pci_info, LOG_NOTICE,
                              "NUMA memory %s.",
                              (numa_status == NV_NUMA_STATUS_ONLINE) ?
                                "onlined" : "offlined");
    }

    return status;
}

/*
 * shutdown_daemon() - This function systematically tears down state that was
 * created while setting up the daemon. This function assumes that it has
 * control over the runtime files, e.g., that no other instance of the daemon
 * is using them, so they can be deleted.
 */
static void shutdown_daemon(int status)
{
    int i;

    /* Nothing to clean up */
    if (pid <= 0) {
        goto shutdown;
    }

    /* Clean up and remove the RPC socket */
    if (socket_fd != -1) {
        /* Unregister any mappings to the RPC dispatch routines */
        svc_unregister(NVPD_PROG, VersionOne);
        svc_unregister(NVPD_PROG, VersionTwo);

        if (close(socket_fd) < 0) {
            syslog(LOG_ERR, "Failed to close socket: %s",
                   strerror(errno));
        } else {
            SYSLOG_VERBOSE(LOG_INFO, "Socket closed.");
        }

        if (unlink(NVPD_SOCKET_PATH) < 0) {
            syslog(LOG_ERR, "Failed to unlink socket: %s",
                   strerror(errno));
        }
    }

    /* Detach and free all devices */
    if (devices != NULL) {
        for (i = 0; i < num_devices; i++) {
            if (devices[i].nv_cfg_handle != NULL) {
                NvPersistenceMode mode = NV_PERSISTENCE_MODE_DISABLED;
                (void) nvPdSetDevicePersistenceMode(devices[i].pci_info.domain,
                                                    devices[i].pci_info.bus,
                                                    devices[i].pci_info.slot,
                                                    0, mode);
            }
        }

        free(devices);
    }

    /* Release the libnvidia-cfg handle */
    if (libnvidia_cfg != NULL) {
        dlclose(libnvidia_cfg);
        libnvidia_cfg = NULL;
    }

    /* Clean up and remove the PID file */
    if (pid_fd != -1) {
        /* Unlock the PID file */
        if (lockf(pid_fd, F_ULOCK, 0) < 0) {
            syslog(LOG_ERR, "Failed to unlock PID file: %s",
                   strerror(errno));
        } else {
            SYSLOG_VERBOSE(LOG_INFO, "PID file unlocked.");
        }

        /* Close the PID file */
        if (close(pid_fd) < 0) {
            syslog(LOG_ERR, "Failed to close PID file: %s",
                   strerror(errno));
        } else {
            SYSLOG_VERBOSE(LOG_INFO, "PID file closed.");
        }

        /* Remove the PID file */
        if (unlink(NVPD_PID_FILE) < 0) {
            syslog(LOG_ERR, "Failed to unlink PID file: %s",
                   strerror(errno));
        }
    }

    /*
     * Remove the runtime data directory if the daemon created it. If the
     * daemon has dropped permissions and is no longer able to remove the
     * directory, issue a notice instead of a warning, as this is expected.
     */
    if (remove_dir && (rmdir(NVPD_VAR_RUNTIME_DATA_PATH) < 0) &&
        (errno != ENOENT)) {
        if (errno == EACCES) {
            SYSLOG_VERBOSE(LOG_NOTICE,
                           "The daemon no longer has permission to remove its "
                           "runtime data directory %s",
                           NVPD_VAR_RUNTIME_DATA_PATH);
        } else {
            syslog(LOG_WARNING, "Failed to remove runtime data directory: %s",
                   strerror(errno));
        }
    }

    syslog(LOG_NOTICE, "Shutdown (%d)", pid);
    closelog();

shutdown:
    exit(status);
}

/*
 * load_nvidia_cfg_sym() - This function loads a specific symbol from the
 * nvidia-cfg library. Arguments are assumed to be valid.
 */
static int load_nvidia_cfg_sym(void **sym_ptr, const char *sym_name)
{
    *sym_ptr = dlsym(libnvidia_cfg, sym_name);
    if (*sym_ptr == NULL) {
        syslog(LOG_ERR, "Failed to load symbol %s from %s: %s",
               sym_name, NVIDIA_CFG_LIB, dlerror());
        return -1;
    }

    return 0;
}

/*
 * setup_nvidia_cfg_api() - This function loads the nvidia-cfg dynamic library
 * and queries the required symbols from it.
 */
static NvPdStatus setup_nvidia_cfg_api(const char *nvidia_cfg_path)
{
    char *lib_path;
    int status = 0;

    if (nvidia_cfg_path != NULL) {
        lib_path = nvstrcat(nvidia_cfg_path, "/", NVIDIA_CFG_LIB, NULL);
    } else {
        lib_path = NVIDIA_CFG_LIB;
    }

    libnvidia_cfg = dlopen(lib_path, RTLD_NOW);

    if (nvidia_cfg_path != NULL) {
        nvfree(lib_path);
    }

    if (libnvidia_cfg == NULL) {
        syslog(LOG_ERR, "Failed to open %s: %s", NVIDIA_CFG_LIB,
               dlerror());
        return NVPD_ERR_DRIVER;
    }

    /* Attempt to load all symbols required. */
    status |= load_nvidia_cfg_sym((void **)&nv_cfg_api.get_pci_devices,
                                  "nvCfgGetPciDevices");
    status |= load_nvidia_cfg_sym((void **)&nv_cfg_api.open_pci_device,
                                  "nvCfgOpenPciDevice");
    status |= load_nvidia_cfg_sym((void **)&nv_cfg_api.close_device,
                                  "nvCfgCloseDevice");

    if (status != 0) {
        /* Missing symbols are already called out by load_nvidia_cfg_sym(). */
        return NVPD_ERR_DRIVER;
    }

    return NVPD_SUCCESS;
}

/*
 * setup_devices() - This function gets a list of devices and initializes the
 * daemon state for each one.
 */
static NvPdStatus setup_devices(NvPersistenceMode default_mode)
{
    NvCfgBool success;
    NvCfgPciDevice *nv_cfg_devices;
    int i;

    success = nv_cfg_api.get_pci_devices(&num_devices, &nv_cfg_devices);
    if (!success) {
        syslog(LOG_ERR, "Failed to query NVIDIA devices. Please ensure that "
                        "the NVIDIA device files (/dev/nvidia*) exist, and "
                        "that user %u has read and write permissions for "
                        "those files.", getuid());
        return NVPD_ERR_DRIVER;
    }

    if (num_devices < 1) {
        syslog(LOG_ERR, "Unable to find any NVIDIA devices");
        return NVPD_ERR_DEVICE_NOT_FOUND;
    }

    /* Allocate our own device table */
    devices = (NvPdDevice *)malloc(num_devices * sizeof(NvPdDevice));
    if (devices == NULL) {
        syslog(LOG_ERR, "Failed to create device table");
        return NVPD_ERR_INSUFFICIENT_RESOURCES;
    }

    memset(devices, 0, num_devices * sizeof(NvPdDevice));

    for (i = 0; i < num_devices; i++) {
        devices[i].nv_cfg_handle = NULL;
        devices[i].pci_info = nv_cfg_devices[i];

        /* nvidia-cfg doesn't fill in the PCI function field, assume 0 */
        devices[i].pci_info.function = 0;
        devices[i].mode = NV_PERSISTENCE_MODE_DISABLED;

        /* Initialize nvidia-numa state */
        devices[i].numa_status = NV_NUMA_STATUS_OFFLINE;
        devices[i].numa_info.fd = -1;
        devices[i].numa_info.pci_info = &devices[i].pci_info;

        SYSLOG_DEVICE_VERBOSE(&(devices[i].pci_info), LOG_DEBUG, "registered");

        if (default_mode != NV_PERSISTENCE_MODE_DISABLED) {
            (void) nvPdSetDevicePersistenceMode(devices[i].pci_info.domain,
                                                devices[i].pci_info.bus,
                                                devices[i].pci_info.slot, 0,
                                                default_mode);
        }
    }

    /*
     * Free the NvCfg device array, now that we've extracted all the necessary
     * info from it.
     */
    free(nv_cfg_devices);

    return NVPD_SUCCESS;
}

/*
 * setup_rpc() - This function starts up the RPC services that the daemon
 * provides. It is derived from the sample auto-generated by rpcgen.
 */
static NvPdStatus setup_rpc()
{
    register SVCXPRT *transp;

    /*
     * We should remove any stale sockets on the filesystem before attempting
     * to create it again.
     */
    (void)unlink(NVPD_SOCKET_PATH);

    /* Create the socket manually so we can shut it down later */
    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        return NVPD_ERR_IO;
    }

    /* Create the RPC service over the Unix-domain socket */
    transp = svcunix_create(socket_fd, 0, 0, NVPD_SOCKET_PATH);
    if (transp == NULL) {
        syslog(LOG_ERR, "Failed to create RPC service");
        return NVPD_ERR_RPC;
    }

    if (!svc_register(transp, NVPD_PROG, VersionOne, nvpd_prog_1, 0)) {
        syslog(LOG_ERR, "Failed to register RPC V1 service");
        return NVPD_ERR_RPC;
    }

    if (!svc_register(transp, NVPD_PROG, VersionTwo, nvpd_prog_2, 0)) {
        syslog(LOG_ERR, "Failed to register RPC V2 service");
        return NVPD_ERR_RPC;
    }

    SYSLOG_VERBOSE(LOG_INFO, "Local RPC services initialized");

    return NVPD_SUCCESS;
}

/*
 * signal_handler() - This function simply catches and processes relevant
 * signals sent to the daemon.
 */
static void signal_handler(int signal)
{
    SYSLOG_VERBOSE(LOG_DEBUG, "Received signal %d", signal);

    switch (signal) {

    case SIGINT:
    case SIGTERM:
        shutdown_daemon(EXIT_SUCCESS);
        break;
    default:
        syslog(LOG_WARNING, "Unable to process signal %d",
               signal);
        break;

    }
}

/*
 * daemonize() - This function converts the current process into a daemon
 * process.
 */
static int daemonize(uid_t uid, gid_t gid)
{
    char pid_str[10];
    struct sigaction signal_action;
    sigset_t signal_set;
    int init_pipe_fds[2];
    int pipe_read_fd, pipe_write_fd;
    int fd;

    /*
     * Set up the signal handler - block TTY-related signals, catch
     * termination signals.
     */
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGCHLD);
    sigaddset(&signal_set, SIGTSTP);
    sigaddset(&signal_set, SIGTTOU);
    sigaddset(&signal_set, SIGTTIN);
    sigprocmask(SIG_BLOCK, &signal_set, NULL);

    signal_action.sa_handler = signal_handler;
    sigemptyset(&signal_action.sa_mask);
    signal_action.sa_flags = 0;

    /*
     * Set up the init pipe for coordinating daemon init with main process
     * return.
     */
    if (pipe(init_pipe_fds) < 0) {
        fprintf(stderr, "Failed to create init pipe: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    pipe_read_fd = init_pipe_fds[0];
    pipe_write_fd = init_pipe_fds[1];

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Failed to fork() daemon: %s", strerror(errno));
        goto shutdown;
    }
    else if (pid > 0) {
        NvPdStatus init_status;

        /*
         * Close the write end of the pipe so we don't block if the child dies
         * or otherwise closes its write fd before it sends the init message.
         */
        close(pipe_write_fd);

        /*
         * Watch the init pipe for the child process to init, then exit the
         * parent process.
         */
        init_status = wait_for_init_complete(pipe_read_fd);
        exit((init_status == NVPD_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    if (verbose) {
        log_mask = LOG_UPTO(LOG_DEBUG);
    } else {
        log_mask = LOG_UPTO(LOG_NOTICE);
    }

    setlogmask(log_mask);

    /* Setup syslog connection */
    openlog(NVPD_DAEMON_NAME, 0, LOG_DAEMON);
    SYSLOG_VERBOSE(LOG_INFO, "Verbose syslog connection opened");

    sigaction(SIGINT,  &signal_action, NULL);
    sigaction(SIGTERM, &signal_action, NULL);

    /* Reset default file permissions */
    umask(0);

    /* Create a new session for the daemon */
    if (setsid() < 0) {
        fprintf(stderr, "Failed to create new daemon session: %s",
                strerror(errno));
        goto shutdown;
    }

    /* Save off the new pid for logging */
    pid = getpid();

    /* Close the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* Close the read end of the init pipe */
    close(pipe_read_fd);

    /* Go somewhere that we won't be unmounted */
    if (chdir("/") < 0) {
        syslog(LOG_WARNING, "Failed to change working directory: %s",
               strerror(errno));
    }

    /*
     * Try to create the supplied data path - if it fails, we'll catch the
     * error with the access() call below.
     */
    if (mkdir(NVPD_VAR_RUNTIME_DATA_PATH, 0755) < 0) {
        if (errno != EEXIST) {
            syslog(LOG_WARNING, "Failed to create directory %s: %s",
                   NVPD_VAR_RUNTIME_DATA_PATH, strerror(errno));
        }

        SYSLOG_VERBOSE(LOG_INFO, "Directory %s will not be removed on exit",
                       NVPD_VAR_RUNTIME_DATA_PATH);
    } else {
        /*
         * Only attempt to remove the directory on shutdown if the daemon
         * created it.
         */
        remove_dir = 1;
    }

    /*
     * If the user ID or group ID are different than the current, change the
     * ownership of the runtime data directory and drop permissions now.
     */
    if ((getuid() != uid) || (getgid() != gid)) {
        if (chown(NVPD_VAR_RUNTIME_DATA_PATH, uid, gid) < 0) {
            syslog(LOG_ERR, "Failed to change ownership of %s: %s",
                   NVPD_VAR_RUNTIME_DATA_PATH, strerror(errno));
            goto shutdown;
        }

        if (setgid(gid) < 0) {
            syslog(LOG_ERR, "Failed to set group ID: %s", strerror(errno));
            goto shutdown;
        }

        if (setuid(uid) < 0) {
            syslog(LOG_ERR, "Failed to set user ID: %s", strerror(errno));
            goto shutdown;
        }

        SYSLOG_VERBOSE(LOG_INFO, "Now running with user ID %d and group ID %d",
                       uid, gid);
    }

    /*
     * Check that the supplied runtime data path is writable by the daemon.
     */
    if (access(NVPD_VAR_RUNTIME_DATA_PATH, R_OK | W_OK) < 0) {
        syslog(LOG_ERR, "Unable to access %s: %s",
               NVPD_VAR_RUNTIME_DATA_PATH, strerror(errno));
        goto shutdown;
    }

    /*
     * Make sure we're the only instance running.
     * This file should be user-writable, global-readable.
     */
    fd = open(NVPD_PID_FILE, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to open PID file: %s", strerror(errno));
        goto shutdown;
    }

    /* Lock the PID file */
    if (lockf(fd, F_TLOCK, 0) < 0) {
        syslog(LOG_ERR, "Failed to lock PID file: %s", strerror(errno));
        close(fd);
        goto shutdown;
    }

    /*
     * Once the PID file is locked, we need to clean it up during shutdown.
     */
    pid_fd = fd;

    /* Update the PID file with the current process ID */
    sprintf(pid_str, "%d\n", pid);
    if (write(pid_fd, pid_str, strlen(pid_str)) != strlen(pid_str)) {
        syslog(LOG_ERR, "Failed to update PID file: %s", strerror(errno));
        goto shutdown;
    }

    syslog(LOG_NOTICE, "Started (%d)", pid);
    return pipe_write_fd;

shutdown:
    close(pipe_write_fd);

    /*
     * If we get this far but have an error condition, we need to cleanup
     * any runtime files left around.
     */
    shutdown_daemon(EXIT_FAILURE);

    /* Unreachable */
    return -1;
}

int main(int argc, char* argv[])
{
    NvPdStatus status;
    NvPdOptions options;
    int pipe_write_fd;

    parse_options(argc, argv, &options);
    verbose = options.verbose;

    pipe_write_fd = daemonize(options.uid, options.gid);

    /* Only the daemon process reaches this point */
    status = setup_nvidia_cfg_api(options.nvidia_cfg_path);
    if (status != NVPD_SUCCESS) {
        goto shutdown;
    }

    status = setup_devices(options.persistence_mode);
    if (status != NVPD_SUCCESS) {
        goto shutdown;
    }

    status = setup_rpc();
    if (status != NVPD_SUCCESS) {
        goto shutdown;
    }

    status = init_complete(pipe_write_fd);
    if (status != NVPD_SUCCESS) {
        goto shutdown;
    }

    svc_run();

    /* We should never return from svc_run() in a non-error scenario */
    syslog(LOG_ERR, "Failed to start local RPC service");

shutdown:
    close(pipe_write_fd);
    shutdown_daemon(EXIT_FAILURE);

    return 0;
}
