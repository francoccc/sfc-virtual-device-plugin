
#include <ci/tools.h>
#include <ci/tools/ippacket.h>

static uint32_t laddr_he = 0xac108564;  /* 172.16.133.100 */
static uint32_t raddr_he = 0xac010203;  /* 172.1.2.3 */
static uint32_t port_he = 8080;

static void init_udp_pkt(void* pkt_buf, int paylen) {
  int ip_len = sizeof(ci_ip4_hdr) + sizeof(ci_udp_hdr) + paylen;
  ci_ether_hdr* eth;
  ci_ip4_hdr* ip4;
  ci_udp_hdr* udp;
  struct iovec iov;

  const uint8_t remote_mac[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

  eth = (ci_ether_hdr*) pkt_buf;
  ip4 = (ci_ip4_hdr*) ((char*) eth + 14);
  udp = (ci_udp_hdr*) (ip4 + 1);

  memcpy(eth->ether_dhost, remote_mac, sizeof(remote_mac));
  eth->ether_type = htons(0x0800);
  ci_ip4_hdr_init(ip4, CI_NO_OPTS, ip_len, 0, IPPROTO_UDP, (unsigned) htons(laddr_he),
    (unsigned) htons(raddr_he), 0);
  ci_udp_hdr_init(udp, ip4, htons(port_he), htons(port_he), udp + 1, paylen, 0);

  iov.iov_base = udp + 1;
  iov.iov_len = paylen;
}


int main(int argc, char **argv) {
  return 0;
}