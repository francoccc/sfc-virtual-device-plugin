#pragma once

#include "efvi_global.hpp"

static inline int dma_send(pkt_buf* pb, ef_vi* vi, int frame_len) {
  return ef_vi_transmit(vi, pb->dma_buf_addr, frame_len, 0);
}