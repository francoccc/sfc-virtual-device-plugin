#pragma once
#include "efvi_global.hpp"
#include "util/util.hpp"

static inline void dma_recv(ef_vi* vi, struct pkt_buf* pb) {
  TRY(ef_vi_receive_post(vi, pb->dma_buf_addr, pb->id));
}

static inline void rx_wait(ef_vi* vi) {
  /* We might exit with events read but unprocessed. */
  static int      n_ev = 0;
  static int      i = 0;
  static ef_event evs[EF_VI_EVENT_POLL_MIN_EVS];
  static int n_rx = 0;
  ef_request_id   tx_ids[EF_VI_TRANSMIT_BATCH];
  ef_request_id   rx_ids[EF_VI_RECEIVE_BATCH];

  while (!n_rx) {
    for (; i < n_ev; ++i)
      switch (EF_EVENT_TYPE(evs[i])) {
      case EF_EVENT_TYPE_RX:
        std::cout << "receive msg" << std::endl;
        ++i;
        return;
      case EF_EVENT_TYPE_TX:
        ef_vi_transmit_unbundle(vi, &(evs[i]), tx_ids);
        break;
      case EF_EVENT_TYPE_TX_ALT:
        // ++(tx_alt.complete_id);
        break;
      case EF_EVENT_TYPE_RX_MULTI:
      case EF_EVENT_TYPE_RX_MULTI_DISCARD:
        n_rx = ef_vi_receive_unbundle(vi, &(evs[i]), rx_ids);
        ++i;
        return;
      case EF_EVENT_TYPE_RX_DISCARD:
        // if (EF_EVENT_RX_DISCARD_TYPE(evs[i]) == EF_EVENT_RX_DISCARD_CRC_BAD &&
        //   (ef_vi_flags(vi) & EF_VI_TX_CTPIO) && !cfg_ctpio_no_poison) {
        //   /* Likely a poisoned frame caused by underrun.  A good copy will
        //    * follow.
        //    */
        //   rx_post(vi);
        //   break;
        // }
      /* Otherwise, fall through. */
      default:
        fprintf(stderr, "ERROR: unexpected event "EF_EVENT_FMT"\n",
          EF_EVENT_PRI_ARG(evs[i]));
        break;
      }
    n_ev = ef_eventq_poll((vi), evs, sizeof(evs) / sizeof(evs[0]));
    i = 0;
  }
  n_rx--;
}