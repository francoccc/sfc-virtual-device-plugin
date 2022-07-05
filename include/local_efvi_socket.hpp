/*
 * @Descripition: Local efvi socket include
 * @Author: Franco Chen
 * @Date: 2022-07-04 16:58:16
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-07-05 14:45:20
 */
#pragma once

#include <string>
#include <cstdint>

namespace efvi {

class VEfviSocket {
public:
  VEfviSocket(std::string local_addr, uint16_t local_port,
    std::string remote_addr, uint16_t remote_port,
    uint8_t local_mac[], uint8_t remote_mac[]);
  virtual ~VEfviSocket();
  bool connect();
  bool close();
  int32_t send(const char *buff, int32_t buff_len);

private:
  void remap_kernel_mem();
  bool del_efvi_filter();

private:
  uint16_t local_port, remote_port;
  uint32_t local_addr_he, remote_addr_he;
  std::string local_addr, remote_addr;
  uint8_t local_mac[8], remote_mac[8];
  
  void *p_vi {0};
  void *p_queue {0};
  void *p_filter_cookie {0};
  int driver_handle;      // driver: sfc_char
};

} // namespace efvi