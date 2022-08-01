#include "local_efvi_socket.hpp"

#include "ci/efhw/common.h"
#include "lib/citools.h"
#include "lib/efvi_global.hpp"
#include "lib/efvi_tx.hpp"
#include "util/util.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <type_traits>

using namespace efvi;

#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define POLL_BATCH_SIZE 8

#ifndef EF_VI_TX_TIMESTAMP_TS_NSEC_INVALID
#define EF_VI_TX_TIMESTAMP_TS_NSEC_INVALID (1u<<30)
#endif 

#ifndef EF_VI_EVS_PER_CACHE_LINE
#define EF_VI_CACHE_LINE_SIZE       64
#define EF_VI_EV_SIZE             8
#define EF_VI_EVS_PER_CACHE_LINE  (EF_VI_CACHE_LINE_SIZE / EF_VI_EV_SIZE)
#endif

namespace efvi {

  struct filter_cookie {
    ef_filter_cookie cookie;
    char ip[16];
    uint16_t port;
  };

  // static variables
  static udp_pkt send_pkt;
  static struct pkt_buf *pkt_bufs[N_RX_BUFS];
  static ef_event events[POLL_BATCH_SIZE];
  static int      event_cursor = 0;
  ef_request_id   tx_ids[EF_VI_TRANSMIT_BATCH];
  ef_request_id   rx_ids[EF_VI_RECEIVE_BATCH];
  
  template<>
  int32_t VEfviSocket::compound_send(const char *buff, int32_t buff_len, MODE _) {
    struct pkt_buf *send_buf = (pkt_buf*) ((char*) p_queue + FIRST_TX_BUF * BUF_SIZE);

    memcpy(send_pkt.io.iov_base, buff, buff_len);
    send_pkt.io.iov_len = buff_len;  // dma_send auto calc the checksum
    
    return dma_send(send_buf, (ef_vi*) p_vi, buff_len + HEADER_SIZE);
  }
  
  template<>
  int32_t VEfviSocket::compound_send(const char *buff, int32_t buff_len, CTPIO_MODE _) {
    struct pkt_buf *send_buf = (pkt_buf*) ((char*) p_queue + FIRST_TX_BUF * BUF_SIZE);

    memcpy(send_pkt.io.iov_base, buff, buff_len);
    send_pkt.io.iov_len = buff_len;
    send_pkt.ip4->ip_tot_len_be16 = CI_BSWAP_BE16(28 + buff_len);
    send_pkt.udp->udp_len_be16 = CI_BSWAP_BE16(8 + buff_len);
    checksum_udp_pkt(&send_pkt);
    return ctpio_send(send_buf, (ef_vi*) p_vi, buff_len + HEADER_SIZE);
  }

  // from vi_internal.h
  static inline int sys_is_numa_(void) {
    struct stat s;
    return stat("/sys/devices/system/node/node1", &s) == 0;
  }
  
  static inline int sys_is_numa(void) {
    static int result;
    if (!result)
      result = sys_is_numa_() + 1;
    return result - 1;
  }
  
  void ef_vi_reset_rxq(struct ef_vi* vi) {
    ef_vi_rxq_state* qs = &vi->ep_state->rxq;
    qs->posted = 0;
    qs->added = 0;
    qs->removed = 0;
    qs->in_jumbo = 0;
    qs->bytes_acc = 0;
    qs->rx_ps_credit_avail = 1;
    qs->last_desc_i = vi->vi_is_packed_stream ? vi->vi_rxq.mask : 0;
    if (vi->vi_rxq.mask) {
      int i;
      for (i = 0; i <= vi->vi_rxq.mask; ++i)
        vi->vi_rxq.ids[i] = EF_REQUEST_ID_MASK;
    }
  }
  
  void ef_vi_reset_txq(struct ef_vi* vi) {
    ef_vi_txq_state* qs = &vi->ep_state->txq;
    qs->previous = 0;
    qs->added = 0;
    qs->removed = 0;
    qs->ts_nsec = EF_VI_TX_TIMESTAMP_TS_NSEC_INVALID;
    if (vi->vi_txq.mask) {
      int i;
      for (i = 0; i <= vi->vi_txq.mask; ++i)
        vi->vi_txq.ids[i] = EF_REQUEST_ID_MASK;
    }
  }
  
  void ef_vi_reset_evq(struct ef_vi* vi, int clear_ring) {
    if (clear_ring)
      memset(vi->evq_base, (char) 0xff, vi->evq_mask + 1);
    vi->ep_state->evq.evq_ptr = 0;
    vi->ep_state->evq.evq_clear_stride = -((int) sys_is_numa() ? EF_VI_EVS_PER_CACHE_LINE : 0);
    vi->ep_state->evq.sync_timestamp_synchronised = 0;
    vi->ep_state->evq.sync_timestamp_major = ~0u;
    vi->ep_state->evq.sync_flags = 0;
  }
  
  // manually export ef_vi_reset_state
  void ef_vi_init_state(ef_vi* vi) {
    efvi::ef_vi_reset_rxq(vi);
    efvi::ef_vi_reset_txq(vi);
    efvi::ef_vi_reset_evq(vi, 0);
  }
  
  int ef_vi_add_queue(ef_vi* evq_vi, ef_vi* add_vi) {
    int q_label;
    if (evq_vi->vi_qs_n == EF_VI_MAX_QS)
      return -EBUSY;
    evq_vi->vi_qs_n = 1;
    evq_vi->vi_qs[0] = add_vi;
    
    return q_label;
  }
}

VEfviSocket::VEfviSocket(
  std::string _local_addr, uint16_t _local_port,
  std::string _remote_addr, uint16_t _remote_port,
  uint8_t _local_mac[], uint8_t _remote_mac[]) : 
    local_addr(_local_addr), local_port(_local_port), 
    remote_addr(_remote_addr), remote_port(_remote_port) {
  local_addr_he = inet_network(_local_addr.c_str());
  remote_addr_he = inet_network(_remote_addr.c_str());
  memcpy(local_mac, _local_mac, sizeof(uint8_t) * 8);  
  memcpy(remote_mac, _remote_mac, sizeof(uint8_t) * 8);
  
  if (ef_driver_open(&driver_handle) != 0) {
    throw std::runtime_error("unable to open sfc driver_handle");
  }
}

VEfviSocket::~VEfviSocket() {
  del_efvi_filter();
}

void VEfviSocket::remap_kernel_mem() {
  void *p;
  char *mem_mmap_ptr;
  ef_vi *vi;
  int rc;
  // mmap from file
  mmap_file("/mnt/vi/vi", sizeof(ef_vi), &p_vi);
  if (access("/mnt/vi/huge", F_OK) == 0) {
    printf("using huge pages\n");
    mmap_file("/mnt/vi/huge/queue", 1024 * BUF_SIZE, &p_queue);
  } else {
    mmap_file("/mnt/vi/queue", N_BUFS * BUF_SIZE, &p_queue);
  }
  if (0 == access("/mnt/vi/filter_cookie", F_OK)) {
    mmap_file("/mnt/vi/filter_cookie", sizeof(ef_filter_cookie), &p_filter_cookie);
  }
  vi = (ef_vi*) p_vi;

  rc = ci_resource_mmap(driver_handle, vi->vi_resource_id, EFCH_VI_MMAP_MEM, vi->vi_mem_mmap_bytes, &p);
  if (rc < 0) {
    throw std::runtime_error("mmap vi mem failed: rc=" + std::to_string(rc));
  }
  vi->vi_mem_mmap_ptr = (char*) p;
  mem_mmap_ptr = (char*) p;
  
  if (vi->vi_flags & EF_VI_TX_CTPIO) {
    rc = ci_resource_mmap(driver_handle, vi->vi_resource_id, EFCH_VI_MMAP_CTPIO, 4096, &p);
    if (rc < 0) {
      throw std::runtime_error("mmap ctpio mem failed: rc=" + std::to_string(rc));
    }
    vi->vi_ctpio_mmap_ptr = (char*) p;
  }
  
  rc = ci_resource_mmap(driver_handle, vi->vi_resource_id, EFCH_VI_MMAP_IO, vi->vi_io_mmap_bytes, &p);
  if (rc < 0) {
    throw std::runtime_error("mmap io failed: rc=" + std::to_string(rc));
  }
  vi->vi_io_mmap_ptr = (char*) p;
  vi->io = (char*) p;

  // state / state_bytes: vi->ep_state_bytes
  rc = ci_resource_mmap(driver_handle, vi->vi_resource_id, EFCH_VI_MMAP_STATE,
    CI_ROUND_UP(vi->ep_state_bytes, CI_PAGE_SIZE), &p);
  if (rc < 0) {
    throw std::runtime_error("mmap state failed: rc=" + std::to_string(rc));
  }
  vi->ep_state = (ef_vi_state*) p;
  post_index = vi->ep_state->rxq.posted;
  
  uint32_t* ids = (uint32_t*) (vi->ep_state + 1);

  vi->evq_base = (char*) mem_mmap_ptr;
  mem_mmap_ptr += ((1024 * sizeof(efhw_event_t) + CI_PAGE_SIZE - 1) & CI_PAGE_MASK);
  vi->vi_rxq.descriptors = mem_mmap_ptr;
  vi->vi_rxq.ids = ids;
  mem_mmap_ptr += ((vi->vi_rxq.mask + 1) * 8 + CI_PAGE_SIZE - 1) & CI_PAGE_MASK;
  ids += 512;
  vi->vi_txq.descriptors = mem_mmap_ptr;
  vi->vi_txq.ids = ids;
}

void VEfviSocket::remap_vi() {
  ef_vi *vi;
  remap_kernel_mem();
  vi = (ef_vi*) p_vi;
  // efvi::ef_vi_init_state(vi);
  efvi::ef_vi_add_queue(vi, vi);
  // efvi::ef_vi_init_state(vi);
  ef10_vi_init(vi);
  ef_vi_get_mac(vi, driver_handle, local_mac);
  
  if (vi->vi_flags & EF_VI_TX_CTPIO) {
    printf("using CTPIO mode\n");
    _sender = [this](const char *buff, int32_t buff_len) -> int32_t {
      static CTPIO_MODE _;
      return this->compound_send<>(buff, buff_len, _);
    };
  } else {
    _sender = [this](const char *buff, int32_t buff_len) -> int32_t {
      static MODE _;
      return this->compound_send<>(buff, buff_len, _);
    };
  }
  printf("local_mac: " MAC_FMT "\n", local_mac[0], local_mac[1], local_mac[2], local_mac[3], local_mac[4], local_mac[5]);
}

bool VEfviSocket::del_efvi_filter() {
  ef_vi *vi = (ef_vi*) p_vi;
  ef_filter_cookie *filter_cookie = (ef_filter_cookie*) p_filter_cookie;
  if (filter_cookie && filter_cookie->filter_id != 0) {
    if (0 != ef_vi_filter_del(vi, driver_handle, filter_cookie)) {
      return false;
    }
    remove("/mnt/vi/filter_cookie");
    p_filter_cookie = nullptr;
    return true;
  }
  return true;
}

bool VEfviSocket::connect() {
  ef_vi *vi;
  filter_cookie *fc;
  int rc;
  
  if (!is_remapped) {
    remap_vi();
    is_remapped = true;
  }
  
  vi = (ef_vi*) p_vi;
  fc = (filter_cookie*) p_filter_cookie;
  
  
  // route...
  struct pkt_buf *send_buf = (pkt_buf*) ((char*) p_queue + FIRST_TX_BUF * BUF_SIZE);
  init_eth_hdr(send_buf->dma_buf, local_mac, remote_mac);
  send_pkt = init_udp_pkt(send_buf->dma_buf, "", 0,
    local_addr_he, remote_addr_he,
    local_port, remote_port
  );
  
  for (int i = 0; i < N_RX_BUFS; i++) {
    pkt_bufs[i] = (pkt_buf*) ((char*) p_queue + i * BUF_SIZE);
  }
  
  if (!fc) {
    int fd = open("/mnt/vi/filter_cookie", O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
      throw std::runtime_error("can't create filter_cookie");
    }
    mmap_trim_file(fd, sizeof(filter_cookie), &p_filter_cookie);
    fc = (filter_cookie*) p_filter_cookie;
  }
  
  if (fc->cookie.filter_id == 0) {
    ef_filter_spec fs;
    ef_filter_spec_init(&fs, EF_FILTER_FLAG_NONE);
    if ((rc = ef_filter_spec_set_ip4_local(&fs, IPPROTO_UDP, inet_addr(local_addr.c_str()), htons(local_port))) != 0) {
      throw std::runtime_error("set local ipv4 filter failed rc=" + std::to_string(rc));    
    }
    if ((rc = ef_vi_filter_add(vi, driver_handle, &fs, (ef_filter_cookie *) fc)) != 0) {
      throw std::runtime_error("add filter failed rc=" + std::to_string(rc));
    }
    fc->port = local_port;
    strncpy(fc->ip, local_addr.c_str(), 15);
    printf("created filter_cookie id: %d local addr: %s port: %d\n", fc->cookie.filter_id, local_addr.c_str(), local_port);
  } else if (strncmp(local_addr.c_str(), fc->ip, 15) != 0 || fc->port != local_port ){
    printf("detected existed filter_cookie\n");
    return false;
  } else {
    printf("reuse filter_cookie port: %d\n", fc->port);
  }
   
  return true;
}

bool VEfviSocket::close() {
  bool succ = true;
  succ &= del_efvi_filter();
  return succ;
}

int32_t VEfviSocket::send(const char *buff, int32_t buff_len) {
  return _sender(buff, buff_len);
}

int VEfviSocket::post_buf() {
  static ef_vi *vi = (ef_vi*) p_vi;
  static int rc;
  struct pkt_buf* pb = pkt_bufs[post_index++ & (N_RX_BUFS - 1)];
  if (vi->ep_state->rxq.posted >= N_RX_BUFS) return 0;
  if ((rc = ef_vi_receive_post(vi, pb->dma_buf_addr, pb->id)) == -EAGAIN) {
    return -EAGAIN;
  }
  // TRY(rc);
}

void VEfviSocket::wait_event() {
  static int      n_ev = 0;
  static int      i = 0;
  static ef_vi *vi = (ef_vi*) p_vi;
  
  int n_rx;

  while (1) {
    for (; event_cursor < n_ev; ++event_cursor)
      switch (EF_EVENT_TYPE(events[event_cursor])) {
      case EF_EVENT_TYPE_RX:
        ++event_cursor;
        return;
      case EF_EVENT_TYPE_TX:
        ef_vi_transmit_unbundle(vi, &(events[event_cursor]), tx_ids);
        break;
      case EF_EVENT_TYPE_RX_MULTI:
      case EF_EVENT_TYPE_RX_MULTI_DISCARD:
        n_rx = ef_vi_receive_unbundle(vi, &(events[event_cursor]), rx_ids);
        ++event_cursor;
        return;
      case EF_EVENT_TYPE_RX_DISCARD:
      default:
        break;
      }
    n_ev = ef_eventq_poll((vi), events, sizeof(events) / sizeof(events[0]));
    event_cursor = 0;
  }
}

const char *VEfviSocket::fetch_message() {
  static ef_vi *vi = (ef_vi*) p_vi;
  static int vi_recv_prefix = ef_vi_receive_prefix_len(vi);
  return (char *) pkt_bufs[EF_EVENT_RX_RQ_ID(events[event_cursor])]->dma_buf + vi_recv_prefix;
}

void VEfviSocket::loop_recv(
  std::function<void(char *buff, int32_t buff_len)> message_handler, 
  std::function<bool(uint32_t&, uint32_t)> error_handler, 
  std::function<bool()> end) {
  
  ef_vi *vi = (ef_vi*) p_vi;
  int vi_recv_prefix = ef_vi_receive_prefix_len(vi);
  unsigned long max_buf = ef_vi_receive_capacity(vi);
 
  while(end()) {  
    if (post_index - recv_index < max_buf / 2) {
      unsigned long total = N_RX_BUFS - (post_index - recv_index);
      if (total > (max_buf - (post_index - recv_index))) {
        total = ((max_buf - (post_index - recv_index)) >> 3) << 3;
      }
      if (total > 0 && total <= N_RX_BUFS) {
        for (unsigned i = 0; i < total; ++i) {
          unsigned long id = (post_index + i) % N_RX_BUFS;
          ef_vi_receive_init(vi, pkt_bufs[id]->dma_buf_addr, id);
        }
        ef_vi_receive_push(vi);
        post_index += total;
      }
    }
    
    int n_ev = ef_eventq_poll(vi, events, POLL_BATCH_SIZE);
    uint32_t idx = 0;
    
    for(int i = 0; i < n_ev; i++) {
      switch(EF_EVENT_TYPE(events[i])) {
      case EF_EVENT_TYPE_RX:
        idx = recv_index % N_RX_BUFS;
        retry:
        if (idx == EF_EVENT_RX_RQ_ID(events[i])) {
          auto len = EF_EVENT_RX_BYTES(events[i]);
          char *p = (char*) pkt_bufs[idx]->dma_buf + vi_recv_prefix;
          message_handler(p, len);
          rcv_index++;
        } else {
          if (error_handler 
            && error_handler(idx, EF_EVENT_RX_RQ_ID(events[i]))) {
            goto retry;  
          }
        }
        break;
      case EF_EVENT_TYPE_RX_MULTI:
      case EF_EVENT_TYPE_RX_MULTI_DISCARD:
        break;
      case EF_EVENT_TYPE_TX:
        ef_vi_transmit_unbundle(vi, &(events[i]), tx_ids);
        break;
      default:
        break;
      }
    }
  }
}
