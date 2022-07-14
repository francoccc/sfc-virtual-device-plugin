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

#define MOUNT_DEFAULT   0
#define MOUNT_HUGE_PAGE 1

#ifndef TRY(x)
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
#endif
  

void print_hex(const unsigned char *data, size_t len);

int64_t get_high_resolution_time();

void mmap_file(std::string file_path, size_t size, void** out);
void mmap_trim_file(int fd, size_t size, void** out, int mount_type = MOUNT_DEFAULT);
void alloc_shm_resource(std::string dir, std::string object_name,
  size_t size, int mount_type, void** out);