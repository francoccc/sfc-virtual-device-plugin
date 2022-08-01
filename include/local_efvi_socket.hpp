/*
 * @Descripition: Local efvi socket include
 * @Author: Franco Chen
 * @Date: 2022-07-04 16:58:16
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-08-01 13:25:59
 */
#pragma once

#include <string>
#include <cstdint>
#include <functional>

namespace efvi {

class MODE {};
class CTPIO_MODE : public MODE {};

class VEfviSocket {
public:
  VEfviSocket(std::string local_addr, uint16_t local_port,
    std::string remote_addr, uint16_t remote_port,
    uint8_t local_mac[], uint8_t remote_mac[]);
  virtual ~VEfviSocket();
  bool connect();
  bool close();
  
  int32_t VEfviSocket::send(const char *buff, int32_t buff_len);
  
  void loop_recv(std::function<void(char *buff, int32_t buff_len)> message_handler,
                 std::function<bool(uint32_t& recv_index, uint32_t event_rq_index)> error_handler = NULL,
                 std::function<bool()> end = []() -> bool { return true; });

  int post_buf();
  void wait_event();
  const char *fetch_message(); // after wait_event;
private:
  void remap_kernel_mem();
  void remap_vi();
  bool del_efvi_filter();
  
  template<typename _MODE>
  int32_t compound_send(const char *buff, int32_t buff_len, _MODE _) {
    return -1;
  }

private:
  uint16_t local_port, remote_port;
  uint32_t local_addr_he, remote_addr_he;
  std::string local_addr, remote_addr;
  uint8_t local_mac[8], remote_mac[8];
  volatile bool is_remapped {false};
  
  void *p_vi {0};
  void *p_queue {0};
  void *p_filter_cookie {0};
  int driver_handle;      // driver: sfc_char
  
  std::function<int32_t(const char *, int32_t)> _sender;
  
  uint32_t post_index;
  uint32_t recv_index {0};
  uint32_t posted;
};

} // namespace efvi