/*
 * @Descripition: efvi_client
 * @Author: Franco Chen
 * @Date: 2022-05-09 14:34:57
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-07-14 15:35:30
 */
#include "include/local_efvi_socket.hpp"
#include "lib/efvi_global.hpp"
#include "util/util.hpp"
#include <iostream>
#include <thread>
#include <arpa/inet.h>

uint8_t local_mac[]  = {0x00, 0x0f, 0x53, 0x98, 0x0c, 0xf1};    // test local mac address
uint8_t remote_mac[] = { 0x00, 0x0f, 0x53, 0x98, 0x0c, 0xf0 };  // ip: 10.18.17.146
// uint8_t remote_mac[] = { 0x00, 0x0f, 0x53, 0x98, 0x0c, 0xf1 };

extern void print_hex(const unsigned char *data, size_t len);

int64_t get_time() {
  static timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return tp.tv_nsec + tp.tv_sec * 1000000000LLU;
}

static int64_t start, end;
int main(int argc, char** argv) {

  std::cout << "INF: try get ef_vi" << std::endl;
  
  if (argc >= 4) {
    laddr_he = inet_network(argv[1]);
    printf("INF: laddr_he=0x%08x\n", laddr_he);
    raddr_he = inet_network(argv[2]);
    printf("INF: raddr_he=0x%08x\n", raddr_he);
    lport_he = std::stoi(argv[3]);
    printf("INF: lport=%d\n", lport_he);
    rport_he = std::stoi(argv[4]);
    printf("INF: rport=%d\n", rport_he);
  }
  
  efvi::VEfviSocket socket(argv[1], lport_he, argv[2], rport_he, local_mac, remote_mac);
  if (!socket.connect()) {
    std::cout << "connect failed, retry..." << std::endl;
    socket.close();
    socket.connect();
  }

  int send_left = 100;
  std::cout << "type any then continue" << std::endl;
  getchar();
  
  socket.send((char*) &start, sizeof(int64_t));
  
  start = get_high_resolution_time();
  
  socket.loop_recv(
    [&socket, &send_left](char *buff, int32_t buff_len) {
      socket.send((char*) &start, sizeof(int64_t));
      send_left--;
    }, 
    [](unsigned long idx, unsigned long event_id) {
      std::cout 
        << "recv not correct message idx: " << idx
        << " event_id: " << event_id << std::endl;
    }, 
    [&send_left]() {
      return send_left > 0;
    }
  );
    
  end = get_high_resolution_time();
  
  std::cout << "elapsed time: " << (end - start) / 100  << std::endl;
  return 0;
}