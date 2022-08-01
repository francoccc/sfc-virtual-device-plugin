/*
 * @Descripition: Efvi virtual interface allocator library
 * @Author: Franco Chen
 * @Date: 2022-07-04 13:11:32
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-07-25 10:29:50
 */
#pragma once

#include <grpcpp/grpcpp.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <stdint.h>

#include "proto/efvi.grpc.pb.h"
#include "efvi_global.hpp"

namespace efvi {

class VirtualInterface {
public:
  VirtualInterface(ef_driver_handle driver_handle, ef_pd pd, std::string vi_name, int ifindex);
  ~VirtualInterface();
  
  bool is_use_hugepages() {
    return use_hugepages;
  }
  
  std::string name() {
    return vi_name;
  }
private:
  bool use_hugepages {false};
  ef_memreg memreg;
  std::string vi_name;
  ef_vi *vi;
  pkt_buf *queue;
};

typedef struct efvi_nic_handle_t {
  int ifindex;
  bool initialized;
  ef_pd protection_domain;
  std::unordered_map<std::string, std::shared_ptr<VirtualInterface>> vis;
} efvi_nic_handle;

class EfviAllocContext {
public:
  EfviAllocContext();
  void put_if_absent(int ifindex);
  std::shared_ptr<VirtualInterface> alloc_virtual_interface(int ifindex, std::string vi_name);
private:
  std::unordered_map<int, efvi_nic_handle> opened_handles;
  int driver_handle;
};

class EfviAllocService final : public efvi::EfviService::Service {
public:
  
  grpc::Status ApplyVirtualDevice(grpc::ServerContext* context,
    const efvi::ApplyRequest* request, efvi::ViResource* response) override;
private:
  EfviAllocContext context;
};

class EfviAllocator {
public:
  EfviAllocator(std::string host, uint16_t port);

  void run(bool async = true);
private:
  EfviAllocService service;
  std::string host;
  uint16_t port;
};

} // namespace efvi