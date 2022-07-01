/*
 * @Descripition: efvi_client
 * @Author: Franco Chen
 * @Date: 2022-05-09 14:34:57
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-07-01 16:15:29
 */
#include "efvi_global.hpp"
#include "efvi_tx.hpp"
#include "citools.h"

#include "ci/efhw/common.h"

#include <stdexcept>
#include <string>
#include <arpa/inet.h>
#include <net/if.h>
#include <thread>

#define POLL_BATCH_SIZE 8

ef_vi* get_ef_vi(std::string vi_name) {
  // auto path = "/dev/shm/" + vi_name + "/vi";
  auto path = "/mnt/" + vi_name + "/vi";

  int fd = open(path.c_str(), O_RDWR, 0666);
  if (fd < 0) {
    throw std::runtime_error("can't open or create file");
  }
  void* p = mmap(NULL, sizeof(ef_vi), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);

  return (ef_vi*) p;
}

void init_packet_buffer(std::string vi_name, std::string pkt_buf_name) {
  // auto path = "/dev/shm/" + vi_name + "/" + pkt_buf_name;
  auto path = "/mnt/" + vi_name + "/" + pkt_buf_name;

  int fd = open(path.c_str(), O_RDWR, 0666);
  if (fd < 0) {
    throw std::runtime_error("can't open or create file");
  }

  void* p = mmap(NULL, N_BUFS * BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);

  for (int i = 0; i < N_BUFS; ++i) {
    pkt_bufs[i] = (pkt_buf*) ((char*) p + i * BUF_SIZE);
  }
}

void del_fs_filter() {
  
}

#ifdef ENTRYPOINT

#include <iostream>

uint8_t local_mac[]  = {0x00, 0x0f, 0x53, 0x98, 0x0c, 0xf1}; // test local mac address
uint8_t remote_mac[] = { 0x00, 0x50, 0x56, 0x9d, 0xa6, 0x85 };

int main(int argc, char** argv) {
  TRY(ef_driver_open(&driver_handle));
  std::cout << "INF: open ef_driver=" << driver_handle << std::endl;

  std::cout << "INF: try get ef_vi" << std::endl;
  
  if (argc >= 4) {
    laddr_he = inet_network(argv[2]);
    printf("INF: laddr_he=0x%08x\n", laddr_he);
    raddr_he = inet_network(argv[3]);
    printf("INF: raddr_he=0x%08x\n", raddr_he);
    port_he = std::stoi(argv[4]);
    printf("INF: port=%d\n", port_he);
  }

  vi = get_ef_vi(argv[1]);
  ef10_vi_init(vi);
  std::cout << "INF: get ef_vi succ" << std::endl;

  init_packet_buffer(argv[1], "queue");
  std::cout << "INF: get queue succ" << std::endl;

  init_eth_hdr(pkt_bufs[FIRST_TX_BUF]->dma_buf, local_mac, remote_mac);
  
  std::string msg;

  pkt_buf* pb = pkt_bufs[FIRST_TX_BUF];
  printf("INF: dma_offset=%d\n", pb->dma_buf_addr);
  std::cout << "INF: efvi_resouce_id=" << vi->vi_resource_id << std::endl;
  std::cout << "INF: efvi_ts_format=" << (int) vi->ts_format << std::endl;
  std::cout << "INF: efvi_mem_mmap_bytes=" << vi->vi_mem_mmap_bytes << std::endl;
  
  void* p, *mem_mmap_ptr;
  int rc;
  
  // auto off = EFAB_MMAP_OFFSET_MAKE(efch_make_resource_id(vi->vi_resource_id), EFCH_VI_MMAP_MEM);
  
  // p = mmap(NULL, vi->vi_mem_mmap_bytes, PROT_READ | PROT_WRITE, 
  //       MAP_SHARED, driver_handle, off);
  rc = ci_resource_mmap(driver_handle, vi->vi_resource_id, EFCH_VI_MMAP_MEM, vi->vi_mem_mmap_bytes, &p);
  if (rc < 0) {
    std::cout << "mmap mem failed: rc=" << rc << std::endl;
    exit(1);
  }      
  vi->vi_mem_mmap_ptr = (char*) p;
  mem_mmap_ptr = p;
  
  rc = ci_resource_mmap(driver_handle, vi->vi_resource_id, EFCH_VI_MMAP_IO, vi->vi_io_mmap_bytes, &p);
  if (rc < 0) {
    std::cout << "mmap io failed: rc=" << rc << std::endl;
    exit(1);
  }
  vi->vi_io_mmap_ptr = (char*) p;
  vi->io = (char*) p;
  
  // state / state_bytes: vi->ep_state_bytes
  rc = ci_resource_mmap(driver_handle, vi->vi_resource_id, EFCH_VI_MMAP_STATE, 
          CI_ROUND_UP(vi->ep_state_bytes, CI_PAGE_SIZE), &p);
  if (rc < 0) {
    std::cout << "mmap state failed: rc=" << rc << std::endl;
    exit(1);
  }
  vi->ep_state = (ef_vi_state*) p;
  uint32_t* ids = (uint32_t*) (vi->ep_state + 1);
  
  vi->evq_base = mem_mmap_ptr;
  mem_mmap_ptr += ((1024 * sizeof(efhw_event_t) + CI_PAGE_SIZE - 1) & CI_PAGE_MASK);
  vi->vi_rxq.descriptors = mem_mmap_ptr;
  vi->vi_rxq.ids = ids;
  mem_mmap_ptr += ((vi->vi_rxq.mask + 1) * 8 + CI_PAGE_SIZE - 1) & CI_PAGE_MASK;
  ids += 512;
  vi->vi_txq.descriptors = mem_mmap_ptr;
  vi->vi_txq.ids = ids;
  
  printf("vi_flag: %08x\n", vi->vi_flags);
  
  // ef_filter_spec fs;
  // ef_filter_spec_init(&fs, EF_FILTER_FLAG_NONE);
  // TRY(ef_filter_spec_set_ip4_local(&fs, IPPROTO_UDP, inet_addr(argv[2]), htons(argv[4])));
  // TRY(ef_vi_filter_add(vi, driver_handle, &fs, &filter_cookie_out));
  
  std::thread t([]() {
    unsigned long max_buf = ef_vi_receive_capacity(vi);
    vi_recv_prefix = ef_vi_receive_prefix_len(vi);
    std::cout << "max_buf:" << max_buf << std::endl;
    printf("%p\n", pkt_bufs[0]->dma_buf_addr);
    // std::cout << *((char *) pkt_bufs[0]->dma_buf_addr) << std::endl;
    std::cout << "vi_receive_prefix:" << vi_recv_prefix << std::endl;
    
    while(true) {
     if (post_index - rcv_index < max_buf / 2) {
       unsigned long total = N_RX_BUFS - (post_index - rcv_index);
       if (total > (max_buf - (post_index - rcv_index))) {
         total = ((max_buf - (post_index - rcv_index)) >> 3) << 3;
       }
       std::cout << "total:" << total << std::endl; 
       if (total > 0 && total <= N_RX_BUFS) {
         for (unsigned i = 0; i < total; ++i) {
           unsigned long id = (post_index + i) % N_RX_BUFS;
           ef_vi_receive_init(vi, pkt_bufs[id]->dma_buf_addr, id);
         }
         ef_vi_receive_push(vi);
         post_index += total;
       }
     }
  
     ef_event events[POLL_BATCH_SIZE];
     ef_request_id rx_ids[EF_VI_RECEIVE_BATCH];
     int n_ev = ef_eventq_poll(vi, events, POLL_BATCH_SIZE);
     unsigned long idx = 0;
  
     for(int i = 0; i < n_ev; i++) {
       switch(EF_EVENT_TYPE(events[i])) {
       case EF_EVENT_TYPE_RX:
         idx = rcv_index % N_RX_BUFS;
         if (idx == EF_EVENT_RX_RQ_ID(events[i])) {
           // recv;
           // pkt_bufs[idx]->len = EF_EVENT_RX_BYTES(events[i]);
           auto len = EF_EVENT_RX_BYTES(events[i]);
           char* p = (char*) pkt_bufs[idx]->dma_buf + vi_recv_prefix;
           print_hex(p, len);
           rcv_index++;
         } else {
           std::cout << "rx_index not right" << std::endl;
         }
         break;
       }
     }
    }
  });
  
  std::cout << "INF: efvi_mem_mmap_ptr=" << vi->vi_mem_mmap_ptr[0] << std::endl;
  while(true) {
    std::cin >> msg;
    init_eth_hdr(pb->dma_buf, local_mac, remote_mac);
    udp_pkt pkt = init_udp_pkt(pb->dma_buf, msg.data(), msg.size(), 
        laddr_he, raddr_he,
        port_he, port_he);
    std::string _msg((char*) pkt.io.iov_base, (char*) pkt.io.iov_base + pkt.io.iov_len);
    std::cout << "INF: sending=" << _msg << std::endl;
    
    std::string dma_msg((char*) pb->dma_buf + HEADER_SIZE, (char*) pb->dma_buf + HEADER_SIZE + msg.size());
    std::cout << "INF: dma_msg=" << dma_msg << std::endl;
    
    checksum_udp_pkt(&pkt);
    dma_send(pb, vi, msg.size() + HEADER_SIZE);
  }

  return 0;
}

#endif