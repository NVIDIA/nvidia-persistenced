#!/bin/sh
#
# NVIDIA Persistence Daemon Installer
#
# Copyright (c) 2013 NVIDIA Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#
# This is a sample installation script that attempts to create a UID for the
# NVIDIA Persistence Daemon and install one of the included sample scripts,
# which include:
#
#   + System V init (requires chkconfig found in PATH)
#   + systemd (requires systemctl found in PATH)
#   + Upstart (requires initctl found in PATH)
#
# This script is provided as a reference for what steps should be taken to
# correctly and safely install and enable the NVIDIA Persistence Daemon,
# and may not work for all init systems or distributions. Please tailor this
# script accordingly for the target system.

##############################################################################
# Common Functions
##############################################################################

# usage() - prints usage information of the script and exits
#
# $1 - return status code
usage()
{
    abs_path=$(cd `dirname "$0"` && pwd)

    printf "\nUsage: $0 [options] [sysv|systemd|upstart]\n"
    printf "where [options] may include:\n"
    printf "  -h\n"
    printf "    Prints this help message and exits.\n"
    printf "  -u <USER>\n"
    printf "    The user to create for the NVIDIA Persistence Daemon if\n"
    printf "    installing, or the user to delete if uninstalling. The default\n"
    printf "    value for this option is 'nvidia-persistenced'. If the user\n"
    printf "    already exists during installation, its primary group must\n"
    printf "    match that given by the -g option.\n"
    printf "  -g <GROUP>\n"
    printf "    The group that the created user will be added to. The default\n"
    printf "    value for this option is the value of the -u option. If the\n"
    printf "    value of this option is different than the -u option, the\n"
    printf "    group must already exist.\n"
    printf "  -p <PATH>\n"
    printf "    Directory to install the script or service files to if installing,\n"
    printf "    or the directory to uninstall the script or service files from if\n"
    printf "    uninstalling. Optional.\n"
    printf "\n"
    printf "  -r\n"
    printf "    Run the script to uninstall the daemon from an init system.\n"
    printf "    Optional.\n"
    printf "  -d\n"
    printf "    Do not delete the user during uninstall. Optional.\n"
    printf "\n"
    printf "If none of the sysv, systemd, or upstart arguments are given, the\n"
    printf "script will attempt to choose a single one which it believes is\n"
    printf "installed. The order in which it will search is systemd, upstart,\n"
    printf "sysv.\n"
    printf "\n"
    printf "Regardless of whether or not a supported init system is found, the\n"
    printf "script will still create functional sample scripts for each system\n"
    printf "that will run the daemon as the user specified by the '-u' option,\n"
    printf "or the default user if the '-u' option is not given. These scripts\n"
    printf "are created in the system-specific subdirectories of\n"
    printf "$abs_path.\n"
    printf "\n"
    printf "This script must be run with super-user privileges.\n"
    exit $1;
}

# cleanup_cmd is a sequence of commands that will be executed in the case
# that nvpdError is called. The purpose of these commands is to cleanup any
# work that has been performed.
cleanup_cmd=""

# nvpdError() - exits the script with an error message.
#
# $1 - the error message to print
nvpdError()
{
    printf "\nError: $1.\n" >&2
    printf "\n"
    printf "Aborting.\n"
    if [ "$uninstall" = "0" ]; then
        printf "Cleaning up... "
        errors=$( { eval $cleanup_cmd >/dev/null; } 2>&1 )
        if [ "$?" != "0" ]; then
            printf "failed:\n"
            printf "$errors\n" >&2
        else
            printf "done.\n"
        fi
    else
        printf "Uninstallation may be incomplete.\n"
    fi
    exit 1
}

# nvpdCommand() - wraps the given command with some formatting, and detects
# error conditions.
#
# $1 - a description about the command being executed
# $2 - the command to execute
# $3 - the cleanup command to reverse the effects of $2, should later commands
#      fail
nvpdCommand()
{
    printf "${1}... "
    errors=$( { eval $2 >/dev/null; } 2>&1 )
    [ "$?" = "0" ] || { printf "failed.\n"; \
        nvpdError "'$2' failed with\n'$errors'"; }
    printf "done.\n"

    # If there's a cleanup command associated with this command, push it onto
    # the front so the cleanup gets executed in reverse order.
    if [ -n "$3" ]; then
        if [ -n "$cleanup_cmd" ]; then
            cleanup_cmd="$3 && $cleanup_cmd"
        else
            cleanup_cmd="$3"
        fi
    fi
}

# checkInstallPath() - checks the given path and if it exists, assigns it to
# the potential_path variable.
#
# $1 - the path to check
#
# Returns 1 if the directory cannot be used as the install path, 0 otherwise.
checkInstallPath()
{
    printf "  $1 directory exists?  "
    if [ -d "$1" ]; then
        printf "Yes\n"
        potential_path=$1
        return 0
    else
        printf "No\n"
        return 1
    fi
}

pid_file="/var/run/nvidia-persistenced/nvidia-persistenced.pid"

# isNvpdRunning() - checks to see whether the daemon is already running.
isNvpdRunning()
{
    if [ -f "$pid_file" ]; then
        # PID file exists - is it running or stale?
        if ps -p `cat $pid_file` > /dev/null 2>&1; then
            # Process is running
            return 0
        else
            # Process info is stale
            return 1
        fi
    fi
    
    # No PID file exists, the daemon isn't running
    return 1
}

# checkCommon() - checks for requirements for installing in any environment.
# This function will exit the subshell if the requirements are not met.
# 
# This function takes no arguments.
#
# This function produces no outputs.
checkCommon()
{
    common_supported=1

    printf "\nChecking for common requirements...\n"
    printf "  sed found in PATH?  "
    $(which sed >/dev/null 2>&1)
    if [ "$?" = "0" ]; then
        printf "Yes\n"
    else
        printf "No\n"
        common_supported=0
    fi

    printf "  useradd found in PATH?  "
    $(which useradd >/dev/null 2>&1)
    if [ "$?" = "0" ]; then
        printf "Yes\n"
    else
        printf "No\n"
        common_supported=0
    fi

    printf "  userdel found in PATH?  "
    $(which userdel >/dev/null 2>&1)
    if [ "$?" = "0" ]; then
        printf "Yes\n"
    else
        printf "No\n"
        common_supported=0
    fi

    printf "  id found in PATH?  "
    $(which id >/dev/null 2>&1)
    if [ "$?" = "0" ]; then
        printf "Yes\n"
    else
        printf "No\n"
        common_supported=0
    fi

    if [ "$common_supported" = "1" ]; then
        printf "Common installation/uninstallation supported\n\n"
    else
        nvpdError "Common installation/uninstallation not supported"
    fi
}

# installCommon() - performs common installation steps required by all init
# systems, such as creating a user for the NVIDIA Persistence Daemon to run
# as.
#
# If the user already exists, this will skip the step of creating the user.
#
# $1 - username of the user to add
# $2 - groupname of the group to add the user to
installCommon()
{
    $(id $1 > /dev/null 2>&1)
    if [ "$?" -ne "0" ]; then
        group_opt=""
        if [ "$1" != "$2" ]; then
            group_opt="-g $2"
        fi
        nvpdCommand "Adding user '$1' to group '$2'" \
                    "useradd $group_opt -s /sbin/nologin -d '/' -c 'NVIDIA Persistence Daemon' -r $1" \
                    "userdel -f $1"
    else
        printf "User '$1' already exists, skipping useradd...\n"
        primary_group="$(id -ng $1 2>&1)"
        $(echo "$primary_group" | grep "$2" >/dev/null 2>&1)
        if [ "$?" = "0" ]; then
            printf "User '$1' is in primary group '$2'.\n"
        else
            nvpdError "User '$1' is not in primary group '$2'"
        fi
    fi
}

# uninstallCommon() - performs common uninstallation steps required by all
# init systems, such as deleting the user created for the NVIDIA Persistence
# Daemon.
#
# $1 - username of the user to delete, or empty to skip user deletion
uninstallCommon()
{
    if [ "$1" != "" ]; then
        nvpdCommand "Deleting user '$1'" \
                    "userdel -f $1" ""
    fi
}

##############################################################################
# SysV Functions
##############################################################################

sysv_script="nvidia-persistenced"

# createSysVScript() - fills in the SysV script template with the specified
# user.
#
# $1 - user to run the daemon as
createSysVScript()
{
    [ -f sysv/${sysv_script}.template ] || \
        nvpdError "sysv/${sysv_script}.template does not exist"

    if [ -f "sysv/$sysv_script" ]; then
        nvpdCommand "Removing previous sample System V script" \
                    "rm -f 'sysv/$sysv_script'" ""
    fi

    nvpdCommand "Creating sample System V script" \
                "sed -e 's/__USER__/$1/g' sysv/${sysv_script}.template > sysv/$sysv_script" \
                ""
}

# checkSysV() - checks for the requirements for installing in a SysV
# environment, and sets the associated configuration paths accordingly.
#
# $1 - the installation path to use instead of checking for defaults
#
# $sysv_supported = 1 if the script can install in a SysV environment
# $potential_path = the path to install the SysV init script to
checkSysV()
{
    sysv_supported=1

    printf "\nChecking for SysV requirements...\n"
    if [ -n "$1" ]; then
        checkInstallPath $1 || sysv_supported=0
    else
        checkInstallPath "/etc/init.d" || \
        checkInstallPath "/etc/rc.d/init.d" || \
        checkInstallPath "/etc/rc.d" || \
        sysv_supported=0
    fi

    printf "  chkconfig found in PATH?  "
    $(which chkconfig >/dev/null 2>&1)
    if [ "$?" = "0" ]; then
        printf "Yes\n"
    else
        printf "No\n"
        sysv_supported=0
    fi

    if [ "$sysv_supported" = "1" ]; then
        printf "SysV installation/uninstallation supported\n"
    else
        printf "SysV installation/uninstallation not supported\n"
    fi
}

# installSysVScript() - performs installation steps required to install the
# SysV init script.
#
# $1 - directory to install the init script in
installSysVScript()
{
    if isNvpdRunning; then
        if [ -f "$1/$sysv_script" ]; then
            # we can use the init script to stop it
            nvpdCommand "Attempting to stop $sysv_script" \
                        "'$1/$sysv_script' stop" \
                        ""
        else
            # something else is running the daemon, kill it
            nvpdCommand "Killing nvidia-persistenced" \
                        "kill -9 `cat $pid_file`" \
                        ""
        fi
    fi

    if [ -f "$1/$sysv_script" ]; then
        nvpdCommand "Backing up existing '$1/$sysv_script'" \
                    "mv '$1/$sysv_script' '$1/${sysv_script}.bk'" \
                    "mv '$1/${sysv_script}.bk' '$1/$sysv_script'"
    fi

    nvpdCommand "Installing sample System V script $sysv_script" \
                "cp 'sysv/$sysv_script' '$1/$sysv_script'" \
                "rm -f '$1/$sysv_script'"
    nvpdCommand "Enabling $sysv_script" \
                "chmod 0755 '$1/$sysv_script' && chkconfig --level 2345 $sysv_script on" \
                "chkconfig --del $sysv_script"
    nvpdCommand "Starting $sysv_script" \
                "'$1/$sysv_script' start" \
                "'$1/$sysv_script' stop"
}

# uninstallSysVScript() - performs uninstallation steps required to
# uninstall the SysV int script.
#
# $1 - directory to uninstall the init script from
uninstallSysVScript()
{
    [ -f "$1/$sysv_script" ] || nvpdError "'$1/$sysv_script' does not exist"

    nvpdCommand "Stopping $sysv_script" \
                "$1/$sysv_script stop" ""
    nvpdCommand "Disabling $sysv_script" \
                "chkconfig --del $sysv_script" ""
    nvpdCommand "Uninstalling $sysv_script script" \
                "rm -f '$1/$sysv_script'" ""
}

##############################################################################
# systemd Functions
##############################################################################

systemd_service="nvidia-persistenced.service"

# createSystemdService() - fills in the systemd service template with the
# specified user.
#
# $1 - user to run the daemon as
createSystemdService()
{
    [ -f systemd/${systemd_service}.template ] || \
        nvpdError "systemd/${systemd_service}.template does not exist"

    if [ -f "systemd/$systemd_service" ]; then
        nvpdCommand "Removing previous sample systemd service file" \
                    "rm -f 'systemd/$systemd_service'" ""
    fi

    nvpdCommand "Creating sample systemd service file" \
                "sed -e 's/__USER__/$1/g' systemd/${systemd_service}.template > systemd/$systemd_service" \
                ""
}

# checkSystemd() - checks for the requirements for installing in a systemd
# environment, and sets the associated configuration paths accordingly.
#
# $1 - the installation path to use instead of checking for defaults
#
# $systemd_supported = 1 if the script can install in a systemd environment
# $potential_path = the path to install the systemd service file to
checkSystemd()
{
    systemd_supported=1

    printf "\nChecking for systemd requirements...\n"
    if [ -n "$1" ]; then
        checkInstallPath $1 || systemd_supported=0
    else
        checkInstallPath "/usr/lib/systemd/system" || \
        checkInstallPath "/etc/systemd/system" || \
        systemd_supported=0
    fi

    printf "  systemctl found in PATH?  "
    $(which systemctl >/dev/null 2>&1)
    if [ "$?" = "0" ]; then
        printf "Yes\n"
    else
        printf "No\n"
        systemd_supported=0
    fi

    if [ "$systemd_supported" = "1" ]; then
        printf "systemd installation/uninstallation supported\n"
    else
        printf "systemd installation/uninstallation not supported\n"
    fi
}

# installSystemdService() - performs installation steps required to install
# the systemd service.
#
# $1 - directory to install the service file in
installSystemdService()
{
    if isNvpdRunning; then
        systemctl status $systemd_service > /dev/null 2>&1
        if [ "$?" = "0" ]; then
            # systemd is running the daemon, stop it
            nvpdCommand "Stopping $systemd_service" \
                        "systemctl stop $systemd_service" \
                        ""
        else
            # something else is running the daemon, kill it
            nvpdCommand "Killing nvidia-persistenced" \
                        "kill -9 `cat $pid_file`" \
                        ""
        fi
    fi

    if [ -f "$1/$systemd_service" ]; then
        nvpdCommand "Backing up existing '$1/$systemd_service'" \
                    "mv '$1/$systemd_service' '$1/${systemd_service}.bk'" \
                    "mv '$1/${systemd_service}.bk' '$1/$systemd_service'"
    fi

    nvpdCommand "Installing sample systemd service $systemd_service" \
                "cp 'systemd/$systemd_service' '$1/$systemd_service'" \
                "rm -f '$1/$systemd_service'"
    nvpdCommand "Enabling $systemd_service" \
                "systemctl reenable $systemd_service" \
                "systemctl disable $systemd_service"
    nvpdCommand "Starting $systemd_service" \
                "systemctl start $systemd_service" \
                "systemctl stop $systemd_service"
}

# uninstallSystemdService() - performs uninstallation steps required to
# uninstall the systemd service.
#
# $1 - directory to uninstall the service file from
uninstallSystemdService()
{
    [ -f "$1/$systemd_service" ] || nvpdError "'$1/$systemd_service' does not exist"

    nvpdCommand "Stopping $systemd_service" \
                "systemctl stop $systemd_service" ""
    nvpdCommand "Disabling $systemd_service" \
                "systemctl disable $systemd_service" ""
    nvpdCommand "Uninstalling $systemd_service" \
                "rm -f '$1/$systemd_service'" ""
}

##############################################################################
# Upstart Functions
##############################################################################

upstart_service="nvidia-persistenced.conf"

# createUpstartService() - fills in the upstart service template with the
# specified user.
#
# $1 - user to run the daemon as
createUpstartService()
{
    [ -f upstart/${upstart_service}.template ] || \
        nvpdError "upstart/${upstart_service}.template does not exist"

    if [ -f "upstart/$upstart_service" ]; then
        nvpdCommand "Removing previous sample Upstart service file" \
                    "rm -f 'upstart/$upstart_service'" ""
    fi

    nvpdCommand "Creating sample Upstart service file" \
                "sed -e 's/__USER__/$1/g' upstart/${upstart_service}.template > upstart/$upstart_service" \
                ""
}

# checkUpstart() - checks for the requirements for installing in an Upstart
# environment, and sets the associated configuration paths accordingly.
#
# $1 - the installation path to use instead of checking for defaults
#
# $upstart_supported = 1 if the script can install in an Upstart environment
# $potential_path = the path to install the Upstart service file to
checkUpstart()
{
    upstart_supported=1

    printf "\nChecking for Upstart requirements...\n"
    if [ -n "$1" ]; then
        checkInstallPath $1 || upstart_supported=0
    else
        checkInstallPath "/etc/init" || upstart_supported=0
    fi

    printf "  initctl found in PATH?  "
    $(which initctl >/dev/null 2>&1)
    if [ "$?" = "0" ]; then
        printf "Yes\n"
    else
        printf "No\n"
        upstart_supported=0
    fi

    if [ "$upstart_supported" = "1" ]; then
        printf "Upstart installation/uninstallation supported\n"
    else
        printf "Upstart installation/uninstallation not supported\n"
    fi
}

# installUpstartService() - performs installation steps required to install
# the Upstart service.
#
# $1 - directory to install the service file in
installUpstartService()
{
    if isNvpdRunning; then
        initctl status nvidia-persistenced | grep "start" > /dev/null 2>&1
        if [ "$?" = "0" ]; then
            # Upstart is running the service, attempt to stop it
            nvpdCommand "Stopping $upstart_service" \
                        "initctl stop nvidia-persistenced" \
                        ""
        else
            # something else is running the daemon, kill it
            nvpdCommand "Killing nvidia-persistenced" \
                        "kill -9 `cat $pid_file`" \
                        ""
        fi
    fi

    if [ -f "$1/$upstart_service" ]; then
        nvpdCommand "Backing up existing '$1/$upstart_service'" \
                    "mv '$1/$upstart_service' '$1/${upstart_service}.bk'" \
                    "mv '$1/${upstart_service}.bk' '$1/$upstart_service'"
    fi

    nvpdCommand "Installing $upstart_service" \
                "cp 'upstart/$upstart_service' '$1/$upstart_service'" \
                "rm -f '$1/$upstart_service'"
    nvpdCommand "Starting $upstart_service" \
                "initctl start nvidia-persistenced" \
                "initctl stop nvidia-persistenced"
}

# uninstallUpstartService() - performs uninstallation steps required to
# uninstall the Upstart service.
#
# $1 - directory to uninstall the service file from
uninstallUpstartService()
{
    [ -f "$1/$upstart_service" ] || nvpdError "'$1/$upstart_service' does not exist"

    nvpdCommand "Stopping $upstart_service" \
                "initctl stop nvidia-persistenced" ""
    nvpdCommand "Uninstalling $upstart_service" \
                "rm -f '$1/$upstart_service'" ""
}

##############################################################################
# main script
##############################################################################

# make sure we execute in the script directory
cd $(dirname $0)

args=$(getopt -o hdu:g:p:r -n $0 -- "$@")

if [ "$?" != "0" ]; then
    usage 1
fi

eval set -- "$args"

# defaults
nvpd_user=nvidia-persistenced
nvpd_group=""
install_path=""
uninstall=0

while [ 1 ]; do
    case "$1" in
        -h)
            shift;
            usage 0
            ;;
        -u)
            shift;
            # Set the target username, unless it's already been overridden by
            # the -d option
            if [ "$nvpd_user" != "" ]; then
                nvpd_user=$1
            fi
            shift;
            ;;
        -g)
            shift;
            nvpd_group=$1
            shift;
            ;;
        -d)
            shift;
            nvpd_user=""
            ;;
        -p)
            shift;
            install_path=$1
            shift;
            ;;
        -r)
            shift;
            uninstall=1
            ;;
        --)
            shift;
            break;
            ;;
        *)
            nvpdError "invalid option '$1'"
            ;;
    esac
done

# If no group was specified, default to the user
if [ "$nvpd_group" = "" ]; then
    nvpd_group="$nvpd_user"
fi

# Make sure we check common dependencies before doing anything else
checkCommon

# Since we overload nvpd_user using the -d option, we can't use it during
# installation (it wouldn't make sense to, anyway).
if [ "$uninstall" = "0" ]; then
    if [ "$nvpd_user" = "" ]; then
        nvpdError "the -d option cannot be used during installation"
    fi

    # Fill in the template files with the specified user
    createSysVScript "$nvpd_user"
    createSystemdService "$nvpd_user"
    createUpstartService "$nvpd_user"
fi

# Select which targets to attempt
targets="systemd upstart sysv"
if [ -n "$1" ]; then
    case "$1" in
        systemd)
            printf "Only systemd will be checked.\n"
            targets="systemd"
            ;;
        upstart)
            printf "Only Upstart will be checked.\n"
            targets="upstart"
            ;;
        sysv)
            printf "Only SysV will be checked.\n"
            targets="sysv"
            ;;
        *)
            nvpdError "invalid argument '$1'"
            ;;
    esac
fi

eval set -- "$targets"
supported=0
target="unknown"
while true; do
    case "$1" in
        systemd)
            target="systemd service"
            checkSystemd $install_path
            [ "$systemd_supported" = "1" ] && { supported=1; break; }
            ;;
        upstart)
            target="Upstart service"
            checkUpstart $install_path
            [ "$upstart_supported" = "1" ] && { supported=1; break; }
            ;;
        sysv)
            target="SysV init script"
            checkSysV $install_path
            [ "$sysv_supported" = "1" ] && { supported=1; break; }
            ;;
        *)
            break
            ;;
    esac
    shift
done

[ "$supported" = "1" ] || nvpdError "No supported init system found"

# The last call to checkInstallPath that succeeded set the $potential_path
# for the supported system.
install_path=$potential_path

# Run through the install/uninstall steps now
if [ "$uninstall" = "0" ]; then
    printf "\n"
    printf "Installation parameters:\n"
    printf "  User  : $nvpd_user\n"
    printf "  Group : $nvpd_group\n"
    printf "  $target installation path : $install_path\n"
    printf "\n"
    installCommon "$nvpd_user" "$nvpd_group"
    case "$1" in
        systemd)
            installSystemdService $install_path
            ;;
        upstart)
            installUpstartService $install_path
            ;;
        sysv)
            installSysVScript $install_path
            ;;
        *)
            nvpdError "Unknown installation target '$1'"
            ;;
    esac

    printf "\n$target successfully installed.\n"
else
    printf "\n"
    printf "Uninstallation parameters:\n"
    printf "  User to remove : "
    if [ -n "$nvpd_user" ]; then
        printf "$nvpd_user\n"
    else
        printf "<none>\n"
    fi
    printf "  Path to remove $target from : $install_path\n"
    printf "\n"
    case "$1" in
        systemd)
            uninstallSystemdService $install_path
            ;;
        upstart)
            uninstallUpstartService $install_path
            ;;
        sysv)
            uninstallSysVScript $install_path
            ;;
        *)
            nvpdError "Unknown uninstallation target '$1'"
            ;;
    esac
    uninstallCommon "$nvpd_user"

    printf "\n$target successfully uninstalled.\n"
fi
