#pragma once  

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/param.h>

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