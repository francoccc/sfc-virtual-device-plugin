#include <chrono>
#include <iostream>

#include "lib/efvi_allocator.hpp"

#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"

void print_efvi_mac() {
  uint8_t mac[6];
  ef_vi_get_mac(vi, driver_handle, mac);
  printf("INF: mac=");
  printf(MAC_FMT, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  printf("\n");
}

int main(int argc, char** argv) {
  efvi::EfviAllocator allocator("127.0.0.1", 8088);
  allocator.run(false);
  return 0;
}