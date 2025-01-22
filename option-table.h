/*
 * nvidia-persistenced: A daemon for maintaining persistent driver state,
 * specifically for use by the NVIDIA Linux driver.
 *
 * Copyright (C) 2013-2016 NVIDIA Corporation
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
 * option-table.h
 */

#include "nvgetopt.h"

enum {
    PERSISTENCE_MODE_OPTION = 1024,
    NVIDIA_CFG_PATH_OPTION,
    UVM_PERSISTENCE_MODE_OPTION,
};

static const NVGetoptOption __options[] = {

    { "version",
      'v',
      NVGETOPT_HELP_ALWAYS,
      NULL,
      "Print the utility version and exit." },

    { "help",
      'h',
      NVGETOPT_HELP_ALWAYS,
      NULL,
      "Print usage information for the command line options and exit." },

    { "verbose",
      'V',
      NVGETOPT_HELP_ALWAYS,
      NULL,
      "Controls how much information is printed. By default, "
      "nvidia-persistenced will only print errors and warnings to syslog for "
      "unexpected events, as well as startup and shutdown notices. "
      "Specifying this flag will cause nvidia-persistenced to also print "
      "notices to syslog on state transitions, such as when persistence mode "
      "is enabled or disabled, and informational messages on startup and "
      "exit." },

    { "user",
      'u',
      NVGETOPT_STRING_ARGUMENT | NVGETOPT_HELP_ALWAYS,
      "USERNAME",
      "Runs nvidia-persistenced with the user permissions of the user "
      "specified by the &USERNAME& argument. This user must have write access "
      "to the /var/run/nvidia-persistenced directory. If this directory does "
      "not exist, nvidia-persistenced will attempt to create it prior to "
      "changing the process user and group IDs. "
      "If this option is not given, nvidia-persistenced will not attempt "
      "to change the process user ID." },

    { "group",
      'g',
      NVGETOPT_STRING_ARGUMENT | NVGETOPT_HELP_ALWAYS,
      "GROUPNAME",
      "Runs nvidia-persistenced with the group permissions of the group "
      "specified by the &GROUPNAME& argument. "
      "If both this option and the --user option are given, this option "
      "will take precedence when determining the group ID to use. "
      "If this option is not given, nvidia-persistenced will use the "
      "primary group ID of the user specified by the --user option argument. "
      "If the --user option is also not given, nvidia-persistenced will not "
      "attempt to change the process group ID." },

    { "persistence-mode",
      PERSISTENCE_MODE_OPTION,
      NVGETOPT_IS_BOOLEAN | NVGETOPT_HELP_ALWAYS,
      NULL,
      "By default, nvidia-persistenced starts with persistence mode enabled "
      "for all devices. Use '--no-persistence-mode' to force persistence mode "
      "off for all devices on startup." },

    { "uvm-persistence-mode",
      UVM_PERSISTENCE_MODE_OPTION,
      NVGETOPT_IS_BOOLEAN | NVGETOPT_HELP_ALWAYS,
      NULL,
      "UVM persistence mode is only supported on the single GPU confidential "
      "computing configuration. By default, nvidia-persistenced starts with UVM "
      "persistence mode disabled for all devices. Use '--uvm-persistence-mode' "
      "to force UVM persistence mode on for supported devices on startup." },

    { "nvidia-cfg-path",
      NVIDIA_CFG_PATH_OPTION,
      NVGETOPT_STRING_ARGUMENT | NVGETOPT_HELP_ALWAYS,
      "PATH",
      "The nvidia-cfg library is used to communicate with the NVIDIA kernel "
      "module to query and manage GPUs in the system. This library is "
      "required by nvidia-persistenced. This option tells "
      "nvidia-persistenced where to look for this library (in case it cannot "
      "find it on its own). This option should normally not be needed." },

    { "foreground",
      'f',
      NVGETOPT_HELP_ALWAYS,
      NULL,
      "Runs the process in the foreground instead of detaching. This is "
      "useful for init systems that can't natively supervise processes "
      "that detach themselves (e.g. runit)."},

    { NULL, 0, 0, NULL, NULL },
};
