/*
 * @Descripition: 
 * @Author: Franco Chen
 * @Date: 2022-06-30 17:05:27
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-06-30 17:43:39
 */
#include <stdio.h>
#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <string>
#include <iostream>

static void get_local_ip_address(void) {
  struct ifaddrs* if_addrs = NULL;
  getifaddrs(&if_addrs);
  
  for (struct ifaddrs* ifa = if_addrs; ifa != NULL; ifa = ifa->ifa_next) {
    if (strncmp(ifa->ifa_name, "lo", 2) == 0 || ifa->ifa_addr->sa_family != AF_INET) {
      continue;
    }
    uint32_t ip, mask;
    std::cout << "name:" << ifa->ifa_name << std::endl;
    std::cout << "addr_family:" << ifa->ifa_addr->sa_family << std::endl;
    std::cout << "addr:" << inet_ntoa(((struct sockaddr_in*)ifa->ifa_addr)->sin_addr) << std::endl;
    std::cout << "addr_s: " << (ip = ntohl(((struct sockaddr_in*) ifa->ifa_addr)->sin_addr.s_addr)) << std::endl;
    std::cout << "netmask:" << inet_ntoa(((struct sockaddr_in*) ifa->ifa_netmask)->sin_addr) << std::endl;
    std::cout << "netmask_s:" << (mask = ntohl(((struct sockaddr_in*) ifa->ifa_netmask)->sin_addr.s_addr)) << std::endl;
    uint32_t subnet;
    struct in_addr in;
    subnet = ip & mask;
    in.s_addr = htonl(subnet);
    std::cout << "subnet:" << inet_ntoa(in) << std::endl;
    
    std::cout << "broadaddr:" << inet_ntoa(((struct sockaddr_in*) ifa->ifa_ifu.ifu_broadaddr)->sin_addr) << std::endl;
    std::cout << "dstaddr:" << inet_ntoa(((struct sockaddr_in*) ifa->ifa_ifu.ifu_dstaddr)->sin_addr) << std::endl;

    std::cout << std::endl;
  }
}

int main(int argc, char **argv) {
  get_local_ip_address();
  return 0;
}