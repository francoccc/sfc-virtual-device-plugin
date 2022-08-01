#pragma once

#include "efvi_global.hpp"

#define CTPIO_THRESH 64

static inline int dma_send(pkt_buf* pb, ef_vi* vi, int frame_len) {
  return ef_vi_transmit(vi, pb->dma_buf_addr, frame_len, 0);
}

static inline int ctpio_send(pkt_buf* pb, ef_vi* vi, int frame_len) {
  ef_vi_transmit_ctpio(vi, pb->dma_buf, frame_len, CTPIO_THRESH);
  return ef_vi_transmit_ctpio_fallback(vi, pb->dma_buf_addr, frame_len, 0);
}
