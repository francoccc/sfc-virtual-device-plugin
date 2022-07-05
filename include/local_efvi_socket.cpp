#include "local_efvi_socket.hpp"

#include "ci/efhw/common.h"
#include "lib/citools.h"
#include "lib/efvi_global.hpp"
#include "lib/efvi_tx.hpp"
#include "util/util.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>

using namespace efvi;

#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define POLL_BATCH_SIZE 8

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
}

VEfviSocket::~VEfviSocket() {
  del_efvi_filter();
}

void VEfviSocket::remap_kernel_mem() {
  void *p;
  char *mem_mmap_ptr;
  ef_vi *vi;
  int rc;
  if (ef_driver_open(&driver_handle) != 0) {
    throw std::runtime_error("unable to open sfc driver_handle");
  }
  // mmap from file
  mmap_file("/mnt/vi/vi", sizeof(ef_vi), &p_vi);
  mmap_file("/mnt/vi/queue", N_BUFS * BUF_SIZE, &p_queue);
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

bool VEfviSocket::del_efvi_filter() {
  ef_filter_cookie *filter_cookie = (ef_filter_cookie*) p_filter_cookie;
  if (filter_cookie && filter_cookie->filter_id != 0) {
    if (0 != ef_vi_filter_del(vi, driver_handle, filter_cookie)) {
      return false;
    }
    return true;
  }
  return true;
}

bool VEfviSocket::connect() {
  ef_vi *vi;
  ef_filter_cookie *filter_cookie;
  int rc;
  
  remap_kernel_mem();
  
  vi = (ef_vi*) p_vi;
  filter_cookie = (ef_filter_cookie*) p_filter_cookie;
  ef10_vi_init(vi);
  ef_vi_get_mac(vi, driver_handle, local_mac);
  printf("local_mac: " MAC_FMT "\n", local_mac[0], local_mac[1], local_mac[2], local_mac[3], local_mac[4], local_mac[5]);
  
  // route...
  struct pkt_buf *send_buf = (pkt_buf*) ((char*) p_queue + FIRST_TX_BUF * BUF_SIZE);
  init_eth_hdr(send_buf->dma_buf, local_mac, remote_mac);
  
  if (!filter_cookie) {
    int fd = open("/mnt/vi/filter_cookie", O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
      throw std::runtime_error("can't create filter_cookie");
    }
    mmap_trim_file(fd, sizeof(ef_filter_cookie), &p_filter_cookie);
    filter_cookie = (ef_filter_cookie*) p_filter_cookie;
  }
  
  if (filter_cookie->filter_id == 0) {
    ef_filter_spec fs;
    ef_filter_spec_init(&fs, EF_FILTER_FLAG_NONE);
    if ((rc = ef_filter_spec_set_ip4_local(&fs, IPPROTO_UDP, inet_addr(local_addr.c_str()), htons(local_port))) != 0) {
      throw std::runtime_error("set local ipv4 filter failed rc=" + std::to_string(rc));    
    }
    if ((rc = ef_vi_filter_add(vi, driver_handle, &fs, filter_cookie)) != 0) {
      throw std::runtime_error("add filter failed rc=" + std::to_string(rc));
    }
    printf("created filter_cookie id: %d\n", filter_cookie->filter_id);
  } else {
    printf("detected existed filter_cookie\n");
  }
  
  return true;
}

bool VEfviSocket::close() {
  bool succ = true;
  succ &= del_efvi_filter();
  return succ;
}

int32_t VEfviSocket::send(const char *buff, int32_t buff_len) {
  struct pkt_buf *send_buf = (pkt_buf*) ((char*) p_queue + FIRST_TX_BUF * BUF_SIZE);
 
  udp_pkt pkt = init_udp_pkt(send_buf->dma_buf, buff, buff_len,
    local_addr_he, remote_addr_he, 
    local_port, remote_port);
  checksum_udp_pkt(&pkt);
  return dma_send(send_buf, (ef_vi*) p_vi, buff_len + HEADER_SIZE);
}
