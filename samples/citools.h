

#ifndef __CITOOLS_H__
#define __CITOOLS_H__

#include <ci/tools.h>
#include <ci/tools/ippacket.h>

#include "src/lib/ciul/driver_access.h"

#include <linux/if_ether.h>
#include <netinet/ip.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct tcp_pkt_t {
    ci_ip4_hdr*    ip4;
    ci_tcp_hdr*    tcp;
    struct iovec    io;
  } tcp_pkt;

  typedef struct udp_pkt_t {
    ci_ip4_hdr*    ip4;
    ci_udp_hdr*    udp;
    struct iovec    io;
  } udp_pkt;

  void* init_eth_hdr(void* pkt_buf, uint8_t shost[ETH_ALEN], uint8_t dhost[ETH_ALEN]);

  udp_pkt init_udp_pkt(void* pkt_buf, const void* data, int paylen, 
      uint32_t laddr_he, uint32_t raddr_he,
      uint32_t sport_he, uint32_t dport_he);

  void checksum_udp_pkt(udp_pkt* pkt);
#ifdef __cplusplus
}
#endif


#endif /* __CITOOLS_H__ */