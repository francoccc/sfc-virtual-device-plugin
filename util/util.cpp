/*
 * @Descripition: Utility
 * @Author: Franco Chen
 * @Date: 2022-07-06 16:58:26
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-07-14 14:59:15
 */
#include "util.hpp"

#include <cerrno>

#define SHARE_MEMO_MOUNT_PATH "/dev/shm/"
#define HUGE_PAGES_MOUNT_PATH "/mnt/hugepages/"

void print_hex(const unsigned char *data, size_t len) {
  printf("packet len:%d\n", len);
  for (int i = 0; i < len; i++) {
    printf("%02X ", data[i]);
  }
  printf("\n");
}

int64_t get_high_resolution_time() {
  static timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return tp.tv_nsec + tp.tv_sec * 1000000000LLU;
}

void mmap_file(std::string file_path, size_t size, void** out) {
  int fd = open(file_path.c_str(), O_RDWR, 0666);
  if (fd < 0) {
    throw std::runtime_error("can't open mmapped file");
  }

  void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  *out = p;
}

void mmap_trim_file(int fd, size_t size, void** out, int mount_type) {
  if (0 != ftruncate64(fd, size)) {
    throw std::runtime_error("can't ftruncate file size");
  }
  if (mount_type == MOUNT_HUGE_PAGE) {
    *out = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE | MAP_HUGETLB, fd, 0);
  } else if (mount_type == MOUNT_DEFAULT) {
    *out = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  } else {
    throw std::runtime_error("unknown type error");
  }
  close(fd);
}

void alloc_shm_resource(std::string dir, std::string object_name, size_t size, int mount_type, void** out) {
  std::string path;
  if (mount_type == MOUNT_DEFAULT) {
    path = SHARE_MEMO_MOUNT_PATH + dir;
  } else if (mount_type == MOUNT_HUGE_PAGE) {
    path = HUGE_PAGES_MOUNT_PATH + dir;
  }
  if (0 != access(path.c_str(), F_OK)) {
    if (0 != mkdir(path.c_str(), 0755)) {
      throw std::runtime_error("failed to mkdir rc:" + std::string(strerror(errno)));
    }
  }
  path += "/" + object_name;
  int fd = open(path.c_str(), O_RDWR | O_CREAT, 0666);
  if (fd < 0) {
    throw std::runtime_error("can't open or create file");
  }
  mmap_trim_file(fd, size, out, mount_type);
}