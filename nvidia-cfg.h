/*
 * nvidia-cfg
 *
 * Copyright (c) 2004  NVIDIA Corp.  All rights reserved.
 *
 * NOTICE TO USER:   The source code  is copyrighted under  U.S. and
 * international laws. NVIDIA, Corp. of Santa Clara, California owns
 * the copyright  and as design patents  pending  on the design  and
 * interface  of the NV chips.   Users and possessors of this source
 * code are hereby granted  a nonexclusive,  royalty-free  copyright
 * and  design  patent license  to use this code  in individual  and
 * commercial software.
 *
 * Any use of this source code must include,  in the user documenta-
 * tion and  internal comments to the code,  notices to the end user
 * as follows:
 *
 * Copyright (c) 2004 NVIDIA Corp.  NVIDIA design patents pending in
 * the U.S. and foreign countries.
 *
 * NVIDIA CORP.  MAKES  NO REPRESENTATION  ABOUT  THE SUITABILITY OF
 * THIS SOURCE CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT
 * EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  NVIDIA, CORP. DISCLAIMS
 * ALL WARRANTIES  WITH REGARD  TO THIS SOURCE CODE,  INCLUDING  ALL
 * IMPLIED   WARRANTIES  OF  MERCHANTABILITY  AND   FITNESS   FOR  A
 * PARTICULAR  PURPOSE.   IN NO EVENT SHALL NVIDIA, CORP.  BE LIABLE
 * FOR ANY SPECIAL, INDIRECT, INCIDENTAL,  OR CONSEQUENTIAL DAMAGES,
 * OR ANY DAMAGES  WHATSOEVER  RESULTING  FROM LOSS OF USE,  DATA OR
 * PROFITS,  WHETHER IN AN ACTION  OF CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT  OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOURCE CODE.
 *
 *
 * This header file defines the public interface to the
 * libnvidia-cfg.so library.
 */

#ifndef __NVIDIA_CFG_H__
#define __NVIDIA_CFG_H__



/*
 * NvCfgDevice - data structure containing bus:slot pairs
 *
 * This is deprecated; please use NvCfgPciDevice instead
 */

typedef struct {
    int bus;
    int slot;
} NvCfgDevice;


/*
 * NvCfgPciDevice - data structure identifying a device on the PCI bus
 */

typedef struct {
    int domain;
    int bus;
    int slot;
    int function;
} NvCfgPciDevice;


/*
 * NvCfgGSyncDeviceType - type of the GSync device
 */

typedef enum {
    NVCFG_TYPE_GSYNC2 = 1,
    NVCFG_TYPE_GSYNC3,
    NVCFG_TYPE_GSYNC4
} NvCfgGSyncDeviceType;



#define NV_CFG_GSYNC_DEVICE_FIRMWARE_FORMAT_1 1



/*
 * NvCfgDisplayDeviceInformation - this data structure contains
 * various limits and other useful data parsed from the EDID.
 */

typedef struct {

    /*
     * The monitor name is the name of the monitor as specified by an
     * EDID 1.x Monitor Descriptors, or an EDID 2.x
     * Manufacturer/Product ID string.
     */

    char monitor_name[64];

    /*
     * The horiz_sync and vert_refresh ranges are retrieved from an
     * EDID 1.x Monitor Descriptor, or an EDID 2.x Range Limit.
     */

    unsigned int min_horiz_sync;    /* in Hz */
    unsigned int max_horiz_sync;    /* in Hz */
    unsigned int min_vert_refresh;  /* in Hz */
    unsigned int max_vert_refresh;  /* in Hz */

    unsigned int max_pixel_clock;   /* in kHz */

    /*
     * The max xres, yres, and refresh, if not 0, are taken from the
     * largest mode in the EDID.
     */

    unsigned int max_xres;          /* in pixels */
    unsigned int max_yres;          /* in pixels */
    unsigned int max_refresh;       /* in Hz */

    /*
     * the preferred xres, yres, and refresh, if not 0, are the values
     * specified by the EDID as the preferred timing mode of the
     * display device.
     */

    unsigned int preferred_xres;    /* in pixels */
    unsigned int preferred_yres;    /* in pixels */
    unsigned int preferred_refresh; /* in Hz */

    /*
     * the physical width and height, if not 0, are the physical
     * dimensions of the display device.
     */

    unsigned int physical_width;    /* in mm */
    unsigned int physical_height;   /* in mm */

} NvCfgDisplayDeviceInformation;



/*
 * NvCfgDeviceHandle - this is an opaque handle identifying a
 * connection to an NVIDIA VGA adapter.
 */

typedef void * NvCfgDeviceHandle;



/*
 * NvCfgGSyncHandle - this is an opaque handle identifying a
 * GSync device.
 */

typedef void * NvCfgGSyncHandle;



/*
 * NvCfg Boolean values
 */

typedef enum {
    NVCFG_TRUE = 1,
    NVCFG_FALSE = 0,
} NvCfgBool;



/*
 * nvCfgGetDevices() - retrieve an array of NvCfgDevice's indicating
 * what PCI devices are present on the system.  On success, NVCFG_TRUE
 * will be returned, n will contain the number of NVIDIA PCI VGA
 * adapters present in the system, and devs will be an allocated array
 * containing the bus address of each NVIDIA PCI VGA adapter.  When
 * the caller is done, it should free the devs array.  On failure,
 * NVCFG_FALSE will be returned.
 *
 * This is deprecated; please use nvCfgGetPciDevices() instead.
 */

NvCfgBool nvCfgGetDevices(int *n, NvCfgDevice **devs);



/*
 * nvCfgGetPciDevices() - retrieve an array of NvCfgPciDevice's
 * indicating what PCI devices are present on the system.  On success,
 * NVCFG_TRUE will be returned, n will contain the number of NVIDIA
 * PCI graphics devices present in the system, and devs will be an
 * allocated array containing the PCI domain:bus:slot:function
 * address of each NVIDIA PCI graphics device.  When the caller is
 * done, it should free the devs array.  On failure, NVCFG_FALSE will
 * be returned.
 */

NvCfgBool nvCfgGetPciDevices(int *n, NvCfgPciDevice **devs);



/*
 * nvCfgOpenDevice() - open a connection to the NVIDIA device
 * identified by the bus:slot PCI address.  On success, NVCFG_TRUE
 * will be returned and handle be assigned.  On failure, NVCFG_FALSE
 * will be returned.
 *
 * This is deprecated; please use nvCfgOpenPciDevice() instead.
 */

NvCfgBool nvCfgOpenDevice(int bus, int slot, NvCfgDeviceHandle *handle);



/*
 * nvCfgAttachPciDevice() - open a limited, display-less connection to
 * the NVIDIA device identified by the domain:bus:slot:function PCI
 * address.  On success, NVCFG_TRUE will be returned and handle will be
 * assigned.  On failure, NVCFG_FALSE will be returned.
 */
NvCfgBool nvCfgAttachPciDevice(int domain, int bus, int device, int function,
                               NvCfgDeviceHandle *handle);



/*
 * nvCfgOpenPciDevice() - open a connection to the NVIDIA device
 * identified by the domain:bus:slot:function PCI address.  On
 * success, NVCFG_TRUE will be returned and handle will be assigned.
 * On failure, NVCFG_FALSE will be returned.
 */

NvCfgBool nvCfgOpenPciDevice(int domain, int bus, int device, int function,
                             NvCfgDeviceHandle *handle);



/*
 * nvCfgOpenAllPciDevices() - open a connection to each NVIDIA device
 * in the system.  On success, NVCFG_TRUE will be returned, n will be
 * assigned the number of NVIDIA devices in the system, and handles
 * will be assigned with an allocated array of NvCfgDeviceHandles;
 * each element in the array is a handle to one of the NVIDIA devices
 * in the system.  The caller should free the handles array when no
 * longer needed.  On failure, NVCFG_FALSE will be returned.
 */

NvCfgBool nvCfgOpenAllPciDevices(int *n, NvCfgDeviceHandle **handles);



/*
 * nvCfgDetachDevice() - close the previously opened limited, display-less
 * connection to an NVIDIA device created by nvCfgAttachPciDevice().
 */
NvCfgBool nvCfgDetachDevice(NvCfgDeviceHandle handle);



/*
 * nvCfgCloseDevice() - close the previously opened connection to an
 * NVIDIA device created by nvCfgOpenPciDevice().
 */

NvCfgBool nvCfgCloseDevice(NvCfgDeviceHandle handle);



/*
 * nvCfgCloseAllPciDevices() - close all the NVIDIA device connections
 * opened by a previous call to nvCfgOpenAllPciDevices().
 */

NvCfgBool nvCfgCloseAllPciDevices(void);



/*
 * nvCfgGetNumCRTCs() - return the number of CRTCs (aka "heads")
 * present on the specified NVIDIA device.  On success, NVCFG_TRUE
 * will be returned and crtcs will be assigned.  On failure,
 * NVCFG_FALSE will be returned.
 */

NvCfgBool nvCfgGetNumCRTCs(NvCfgDeviceHandle handle, int *crtcs);



/*
 * nvCfgGetProductName() - return an allocated string containing the
 * product name of the specified NVIDIA device.  It is the caller's
 * responsibility to free the returned string.  On success, NVCFG_TRUE
 * will be returned and name will be assigned.  On failure,
 * NVCFG_FALSE will be returned.
 */

NvCfgBool nvCfgGetProductName(NvCfgDeviceHandle handle, char **name);



/*
 * nvCfgGetDeviceUUID() - return an allocated string containing the
 * global unique identifier of the specified NVIDIA device.  It is the caller's
 * responsibility to free the returned string.  On success, NVCFG_TRUE
 * will be returned and uuid will be assigned.  On failure,
 * NVCFG_FALSE will be returned.
 */

NvCfgBool nvCfgGetDeviceUUID(NvCfgDeviceHandle handle, char **uuid);



/*
 * nvCfgGetDisplayDevices() - retrieve a bitmask describing the
 * currently connected display devices: this "display device mask" is
 * an unsigned 32 bit value that identifies one or more display
 * devices.  The first 8 bits each identify a CRT, the next 8 bits
 * each identify a TV, and the next 8 each identify a DFP.  For
 * example, 0x1 refers to CRT-0, 0x3 refers to CRT-0 and CRT-1,
 * 0x10001 refers to CRT-0 and DFP-0, etc.  On success, NVCFG_TRUE
 * will be returned and display_device_mask will be assigned.  On
 * failure, NVCFG_FALSE will be returned.
 */

NvCfgBool nvCfgGetDisplayDevices(NvCfgDeviceHandle handle,
                                 unsigned int *display_device_mask);



/* nvCfgGetSupportedDisplayDevices() - get all supported display devices,
 * not only connected ones. Interpretation of display_device_mask
 * parameter is the same as for nvCfgGetDisplayDevices() call.
 * On success, NVCFG_TRUE will be returned and display_device_mask will be
 * assigned.  On failure, NVCFG_FALSE will be returned.
 */

NvCfgBool nvCfgGetSupportedDisplayDevices(NvCfgDeviceHandle handle,
                                 unsigned int *display_device_mask);


/*
 * nvCfgGetEDIDData() - return an allocated byte array containing the
 * EDID for the specified display device, if any.  On success,
 * NVCFG_TRUE will be returned and edidSize and edid will be assigned.
 * On failure, NVCFG_FALSE will be returned.  It is the caller's
 * responsibility to free the allocated EDID.
 */

NvCfgBool nvCfgGetEDIDData(NvCfgDeviceHandle handle,
                           unsigned int display_device,
                           int *edidSize, void **edid);


/*
 * nvCfgGetEDIDMonitorData() - Initialize the fields in the
 * NvCfgDisplayDeviceInformation data structure, using data from the
 * EDID.  On success, NVCFG_TRUE will be returned and info will be
 * assigned.  On failure, NVCFG_FALSE will be returned.
 */

NvCfgBool nvCfgGetEDID(NvCfgDeviceHandle handle,
                       unsigned int display_device,
                       NvCfgDisplayDeviceInformation *info);


/*
 * nvCfgIsPrimaryDevice() - determines whether the specified NVIDIA
 * device is the primary device.  On success, NVCFG_TRUE will be
 * returned and is_primary_device set to indicate whether the
 * device is the primary device.  On failure, NVCFG_FALSE will be
 * returned.
 */

NvCfgBool nvCfgIsPrimaryDevice(NvCfgDeviceHandle handle,
                               NvCfgBool *is_primary_device);

/*
 * nvCfgGetTeslaSerialNumbers() - returns an allocated array of strings 
 * containing the serial numbers of all NVIDIA Tesla/QuadroPlex devices
 * connected to the host, followed by a NULL character. It is the caller's
 * responsibility to free the returned array of strings. On success, 
 * NVCFG_TRUE will be returned and serials will be assigned.  On failure,
 * NVCFG_FALSE will be returned.
 *
 * Note that this function is deprecated and will always return an empty array
 * on recent drivers, since QuadroPlex devices are no longer supported.
 */

NvCfgBool nvCfgGetTeslaSerialNumbers(char ***serials);


/*
 * nvCfgOpenAllGSyncDevices() - returns an array of NvCfgGSyncHandle's
 * indicating what GSync devices are present in the system. On success,
 * NVCFG_TRUE will be returned, n will contain the number of GSync devices
 * present in the system, and handles will be an allocated array containing
 * a handle for each of the GSync devices.The caller should free the
 * handles array when no longer needed. On failure, NVCFG_FALSE will be
 * returned.
 */

NvCfgBool nvCfgOpenAllGSyncDevices(int *n, NvCfgGSyncHandle **handles);


/*
 * nvCfgCloseAllGSyncDevices() - close all the GSync device connections
 * opened by a previous call to nvCfgOpenAllGSyncDevices().
 */

NvCfgBool nvCfgCloseAllGSyncDevices(void);


/*
 * nvCfgGetGSyncDeviceType() - returns the type of GSync device referenced
 * by handle.
 */

NvCfgGSyncDeviceType nvCfgGetGSyncDeviceType(NvCfgGSyncHandle handle);


/*
 * nvCfgGetGSyncDeviceFirmwareVersion() - returns the firmware major version of
 * the GSync device referenced by handle.
 */

int nvCfgGetGSyncDeviceFirmwareVersion(NvCfgGSyncHandle handle);


/*
 * nvCfgGetGSyncDeviceFirmwareMinorVersion() - returns the firmware minor
 * version of the GSync device referenced by handle.
 */

int nvCfgGetGSyncDeviceFirmwareMinorVersion(NvCfgGSyncHandle handle);


/*
 * nvCfgFlashGSyncDevice() - flashes the GSync device referenced by handle.
 * format contains the firmware format, newFirmwareImage contains the
 * new firmware image to be flashed, and size contains the size of
 * newFirmwareImage. On success, NVCFG_TRUE will be returned.
 * On failure, NVCFG_FALSE will be returned.
 */

NvCfgBool nvCfgFlashGSyncDevice(NvCfgGSyncHandle handle, int format,
                                const unsigned char *newFirmwareImage,
                                int size);


/*
 * nvCfgDumpDisplayPortAuxLog() - dump the DisplayPort AUX channel log to the
 * system log. On success, NVCFG_TRUE will be returned. On failure, NVCFG_FALSE
 * will be returned.
 */
NvCfgBool nvCfgDumpDisplayPortAuxLog(NvCfgDeviceHandle handle);

#endif /* __NVIDIA_CFG__ */
