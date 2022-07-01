#pragma once

#include "efvi_global.hpp"
#include "util/util.hpp"

static inline void dma_send(pkt_buf* pb, ef_vi* vi, int frame_len) {
  TRY(ef_vi_transmit(vi, pb->dma_buf_addr, frame_len, 0));
}