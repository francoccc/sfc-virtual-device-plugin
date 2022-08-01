#include "citools.h"


void* init_eth_hdr(void* pkt_buf, uint8_t shost[ETH_ALEN], uint8_t dhost[ETH_ALEN]) {
  ci_ether_hdr* eth;
  eth = (ci_ether_hdr*) pkt_buf;
  memcpy(eth->ether_shost, shost, sizeof(shost));
  memcpy(eth->ether_dhost, dhost, sizeof(dhost));
  eth->ether_type = htons(ETH_P_IP);
  return (char*) pkt_buf + sizeof(ci_ether_hdr);
}

udp_pkt init_udp_pkt(void* pkt_buf, const void* data, int paylen, 
      uint32_t laddr_he, uint32_t raddr_he,
      uint32_t sport_he, uint32_t dport_he) {
  int ip_len = sizeof(ci_ip4_hdr) + sizeof(ci_udp_hdr) + paylen;
  ci_ip4_hdr* ip4;
  ci_udp_hdr* udp;
  udp_pkt pkt;

  ip4 = (ci_ip4_hdr*) ((char*) pkt_buf + 14);
  udp = (ci_udp_hdr*) (ip4 + 1);

  ci_ip4_hdr_init(ip4, CI_NO_OPTS, ip_len, 0, IPPROTO_UDP, htonl(laddr_he),
      htonl(raddr_he), 0);
  ci_udp_hdr_init(udp, ip4, htons(sport_he), htons(dport_he), udp + 1, paylen, 0);
  
  pkt.ip4 = ip4;
  pkt.udp = udp;
  pkt.io.iov_base = udp + 1;
  pkt.io.iov_len = paylen;

  memcpy(pkt.io.iov_base, data, paylen);
  
  return pkt;
}

void checksum_udp_pkt(udp_pkt* pkt_buf) {
  pkt_buf->ip4->ip_check_be16 = ci_ip_checksum(pkt_buf->ip4);
  pkt_buf->udp->udp_check_be16 = ci_udp_checksum(pkt_buf->ip4, pkt_buf->udp, &pkt_buf->io, 1);
}