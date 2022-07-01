/*
 * @Descripition: 
 * @Author: Franco Chen
 * @Date: 2022-05-09 14:34:57
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-07-01 16:20:21
 */
#pragma once


#include "etherfabric/vi.h"
#include "etherfabric/pd.h"
#include "etherfabric/pio.h"
#include "etherfabric/memreg.h"
#include "etherfabric/checksum.h"
#include "etherfabric/capabilities.h"



static ef_driver_handle driver_handle;
static ef_pd pd;
static bool efvi_context_initialized = false;
static ef_vi* vi;
static ef_memreg memreg;
static ef_filter_cookie filter_cookie_out;

static unsigned long vi_recv_prefix;
static unsigned long post_index;
static unsigned long rcv_index;
static unsigned long deal_index;

static uint32_t laddr_he = 0xac108564;  /* 172.16.133.100 */
static uint32_t raddr_he = 0xac010203;  /* 172.1.2.3 */
static uint32_t port_he = 8080;

static int tx_frame_len;

struct pkt_buf {
  struct pkt_buf* next;
  ef_addr dma_buf_addr;
  int id;
  unsigned dma_buf[1] EF_VI_ALIGN(EF_VI_DMA_ALIGN);
};

enum mode {
  MODE_DMA = 1,
  MODE_PIO = 2,
  MODE_ALT = 4,
  MODE_CTPIO = 8,
  MODE_DEFAULT = MODE_CTPIO | MODE_ALT | MODE_PIO | MODE_DMA
};

#define N_RX_BUFS	256u
#define N_TX_BUFS	1u
#define N_BUFS          (N_RX_BUFS + N_TX_BUFS)
#define FIRST_TX_BUF    N_RX_BUFS
#define BUF_SIZE        2048

#define HEADER_SIZE     (14 + 20 + 8) // for test

struct pkt_buf*          pkt_bufs[N_RX_BUFS + N_TX_BUFS];