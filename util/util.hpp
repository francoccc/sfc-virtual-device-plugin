#pragma once  

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>

#define TRY(x)                                                  \
  do {                                                          \
    int __rc = (x);                                             \
    if( __rc < 0 ) {                                            \
      fprintf(stderr, "ERROR: TRY(%s) failed\n", #x);           \
      fprintf(stderr, "ERROR: at %s:%d\n", __FILE__, __LINE__); \
      fprintf(stderr, "ERROR: rc=%d errno=%d (%s)\n",           \
              __rc, errno, strerror(errno));                    \
      abort();                                                  \
    }                                                           \
  } while( 0 )
  

void print_hex(const unsigned char *data, size_t len) {
  printf("packet len:%d\n", len);
  for(int i = 0; i < len; i++) {
    printf("%02X ", data[i]);
  }
  printf("\n");
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

void mmap_trim_file(int fd, size_t size, void** out) {
  if (0 != ftruncate64(fd, size)) {
    throw std::runtime_error("can't ftruncate file size");
  }
  void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  
  *out = p;
}

void alloc_shm_resource(std::string dir, std::string object_name, size_t size, void** out) {
  std::string path = "/dev/shm/" + dir;
  if (0 != access(dir.c_str(), F_OK)) {
    mkdir(path.c_str(), 0755);
  }
  path += "/" + object_name;
  int fd = open(path.c_str(), O_RDWR | O_CREAT, 0666);
  if (fd < 0) {
    throw std::runtime_error("can't open or create file");
  }
  mmap_trim_file(fd, size, out);
}