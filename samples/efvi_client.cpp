/*
 * @Descripition: efvi_client
 * @Author: Franco Chen
 * @Date: 2022-05-09 14:34:57
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-07-05 17:08:15
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
  
  while(true) {
    std::cin >> msg;
    socket.send(msg.c_str(), msg.size());
  }

  return 0;
}