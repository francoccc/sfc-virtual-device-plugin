#include "efvi_allocator.hpp"
#include <exception>
#include <iostream>
#include <fstream>
#include <sstream>
#include <functional>
#include <thread>
#include <sys/shm.h>
#include "util/util.hpp"
#include "util/net_util.hpp"


using namespace efvi;

VirtualInterface::VirtualInterface(ef_driver_handle driver_handle,
  ef_pd pd, std::string _vi_name, int ifindex) : vi_name(_vi_name) {
  int rc;
  int vi_flags = EF_VI_FLAGS_DEFAULT;
  unsigned long capability_val;
  
  if (ef_vi_capabilities_get(driver_handle, ifindex, EF_VI_CAP_CTPIO, 
    &capability_val) == 0 && capability_val) {
    vi_flags |= EF_VI_TX_CTPIO;
  }
  
  alloc_shm_resource(vi_name, "vi", sizeof(ef_vi), MOUNT_DEFAULT, (void**) &vi);
  if ((rc = ef_vi_alloc_from_pd(vi, driver_handle, 
    &pd, driver_handle, -1, -1, -1, NULL, -1, (enum ef_vi_flags) vi_flags)) < 0) {
    if (rc == -EPERM) {
      vi_flags |= EF_VI_RX_EVENT_MERGE;
      rc = ef_vi_alloc_from_pd(vi, driver_handle,
        &pd, driver_handle, -1, -1, -1, NULL, -1, (enum ef_vi_flags) vi_flags);
    }
    if (rc < 0) {
      throw std::runtime_error("alloc ef_vi failed");
    }
  }
  
  int nr_hugepages;
  std::stringstream stream;
  std::ifstream in("/proc/sys/vm/nr_hugepages");
  stream << in.rdbuf();
  stream >> nr_hugepages;
  if (nr_hugepages > 0) {
    printf("[efvi_allocator] alloc huge pages\n");
    // key_t key = ftok("./", 777);
    // int shmget_id = shmget(key, 1024 * BUF_SIZE, SHM_HUGETLB | IPC_CREAT | 0666);
    // if (shmget_id == -1) {
    //   throw std::runtime_error("alloc shmget failed");
    // }
    alloc_shm_resource(vi_name, "queue", 1024 * BUF_SIZE, MOUNT_HUGE_PAGE, (void**) &queue);  // use a 2M hugePage
    use_hugepages = true;
    // queue = (pkt_buf*) shmat(shmget_id, 0, 0);
    // struct pkt_buf *pb;
    // for (int i = 0; i < N_BUFS; ++i) {
    //   pb = (pkt_buf*) ((unsigned char*) queue + i * BUF_SIZE);
    //   pb->id = i;
    // }
    printf("[efvi_allocator] queue ptr: %p\n", queue);
    if (ef_memreg_alloc(&memreg, driver_handle,
      &pd, driver_handle, queue, 1024 * BUF_SIZE) != 0) {
      throw std::runtime_error("alloc memreg failed rs:" + std::string(strerror(errno)));
    }
  } else {
    alloc_shm_resource(vi_name, "queue", N_BUFS * BUF_SIZE, MOUNT_DEFAULT, (void**) &queue);
    if (ef_memreg_alloc(&memreg, driver_handle,
      &pd, driver_handle, queue, N_BUFS * BUF_SIZE) != 0) {
      throw std::runtime_error("alloc memreg failed rs:" + std::string(strerror(errno)));
    }
  }

  
  struct pkt_buf *pb;
  for (int i = 0; i < N_BUFS; ++i) {
    pb = (pkt_buf*) ((unsigned char*) queue + i * BUF_SIZE);
    pb->id = i;
    pb->dma_buf_addr = ef_memreg_dma_addr(&memreg, i * BUF_SIZE);
    pb->dma_buf_addr += offsetof(struct pkt_buf, dma_buf);
  }
}

VirtualInterface::~VirtualInterface() {
  // TODO release
}

EfviAllocContext::EfviAllocContext() {
  if (ef_driver_open(&driver_handle) != 0) {
    throw std::runtime_error("unable to open sfc driver_handle");
  }
}

void EfviAllocContext::put_if_absent(int ifindex) {
  if (!opened_handles.count(ifindex)) {
    efvi_nic_handle lc_handle;
    enum ef_pd_flags pd_flags = EF_PD_DEFAULT;
    if (0 != ef_pd_alloc(&lc_handle.protection_domain, driver_handle, ifindex, pd_flags)) {
      throw std::runtime_error("unable to apply protection domain");
    }
    lc_handle.initialized = true;
    opened_handles[ifindex] = lc_handle;
  }
  return;
}

std::shared_ptr<VirtualInterface> EfviAllocContext::alloc_virtual_interface(int ifindex, std::string vi_name) {
  if (!opened_handles.count(ifindex)) {
    throw std::runtime_error("unfounded local driver_handle");
  }
  auto handle = opened_handles[ifindex];
  if (handle.vis.count(vi_name)) {
    return;
  }
  auto vi = std::make_shared<VirtualInterface>(driver_handle, handle.protection_domain, vi_name, ifindex);
  handle.vis[vi_name] = vi;
  return vi;
}

grpc::Status EfviAllocService::ApplyVirtualDevice(grpc::ServerContext*,
  const efvi::ApplyRequest *request, efvi::ViResource *response) {
  int ifindex;
  std::cout << "[efvi_allocator] efvi_interface: " << request->interface() << std::endl;
  if (!parse_interface(request->interface().c_str(), &ifindex) != 0) {
    std::cout << "[efvi_allocator] unfound" << std::endl;
    return grpc::Status(grpc::StatusCode::NOT_FOUND, "Invalid interface");
  }
  std::cout << "[efvi_allocator] ifindex: " << ifindex << std::endl;

  try {
    context.put_if_absent(ifindex);
    auto vi = context.alloc_virtual_interface(ifindex, request->name().c_str());
    response->set_code(0);
    response->set_vipath("/dev/shm/" + vi->name());
    if (vi->is_use_hugepages()) {
      response->set_queuepath("/mnt/hugepages/" + vi->name());
    }
  } catch (std::runtime_error& e) {
    std::cout << "[efvi_allocator] inner error: " << e.what() << std::endl;
    return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
  }
  return grpc::Status::OK;
}

EfviAllocator::EfviAllocator(std::string _host, uint16_t _port) : host(_host), port(_port) {}

void EfviAllocator::run(bool async) {
  std::function<void()> _run = [this] {
    std::cout << "[efvi_allocator] start_server at:" << host << ":" << port << std::endl;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(host + ":" + std::to_string(port), grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    server->Wait();
  };
  if (async) {
    std::thread t(_run);
  } else {
    _run();
  }
}
