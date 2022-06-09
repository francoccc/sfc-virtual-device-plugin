#include "string.h"

#include <arpa/inet.h>
#include <net/if.h>

#include "efvi_global.hpp"
#include "efvi_tx.hpp"
#include "util/util.hpp"
#include "util/net_util.hpp"

#include "citools.h"

ef_vi* create_ef_vi(std::string vi_name) {  
  auto path = "/dev/shm/" + vi_name;
  int fd = open(path.c_str(), O_RDWR | O_CREAT, 0666);
  if (fd < 0) {
    throw std::runtime_error("can't open or create file");
  }
  if (0 != ftruncate64(fd, sizeof(ef_vi))) {
    throw std::runtime_error("can't ftruncate file size");
  }
  void* p = mmap(NULL, sizeof(ef_vi), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  
  close(fd);
  return (ef_vi*) p;
}

void init_packet_buffer(std::string vi_name, std::string pkt_buf_name) {
  auto path = "/dev/shm/" + vi_name + "_" + pkt_buf_name;
  int fd = open(path.c_str(), O_RDWR | O_CREAT, 0666);
  if (fd < 0) {
    throw std::runtime_error("can't open or create file");
  }
  if (0 != ftruncate64(fd, N_BUFS * BUF_SIZE)) {
    throw std::runtime_error("can't ftruncate file size");
  }
  void* p = mmap(NULL, N_BUFS * BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);

  TRY(ef_memreg_alloc(&memreg, driver_handle, &pd, driver_handle, p, N_BUFS * BUF_SIZE));

  for (int i = 0; i <= N_RX_BUFS; ++i) {
    pkt_bufs[i] = (pkt_buf*) ((char*) p + i * BUF_SIZE);
    pkt_bufs[i]->dma_buf_addr = ef_memreg_dma_addr(&memreg, i * BUF_SIZE);
  } 

  struct pkt_buf* pb;
  for (int i = 0; i <= N_RX_BUFS; ++i) {
    pb = pkt_bufs[i];
    pb->id = i;
    pb->dma_buf_addr += offsetof(struct pkt_buf, dma_buf);
  }
}

void init_efvi_context(int ifindex) {
  if (efvi_context_initialized) {
    return;
  }
  enum ef_pd_flags pd_flags = EF_PD_DEFAULT;
  TRY(ef_driver_open(&driver_handle));
  TRY(ef_pd_alloc(&pd, driver_handle, ifindex, pd_flags));
  
  fprintf(stderr, "pd: %d\n", pd.pd_resource_id);
  efvi_context_initialized = true;
}

void alloc_virtual_interface(const char* vi_name) {
  enum ef_pd_flags pd_flags = EF_PD_DEFAULT;
  ef_filter_spec filter_spec;
  enum ef_vi_flags vi_flags = EF_VI_FLAGS_DEFAULT;
  int rc;
  vi = create_ef_vi(vi_name);

  if ((rc = ef_vi_alloc_from_pd(vi, driver_handle,
    &pd, driver_handle, -1, -1, -1,
    NULL, -1, vi_flags)) < 0) {
    if (rc == -EPERM) {
      fprintf(stderr, "Failed to allocate VI without event merging\n");
      vi_flags = EF_VI_RX_EVENT_MERGE;
    }
    else
      TRY(rc);
  }
}

#ifdef ENTRYPOINT

#include <iostream>

void print_efvi_mac() {
  uint8_t mac[6];
  ef_vi_get_mac(vi, driver_handle, mac);
  printf("INF: mac=");
  for(int i = 0; i < 6; ++i) {
    printf("%02x", mac[i]);
    if (i != 5) {
      printf(":");
    }
  }
  printf("\n");
}

uint8_t local_mac[] = { 0x00, 0x0f, 0x53, 0x98, 0x0c, 0xf0 }; // test local mac address
uint8_t remote_mac[] = { 0x00, 0x50, 0x56, 0x9d, 0xa6, 0x85 };

int main(int argc, char** argv) {
  int ifindex;

  if (argc >= 4) {
    laddr_he = inet_network(argv[2]);
    printf("INF: laddr_he=0x%08x\n", laddr_he);
    raddr_he = inet_network(argv[3]);
    printf("INF: raddr_he=0x%08x\n", raddr_he);
    port_he = std::stoi(argv[4]);
    printf("INF: port=%d\n", port_he);
  }

  parse_interface(argv[1], &ifindex);

  init_efvi_context(ifindex);

  alloc_virtual_interface("vi01");
  std::cout << "INF: create virtual_interface succ" << std::endl;
  print_efvi_mac();

  init_packet_buffer("vi01", "queue");
  std::cout << "INF: create packet_buffers succ" << std::endl;

  init_eth_hdr(pkt_bufs[FIRST_TX_BUF]->dma_buf, local_mac, remote_mac);

  std::string msg;

  pkt_buf* pb = pkt_bufs[FIRST_TX_BUF];
  printf("INF: dma_offset=%d\n", pb->dma_buf_addr);
  std::cout << "INF: efvi_ts_format=" << (int) vi->ts_format << std::endl;
  std::cout << "INF: efvi_mem_mmap_bytes=" << vi->vi_mem_mmap_bytes << std::endl;
  std::cout << "INF: efvi_mem_mmap_ptr=" << vi->vi_mem_mmap_ptr[0] << std::endl;
  while (true) {
    std::cin >> msg;
    // init_eth_hdr(pb->dma_buf, local_mac, remote_mac);
    // udp_pkt pkt = init_udp_pkt(pb->dma_buf, msg.data(), msg.size(),
    //   laddr_he, raddr_he,
    //   port_he, port_he);
    // std::string _msg((char*) pkt.io.iov_base, (char*) pkt.io.iov_base + pkt.io.iov_len);
    // std::cout << "INF: sending=" << _msg << std::endl;

    // std::string dma_msg((char*) pb->dma_buf + HEADER_SIZE, (char*) pb->dma_buf + HEADER_SIZE + msg.size());
    // std::cout << "INF: dma_msg=" << dma_msg << std::endl;

    // checksum_udp_pkt(&pkt);
    dma_send(pb, vi, 5 + HEADER_SIZE);
  }
}

#endif