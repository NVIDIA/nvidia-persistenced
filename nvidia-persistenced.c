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
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common-utils.h"
#include "nvidia-persistenced.h"
#include "nvidia-cfg.h"
#include "nvpd_defs.h"
#include "nvpd_rpc.h"

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
static int log_mask = 0;
static int remove_dir = 0;
static int verbose = 0;

static struct {
    NvCfgBool (*get_pci_devices)(int *, NvCfgPciDevice **);
    NvCfgBool (*attach_pci_device)(int, int, int, int, NvCfgDeviceHandle *);
    NvCfgBool (*detach_device)(NvCfgDeviceHandle);
} nv_cfg_api;

/*
 * Local Functions
 */
static void daemonize(uid_t uid, gid_t gid);
static int load_nvidia_cfg_sym(void **sym_ptr, const char *sym_name);
static NvPdStatus setup_nvidia_cfg_api(const char *nvidia_cfg_path);
static NvPdStatus setup_devices(NvPersistenceMode default_mode);
static NvPdStatus setup_rpc(void);
static NvPdStatus set_device_mode(NvPdDevice *device, NvPersistenceMode mode);
static void syslog_device(NvPdDevice *device, int priority,
                          const char *format, ...);

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
    int i = 0;
    NvPdStatus ret = NVPD_ERR_DEVICE_NOT_FOUND;

    if (devices == NULL) {
        return ret;
    }

    for (i = 0; i < num_devices; i++) {
        if ((devices[i].pci_info.domain == domain) &&
            (devices[i].pci_info.bus == bus) &&
            (devices[i].pci_info.slot == slot)) {
            ret = set_device_mode(&devices[i], mode);
            break;
        }
    }

    return ret;
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
    int i = 0;
    NvPdStatus ret = NVPD_ERR_DEVICE_NOT_FOUND;

    if (devices == NULL) {
        return ret;
    }

    for (i = 0; i < num_devices; i++) {
        if ((devices[i].pci_info.domain == domain) &&
            (devices[i].pci_info.bus == bus) &&
            (devices[i].pci_info.slot == slot)) {
            *mode = devices[i].mode;
            ret = NVPD_SUCCESS;
            break;
        }
    }

    return ret;
}

/*
 * syslog_device() - This function prints the device info along with the
 * specified message to syslog. If there is a failure to allocate memory along
 * the way, it will simply fail with an error to syslog().
 */
static void syslog_device(NvPdDevice *device, int priority,
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
    device_str = nvasprintf("device %04d:%02x:%02x.%x - %s",
                            device->pci_info.domain, device->pci_info.bus,
                            device->pci_info.slot, device->pci_info.function,
                            str);
    nvfree(str);
    if (device_str == NULL) {
        syslog(LOG_ERR, "Failed to create device message.");
        return;
    }

    syslog(priority, device_str);

    nvfree(device_str);
}

/*
 * set_device_mode() - This function performs the heavy lifting in enabling or
 * disabling device mode for a given device by performing mode checks and
 * calling libnvidia-cfg to attach and detach the device.
 */
static NvPdStatus set_device_mode(NvPdDevice *device, NvPersistenceMode mode)
{
    NvPdStatus status = NVPD_SUCCESS;
    NvCfgBool success;

    /* If the device is already in the mode specified, just abort */
    if (mode == device->mode) {
        if (verbose) {
            syslog_device(device, LOG_NOTICE,
                          "already in requested persistence mode.");
        }
        return status;
    }

    switch (mode) {

    case NV_PERSISTENCE_MODE_DISABLED:

        /* If the new mode is disabled, we must detach the device. */
        success = nv_cfg_api.detach_device(device->nv_cfg_handle);
        if (!success) {
            syslog_device(device, LOG_ERR, "failed to detach.");
            status = NVPD_ERR_DRIVER;
        } else {
            if (verbose) {
                syslog_device(device, LOG_NOTICE,
                              "persistence mode disabled.");
            }
            device->nv_cfg_handle = NULL;
        }
    
        break;

    case NV_PERSISTENCE_MODE_ENABLED:

        /* If the new mode is enabled, we must attach the device. */
        success = nv_cfg_api.attach_pci_device(device->pci_info.domain,
                                               device->pci_info.bus,
                                               device->pci_info.slot,
                                               device->pci_info.function,
                                               &device->nv_cfg_handle);
        if (!success) {
            syslog_device(device, LOG_ERR, "failed to attach.");
            status = NVPD_ERR_DRIVER;
        } else if (verbose) {
            syslog_device(device, LOG_NOTICE, "persistence mode enabled.");
        }

        break;

    default:

        syslog_device(device, LOG_ERR, "requested invalid mode %d", mode);
        status = NVPD_ERR_INVALID_ARGUMENT;
        break;

    }

    if (status == NVPD_SUCCESS) {
        device->mode = mode;
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

    /* Clean up and remove the RPC socket */
    if (socket_fd != -1) {
        /* Unregister any mappings to the RPC dispatch routine */
        svc_unregister(NVPD_PROG, VersionOne);

        if (close(socket_fd) < 0) {
            syslog(LOG_ERR, "Failed to close socket: %s",
                   strerror(errno));
        } else if (verbose) {
            syslog(LOG_INFO, "Socket closed.");
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
                (void) set_device_mode(&devices[i],
                                       NV_PERSISTENCE_MODE_DISABLED);
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
        } else if (verbose) {
            syslog(LOG_INFO, "PID file unlocked.");
        }

        /* Close the PID file */
        if (close(pid_fd) < 0) {
            syslog(LOG_ERR, "Failed to close PID file: %s",
                   strerror(errno));
        } else if (verbose) {
            syslog(LOG_INFO, "PID file closed.");
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
            syslog(LOG_NOTICE,
                   "The daemon no longer has permission to remove its runtime "
                   "data directory %s", NVPD_VAR_RUNTIME_DATA_PATH);
        } else {
            syslog(LOG_WARNING, "Failed to remove runtime data directory: %s",
                   strerror(errno));
        }
    }

    syslog(LOG_NOTICE, "Shutdown (%d)", pid);
    closelog();

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
    status |= load_nvidia_cfg_sym((void **)&nv_cfg_api.attach_pci_device,
                                  "nvCfgAttachPciDevice");
    status |= load_nvidia_cfg_sym((void **)&nv_cfg_api.detach_device,
                                  "nvCfgDetachDevice");

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

        if (verbose) {
            syslog_device(&devices[i], LOG_DEBUG, "registered");
        }

        if (default_mode != NV_PERSISTENCE_MODE_DISABLED) {
            (void) set_device_mode(&devices[i], default_mode);
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
        syslog(LOG_ERR, "Failed to register RPC service");
        return NVPD_ERR_RPC;
    }

    if (verbose) {
        syslog(LOG_INFO, "Local RPC service initialized");
    }

    return NVPD_SUCCESS;
}

/*
 * signal_handler() - This function simply catches and processes relevant
 * signals sent to the daemon.
 */
static void signal_handler(int signal)
{
    if (verbose) {
        syslog(LOG_DEBUG, "Received signal %d", signal);
    }

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
static void daemonize(uid_t uid, gid_t gid)
{
    char pid_str[10];
    struct sigaction signal_action;
    sigset_t signal_set;
    int status = EXIT_SUCCESS;

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

    sigaction(SIGINT,  &signal_action, NULL);
    sigaction(SIGTERM, &signal_action, NULL);

    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Failed to fork() daemon: %s",
               strerror(errno));
        exit(EXIT_FAILURE);
    }
    else if (pid > 0) {
        /* Kill off the parent process */
        exit(EXIT_SUCCESS);
    }

    /* Save off the new pid for logging */
    pid = getpid();

    /* Reset default file permissions */
    umask(0);

    /* Create a new session for the daemon */
    if (setsid() < 0) {
        syslog(LOG_ERR, "Failed to create new daemon session: %s",
               strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Close the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    if (verbose) {
        log_mask = LOG_UPTO(LOG_DEBUG);
    } else {
        log_mask = LOG_UPTO(LOG_NOTICE);
    }

    setlogmask(log_mask);

    /* Setup syslog connection */
    openlog(NVPD_DAEMON_NAME, 0, LOG_DAEMON);
    if (verbose) {
        syslog(LOG_INFO, "Verbose syslog connection opened");
    }

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

        if (verbose) {
            syslog(LOG_INFO, "Directory %s will not be removed on exit",
                   NVPD_VAR_RUNTIME_DATA_PATH);
        }
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
            status = EXIT_FAILURE;
            goto done;
        }

        if (setgid(gid) < 0) {
            syslog(LOG_ERR, "Failed to set group ID: %s", strerror(errno));
            status = EXIT_FAILURE;
            goto done;
        }

        if (setuid(uid) < 0) {
            syslog(LOG_ERR, "Failed to set user ID: %s", strerror(errno));
            status = EXIT_FAILURE;
            goto done;
        }

        if (verbose) {
            syslog(LOG_INFO, "Now running with user ID %d and group ID %d",
                   uid, gid);
        }
    }

    /*
     * Check that the supplied runtime data path is writable by the daemon.
     */
    if (access(NVPD_VAR_RUNTIME_DATA_PATH, R_OK | W_OK) < 0) {
        syslog(LOG_ERR, "Unable to access %s: %s",
               NVPD_VAR_RUNTIME_DATA_PATH, strerror(errno));
        status = EXIT_FAILURE;
        goto done;
    }

    /*
     * Make sure we're the only instance running.
     * This file should be user-writable, global-readable.
     */
    pid_fd = open(NVPD_PID_FILE, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (pid_fd < 0) {
        syslog(LOG_ERR, "Failed to open PID file: %s", strerror(errno));
        status = EXIT_FAILURE;
        goto done;
    }

    /* Lock the PID file */
    if (lockf(pid_fd, F_TLOCK, 0) < 0) {
        syslog(LOG_ERR, "Failed to lock PID file: %s", strerror(errno));
        status = EXIT_FAILURE;
        goto done;
    }

done:
    if (status == EXIT_SUCCESS) {
        /* Update the PID file with the current process ID */
        sprintf(pid_str, "%d\n", pid);
        write(pid_fd, pid_str, strlen(pid_str));
        syslog(LOG_NOTICE, "Started (%d)", pid);
    }
    else {
        /*
         * If we get this far but have an error condition, we need to cleanup
         * any runtime files left around.
         */
        shutdown_daemon(status);
    }
}

int main(int argc, char* argv[])
{
    NvPdStatus status;
    NvPdOptions options;

    parse_options(argc, argv, &options);
    verbose = options.verbose;

    daemonize(options.uid, options.gid);

    status = setup_nvidia_cfg_api(options.nvidia_cfg_path);
    if (status != NVPD_SUCCESS) {
        shutdown_daemon(EXIT_FAILURE);
    }

    status = setup_devices(options.persistence_mode);
    if (status != NVPD_SUCCESS) {
        shutdown_daemon(EXIT_FAILURE);
    }

    status = setup_rpc();
    if (status != NVPD_SUCCESS) {
        shutdown_daemon(EXIT_FAILURE);
    }

    svc_run();

    /* We should never return from svc_run() in a non-error scenario */
    syslog(LOG_ERR, "Failed to start local RPC service");
    shutdown_daemon(EXIT_FAILURE);

    return 0;
}
