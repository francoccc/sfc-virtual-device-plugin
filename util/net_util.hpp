/*
 * @Descripition: Net Utility
 * @Author: Franco Chen
 * @Date: 2022-05-09 14:34:57
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-06-10 14:50:09
 */
#pragma once

#include <string>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

// get IPv4 IP address
inline std::string get_address_from_interface(std::string interface) {
  int fd;
  struct ifreq ifr;
  fd = socket(AF_INET, SOCK_DGRAM, 0);

  ifr.ifr_addr.sa_family = AF_INET;  // Use AF_INET for IPv4 addresses
  strncpy(ifr.ifr_name, interface.c_str(), IF_NAMESIZE - 1);

  ioctl(fd, SIOCGIFADDR, &ifr);
  close(fd);

  return inet_ntoa(((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr);
}

inline int parse_interface(const char* s, int* ifindex_out) {
  char dummy;
  if ((*ifindex_out = if_nametoindex(s)) == 0) {
    if (sscanf(s, "%d%c", ifindex_out, &dummy) != 1)
      return 0;
  }
  return 1;
}

std::string uint32_to_ip(uint32_t uint_ip) {
  return std::to_string(uint_ip >> 24) + "." +
    std::to_string((uint_ip >> 16) & 0xff) + "." +
    std::to_string((uint_ip >> 8) & 0xff) + "." +
    std::to_string(uint_ip & 0xff);
}

std::string net_uint32_to_ip(uint32_t uint_ip) {
  return std::to_string(uint_ip & 0xff) + "." +
    std::to_string((uint_ip >> 8) & 0xff) + "." +
    std::to_string((uint_ip >> 16) & 0xff) + "." +
    std::to_string(uint_ip >> 24);
}

