# SPDX-License-Identifier: BSD-2-Clause
# X-SPDX-Copyright-Text: (c) Solarflare Communications Inc
SUBDIRS	:= wire_order tproxy_preload woda_preload hwtimestamping \
           sync_preload l3xudp_preload

ifneq ($(ONLOAD_ONLY),1)
# These tests have dependency on kernel_compat lib,
# tests/tap, libmnl that are !ONLOAD_ONLY
SUBDIRS += oof onload_remote_monitor
ifneq ($(NO_TEAMING),1)
SUBDIRS += cplane_unit cplane_sysunit
endif
endif

OTHER_SUBDIRS	:= titchy_proxy thttp

all:
	+@$(MakeSubdirs)

clean:
	@$(MakeClean)

