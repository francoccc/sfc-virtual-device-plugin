/*
 * @Descripition: 
 * @Author: Franco Chen
 * @Date: 2022-06-22 13:48:41
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-06-22 13:55:12
 */
#pragma once
#include <stdint.h>
#include <net/ethernet.h>
 
typedef struct ip_header_t {
  uint8_t ip_ihl_version;
  uint8_t ip_tos;
  uint8_t ip_tot_len_be16;
  uint16_t  ip_id_be16;
  uint16_t  ip_frag_off_be16;
  uint8_t   ip_ttl;
  uint8_t   ip_protocol;
  uint16_t  ip_check_be16;
  uint32_t  ip_saddr_be32;
  uint32_t  ip_daddr_be32;
} ip_header;

typedef struct udp_header_t {
  uint16_t udp_source_be16;
  uint16_t udp_dest_be16;
  uint16_t udp_len_be16;
  uint16_t udp_check_be16;
} udp_header;