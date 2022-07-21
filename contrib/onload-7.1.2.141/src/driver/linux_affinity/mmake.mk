# SPDX-License-Identifier: GPL-2.0
# X-SPDX-Copyright-Text: (c) Solarflare Communications Inc
SFCAFF_SRCS	:= sfcaffinity.c

SFCAFF_TARGET	:= sfc_affinity.o
SFCAFF_TARGET_SRCS := $(SFCAFF_SRCS)

TARGETS		:= $(SFCAFF_TARGET)

IMPORT		:= ../linux_net/driverlink_api.h


######################################################
# linux kbuild support
#

KBUILD_EXTRA_SYMBOLS := $(BUILDPATH)/driver/linux_net/Module.symvers

all: $(KBUILD_EXTRA_SYMBOLS) $(BUILDPATH)/driver/linux_affinity/autocompat.h
	$(MAKE) $(MMAKE_KBUILD_ARGS) M=$(CURDIR)
	cp -f sfc_affinity.ko $(DESTPATH)/driver/linux
ifndef CI_FROM_DRIVER
	$(warning "Due to build order sfc.ko may be out-of-date. Please build in driver/linux_net")
endif

$(BUILDPATH)/driver/linux_affinity/autocompat.h: kernel_compat.sh ../linux_net/kernel_compat_funcs.sh
	./kernel_compat.sh -k $(KPATH) $(if $(filter 1,$(V)),-v,-q) >$@

clean:
	@$(MakeClean)
	rm -rf *.ko Module.symvers .*.cmd


ifdef MMAKE_IN_KBUILD

dummy := $(shell echo>&2 "MMAKE_IN_KBUILD")

obj-m := $(SFCAFF_TARGET) 

sfc_affinity-objs := $(SFCAFF_TARGET_SRCS:%.c=%.o)

endif # MMAKE_IN_KBUILD
