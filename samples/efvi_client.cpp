/*
 * @Descripition: efvi_client
 * @Author: Franco Chen
 * @Date: 2022-05-09 14:34:57
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-07-05 16:59:48
 */
#include "include/local_efvi_socket.hpp"
#include "lib/efvi_global.hpp"
#include <iostream>
#include <arpa/inet.h>

uint8_t local_mac[]  = {0x00, 0x0f, 0x53, 0x98, 0x0c, 0xf1}; // test local mac address
uint8_t remote_mac[] = { 0x00, 0x50, 0x56, 0x9d, 0xa6, 0x85 };

int main(int argc, char** argv) {

  std::cout << "INF: try get ef_vi" << std::endl;
  
  if (argc >= 4) {
    laddr_he = inet_network(argv[1]);
    printf("INF: laddr_he=0x%08x\n", laddr_he);
    raddr_he = inet_network(argv[2]);
    printf("INF: raddr_he=0x%08x\n", raddr_he);
    port_he = std::stoi(argv[3]);
    printf("INF: port=%d\n", port_he);
  }
  efvi::VEfviSocket socket(argv[1], port_he, argv[2], port_he, local_mac, remote_mac);
  socket.connect();

  std::string msg;
  
  // std::thread t([]() {
  //   unsigned long max_buf = ef_vi_receive_capacity(vi);
  //   vi_recv_prefix = ef_vi_receive_prefix_len(vi);
  //   std::cout << "max_buf:" << max_buf << std::endl;
  //   printf("%p\n", pkt_bufs[0]->dma_buf_addr);
  //   // std::cout << *((char *) pkt_bufs[0]->dma_buf_addr) << std::endl;
  //   std::cout << "vi_receive_prefix:" << vi_recv_prefix << std::endl;
    
  //   while(true) {
  //    if (post_index - rcv_index < max_buf / 2) {
  //      unsigned long total = N_RX_BUFS - (post_index - rcv_index);
  //      if (total > (max_buf - (post_index - rcv_index))) {
  //        total = ((max_buf - (post_index - rcv_index)) >> 3) << 3;
  //      }
  //      std::cout << "total:" << total << std::endl; 
  //      if (total > 0 && total <= N_RX_BUFS) {
  //        for (unsigned i = 0; i < total; ++i) {
  //          unsigned long id = (post_index + i) % N_RX_BUFS;
  //          ef_vi_receive_init(vi, pkt_bufs[id]->dma_buf_addr, id);
  //        }
  //        ef_vi_receive_push(vi);
  //        post_index += total;
  //      }
  //    }
  
  //    ef_event events[POLL_BATCH_SIZE];
  //    ef_request_id rx_ids[EF_VI_RECEIVE_BATCH];
  //    int n_ev = ef_eventq_poll(vi, events, POLL_BATCH_SIZE);
  //    unsigned long idx = 0;
  
  //    for(int i = 0; i < n_ev; i++) {
  //      switch(EF_EVENT_TYPE(events[i])) {
  //      case EF_EVENT_TYPE_RX:
  //        idx = rcv_index % N_RX_BUFS;
  //        if (idx == EF_EVENT_RX_RQ_ID(events[i])) {
  //          // recv;
  //          // pkt_bufs[idx]->len = EF_EVENT_RX_BYTES(events[i]);
  //          auto len = EF_EVENT_RX_BYTES(events[i]);
  //          char* p = (char*) pkt_bufs[idx]->dma_buf + vi_recv_prefix;
  //          print_hex(p, len);
  //          rcv_index++;
  //        } else {
  //          std::cout << "rx_index not right" << std::endl;
  //        }
  //        break;
  //      }
  //    }
  //   }
  // });
  
  while(true) {
    std::cin >> msg;
    socket.send(msg.c_str(), msg.size());
  }

  return 0;
}