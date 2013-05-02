# nvidia-persistenced: A daemon for maintaining persistent driver state,
# specifically for use by the NVIDIA Linux driver.
#
# Copyright (C) 2013 NVIDIA Corporation
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

##############################################################################
# define the list of files that should be distributed in the
# nvidia-persistenced tarball
##############################################################################

RPC_DIR ?= .

RPC_SRC := $(RPC_DIR)/nvpd_rpc_server.c
RPC_SRC += $(RPC_DIR)/nvpd_rpc_xdr.c

# Sources
SRC += command_server.c
SRC += nvidia-persistenced.c
SRC += options.c
SRC += $(RPC_SRC)

# Sample files included in the distribution
SAMPLE_FILES := init/README
SAMPLE_FILES += init/install.sh
SAMPLE_FILES += init/systemd/nvidia-persistenced.service.template
SAMPLE_FILES += init/sysv/nvidia-persistenced.template
SAMPLE_FILES += init/upstart/nvidia-persistenced.conf.template

# Other distributed files
DIST_FILES := $(SRC)
DIST_FILES += COPYING
DIST_FILES += README
DIST_FILES += dist-files.mk
DIST_FILES += nvpd_defs.h
DIST_FILES += nvidia-persistenced.h
DIST_FILES += option-table.h
DIST_FILES += nvidia-persistenced.1.m4
DIST_FILES += gen-manpage-opts.c
DIST_FILES += $(RPC_DIR)/nvpd_rpc.h
