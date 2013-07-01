dnl This file is to be preprocessed by m4.
changequote([[[, ]]])dnl
define(__OPTIONS__, [[[include([[[options.1.inc]]])dnl]]])dnl
dnl Solaris man chokes on three-letter macros.
ifelse(__BUILD_OS__,SunOS,[[[define(__URL__,UR)]]],[[[define(__URL__,URL)]]])dnl
.\" Copyright (C) 2013 NVIDIA Corporation.
.\"
__HEADER__
.\" Define the .__URL__ macro and then override it with the www.tmac package if it
.\" exists.
.de __URL__
\\$2 \(la \\$1 \(ra\\$3
..
.if \n[.g] .mso www.tmac
.TH nvidia\-persistenced 1 "__DATE__" "nvidia\-persistenced __VERSION__"

.SH NAME
nvidia\-persistenced \- A daemon to maintain persistent software state in the NVIDIA driver.

.SH SYNOPSIS
.BI "nvidia\-persistenced"
.br

.SH DESCRIPTION
The
.B nvidia\-persistenced
utility is used to enable persistent software state in the NVIDIA driver.
When persistence mode is enabled, the daemon prevents the driver from releasing device state when the device is not in use.
This can improve the startup time of new clients in this scenario.
.PP

__OPTIONS__

.SH NOTES
When installed by
.B nvidia\-installer
, sample init scripts to start the daemon for some of the more prevalent init systems are installed as the compressed tarball /usr/share/doc/NVIDIA_GLX-1.0/sample/nvidia-persistenced-init.tar.bz2.
These init scripts should be customized to the user's distribution and installed in the proper location by the user to run 
.B nvidia\-persistenced
on system initialization.
.PP
Once the init script is installed so that the daemon is running, users should not normally need to manually interact with
.B nvidia\-persistenced:
the NVIDIA management utilities, such as
.B nvidia\-smi,
can communicate with it automatically as necessary to manage persistence mode.
.PP
The daemon does not require root privileges to run, and may safely be run as an unprivileged user, given that its runtime directory, /var/run/nvidia-persistenced, is created for and owned by that user prior to starting the daemon.
.B nvidia\-persistenced
also requires read and write access to the NVIDIA character device files.
If the permissions of the device files have been altered through any of the NVreg_DeviceFileUID, NVreg_DeviceFile_GID, or NVreg_DeviceFileMode NVIDIA kernel module options,
.B nvidia\-persistenced
will need to run as a suitable user.
.PP
If the daemon is started with root privileges, the
.B \-\-user
option may be used instead to indicate that the daemon should drop its privileges and run as the specified user after setting up its runtime directory.
Using this option may cause the daemon to be unable to remove the /var/run/nvidia-persistenced directory when it is killed, if the specified user does not have write permissions to /var/run.
In this case, directory removal should be handled by a post-execution script.
See the sample init scripts provided in /usr/share/doc/NVIDIA_GLX-1.0/sample/nvidia-persistenced-init.tar.bz2 for examples of this behavior.
.PP
The daemon indirectly utilizes
.B nvidia\-modprobe
via the nvidia-cfg library to load the NVIDIA kernel module and create the NVIDIA character device files after the daemon has dropped its root privileges, if it had any to begin with.
If
.B nvidia\-modprobe
is not installed, the daemon may not be able to start properly if it is not run with root privileges.
.PP
The source code to
.B nvidia\-persistenced
is released under the MIT license and is available here:
.__URL__ ftp://download.nvidia.com/XFree86/nvidia-persistenced/
.PP

.SH EXAMPLES
.TP
.B nvidia\-persistenced
Starts the NVIDIA Persistence Daemon with persistence mode disabled for all NVIDIA devices.
.TP
.B nvidia\-persistenced \-\-persistence-mode
Starts the NVIDIA Persistence Daemon with persistence mode enabled for all NVIDIA devices.
.TP
.B nvidia\-persistenced \-\-user=foo
Starts the NVIDIA Persistence Daemon so that it will run as user 'foo'.

.SH AUTHOR
Will Davis
.br
NVIDIA Corporation

.SH SEE ALSO
.BR nvidia\-smi (1),
.BR nvidia\-modprobe (1)

.SH COPYRIGHT
Copyright \(co 2013 NVIDIA Corporation.
