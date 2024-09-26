#
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
# include common variables and functions
##############################################################################

include utils.mk

##############################################################################
# assign variables
##############################################################################

NVIDIA_PERSISTENCED = $(OUTPUTDIR)/nvidia-persistenced

NVIDIA_PERSISTENCED_PROGRAM_NAME = "nvidia-persistenced"

MANPAGE_GZIP ?= 1

MANPAGE_not_gzipped = $(OUTPUTDIR)/nvidia-persistenced.1
MANPAGE_gzipped     = $(OUTPUTDIR)/nvidia-persistenced.1.gz

ifeq ($(MANPAGE_GZIP),1)
  MANPAGE = $(MANPAGE_gzipped)
else
  MANPAGE = $(MANPAGE_not_gzipped)
endif
GEN_MANPAGE_OPTS = $(OUTPUTDIR_ABSOLUTE)/gen-manpage-opts
OPTIONS_1_INC    = $(OUTPUTDIR)/options.1.inc

##############################################################################
# The calling Makefile may export any of the following variables; we assign
# default values if they are not exported by the caller
##############################################################################

RPC_DIR ?= .
NVIDIA_CFG_DIR ?= .
NVIDIA_NUMA_DIR ?= .
NV_IOCTL_INC_DIR ?= .

##############################################################################
# The common-utils directory may be in one of two places: either elsewhere in
# the driver source tree when building nvidia-persistenced as part of the
# NVIDIA driver build (in which case, COMMON_UTILS_DIR should be defined by
# the calling makefile), or directly in the source directory when building
# from the nvidia-persistenced source tarball (in which case, the below
# conditional assignment should be used)
##############################################################################

COMMON_UTILS_DIR ?= common-utils

# include the list of source files; defines SRC
include dist-files.mk

include $(COMMON_UTILS_DIR)/src.mk
SRC += $(addprefix $(COMMON_UTILS_DIR)/,$(COMMON_UTILS_SRC))

# rpcgen generates code that emits unused variable warnings and suspicious
# function type casts
$(call BUILD_OBJECT_LIST,$(RPC_SRC)): CFLAGS += -Wno-unused-variable
suppress_cast_func_type_warning := $(call TEST_CC_ARG,-Wno-cast-function-type)
$(call BUILD_OBJECT_LIST,$(RPC_SRC)): CFLAGS += $(suppress_cast_func_type_warning)

OBJS = $(call BUILD_OBJECT_LIST,$(SRC))

common_cflags += -I $(COMMON_UTILS_DIR)
common_cflags += -I $(NVIDIA_CFG_DIR)
common_cflags += -I $(NVIDIA_NUMA_DIR)
common_cflags += -I $(NV_IOCTL_INC_DIR)
common_cflags += -I $(RPC_DIR)
common_cflags += -I $(OUTPUTDIR)
common_cflags += -I .

common_cflags += -DPROGRAM_NAME=\"$(NVIDIA_PERSISTENCED_PROGRAM_NAME)\"
common_cflags += -D_BSD_SOURCE
common_cflags += -D_XOPEN_SOURCE=500
common_cflags += -std=c99

CFLAGS += $(common_cflags)
HOST_CFLAGS += $(common_cflags)

ifneq ($(TARGET_OS),FreeBSD)
  LIBS += -ldl
endif

USE_TIRPC ?= $(shell $(PKG_CONFIG) --atleast-version=1.0.1 libtirpc && echo 1)

ifeq ($(USE_TIRPC),1)
  TIRPC_LDFLAGS ?= $(shell $(PKG_CONFIG) --libs libtirpc)
  TIRPC_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags libtirpc)
  $(call BUILD_OBJECT_LIST,$(SRC)): CFLAGS += $(TIRPC_CFLAGS)
  LIBS += $(TIRPC_LDFLAGS)
endif

##############################################################################
# build rules
##############################################################################

.PHONY: all
all: $(NVIDIA_PERSISTENCED) $(MANPAGE)

.PHONY: install
install: NVIDIA_PERSISTENCED_install MANPAGE_install

.PHONY: NVIDIA_PERSISTENCED_install
NVIDIA_PERSISTENCED_install: $(NVIDIA_PERSISTENCED)
	$(MKDIR) $(BINDIR)
	$(INSTALL) $(INSTALL_BIN_ARGS) $< $(BINDIR)/$(notdir $<)

.PHONY: MANPAGE_install
MANPAGE_install: $(MANPAGE)
	$(MKDIR) $(MANDIR)
	$(INSTALL) $(INSTALL_BIN_ARGS) $< $(MANDIR)/$(notdir $<)

$(eval $(call DEBUG_INFO_RULES, $(NVIDIA_PERSISTENCED)))
$(NVIDIA_PERSISTENCED).unstripped: $(OBJS)
	$(call quiet_cmd,LINK) $(CFLAGS) $(LDFLAGS) $(BIN_LDFLAGS) \
		-o $@ $(OBJS) $(LIBS)

# define the rule to build each object file
$(foreach src, $(SRC), $(eval $(call DEFINE_OBJECT_RULE,TARGET,$(src))))

.PHONY: clean clobber
clean clobber:
	$(RM) -rf $(NVIDIA_PERSISTENCED) $(MANPAGE) *~ \
		$(OUTPUTDIR)/*.o $(OUTPUTDIR)/*.d \
		$(GEN_MANPAGE_OPTS) $(OPTIONS_1_INC)

##############################################################################
# documentation
##############################################################################

AUTO_TEXT = ".\\\" WARNING: THIS FILE IS AUTO_GENERATED!  Edit $< instead."

.PHONY: doc
doc: $(MANPAGE)

GEN_MANPAGE_OPTS_SRC = gen-manpage-opts.c
GEN_MANPAGE_OPTS_SRC += $(COMMON_UTILS_DIR)/gen-manpage-opts-helper.c

GEN_MANPAGE_OPTS_OBJS = $(call BUILD_OBJECT_LIST,$(GEN_MANPAGE_OPTS_SRC))

$(foreach src,$(GEN_MANPAGE_OPTS_SRC), \
    $(eval $(call DEFINE_OBJECT_RULE,HOST,$(src))))

$(GEN_MANPAGE_OPTS): $(GEN_MANPAGE_OPTS_OBJS)
	$(call quiet_cmd,HOST_LINK) \
		$(HOST_CFLAGS) $(HOST_LDFLAGS) $(HOST_BIN_LDFLAGS) $^ -o $@

$(OPTIONS_1_INC): $(GEN_MANPAGE_OPTS)
	@$< > $@

$(MANPAGE_not_gzipped): nvidia-persistenced.1.m4 $(OPTIONS_1_INC) $(VERSION_MK)
	$(call quiet_cmd,M4) -D__HEADER__=$(AUTO_TEXT) -I $(OUTPUTDIR) \
	  -D__VERSION__=$(NVIDIA_VERSION) \
	  -D__DATE__="`$(DATE) -u -r nvidia-persistenced.1.m4 +%F || $(DATE) +%F`" \
	  -D__BUILD_OS__=$(TARGET_OS) \
	  $< > $@

$(MANPAGE_gzipped): $(MANPAGE_not_gzipped)
	$(GZIP_CMD) -9nf < $< > $@
