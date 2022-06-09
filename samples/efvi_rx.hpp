#pragma once
#include "efvi_global.hpp"
#include "util/util.hpp"

static inline void dma_send(ef_vi* vi, struct pkt_buf* pb, int len) {
  TRY(ef_vi_transmit(vi, pb->dma_buf_addr, tx_frame_len, 0));
}