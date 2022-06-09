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