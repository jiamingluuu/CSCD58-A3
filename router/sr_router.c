/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include "sr_router.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "sr_arpcache.h"
#include "sr_if.h"
#include "sr_protocol.h"
#include "sr_rt.h"
#include "sr_utils.h"

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance *sr) {
  /* REQUIRES */
  assert(sr);

  /* Initialize cache and cache cleanup thread */
  sr_arpcache_init(&(sr->cache));

  pthread_attr_init(&(sr->attr));
  pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
  pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
  pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
  pthread_t thread;

  pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

  /* Add initialization code here! */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

static void handle_ip_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface);
static void handle_arp_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface);
static struct sr_if *get_dst_interface(const struct sr_instance *sr, const sr_ip_hdr_t *ip_hdr);
static void send_icmp_response(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface, uint8_t type,
                               uint8_t code, struct sr_if *dst_interface);

void sr_handlepacket(struct sr_instance *sr, uint8_t *packet /* lent */, unsigned int len, char *interface /* lent */) {
  uint16_t type;

  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d, pointer: %p\n", len, packet);
  type = ethertype(packet);
  printf("Received packet type: %d\n", type);
  if (type == ethertype_ip) {
    printf("Received packet is an IP packet.\n");
    handle_ip_packet(sr, packet, len, interface);
  } else if (type == ethertype_arp) {
    printf("Received packet is an ARP packet.\n");
    handle_arp_packet(sr, packet, len, interface);
  } else {
    /* Ignored. */
    printf("Received packet is not an IP or ARP packet.\n");
  }
  printf("Packet handled.\n");
} /* end sr_ForwardPacket */

/* TODO: Implements this function. */
void handle_ip_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface) {
  uint16_t header_checksum;
  sr_ip_hdr_t *ip_hdr;
  sr_icmp_hdr_t *icmp_hdr;
  struct sr_if *ip_interface, *eth_interface;
  uint8_t protocol;

  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  /* Sanity-check the packet (meets minimum length and has correct checksum). */
  if (len < sizeof(struct sr_ethernet_hdr) + sizeof(sr_ip_hdr_t)) {
    printf("IP packet does not meet expected length.\n");
    return;
  }
  ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(struct sr_ethernet_hdr));
  header_checksum = ip_hdr->ip_sum;
  ip_hdr->ip_sum = 0;
  if (header_checksum != cksum((const void *)ip_hdr, sizeof(sr_ip_hdr_t))) {
    printf("IP: Wrong header checksum.\n");
    return;
  }

  /* If the packet is sending to one of our interface. */
  ip_interface = get_dst_interface(sr, ip_hdr);
  printf("#####################\n");
  if (ip_interface != NULL) {
    struct sr_if *iface = sr_get_interface(sr, interface);
    protocol = ip_protocol(packet);
    printf("Iface found, protocol: %d\n", protocol);
    /* TODO(Lu Jiaming): Finish the if statement block. */
    if (protocol == ip_protocol_icmp) {
      /* The packet is an ICMP echo request, send an ICMP echo reply to the
        sending host.
       */
      printf("protocol is ICMP\n");
      icmp_hdr = (sr_icmp_hdr_t *)(packet + sizeof(struct sr_ethernet_hdr) + sizeof(sr_ip_hdr_t));
      header_checksum = cksum(icmp_hdr, len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));

      if (header_checksum != icmp_hdr->icmp_sum) {
        printf("ICMP: Wrong header checksum.\n");
        return;
      }
      if (icmp_hdr->icmp_type != (uint8_t)8) {
        printf("Received packet is not an ICMP echo request.\n");
        return;
      }
      send_icmp_response(sr, packet, len, interface, 0, 0, ip_interface);
    } else if (protocol == ip_protocol_tcp || protocol == ip_protocol_udp) {
      /*
        The packet contains a TCP or UDP payload, send an ICMP port unreachable
        to the sending host. Otherwise, ignore the packet. Packets destined
        elsewhere should be forwarded using your normal forwarding logic.
        send_icmp_response(sr, packet, len, interface, 3, 3, ip_interface);
      */
      printf("protocol is TCP or UDP\n");
      printf("sending type 3 code 3\n");
      send_icmp_response(sr, packet, len, interface, 3, 3, ip_interface);
    }
  } else {
    /*
      [x] Finish the else statement block
      Otherwise we need to forward the packet.
      Decrement the TTL by 1, and recompute the packet checksum over the
      modified header.
     */
    printf("Iface not found\n");
    printf("Decrementing TTL by 1.\n");
    ip_hdr->ip_ttl--;
    if (ip_hdr->ip_ttl == 0) {
      /* time out */
      send_icmp_response(sr, packet, len, interface, 11, 0, ip_interface);
      return;
    }
    ip_hdr->ip_sum = 0;
    ip_hdr->ip_sum = cksum((const void *)ip_hdr, sizeof(sr_ip_hdr_t));

    /*
      Find out which entry in the routing table has the longest prefix match
      with the destination IP address.
    */
    printf("Finding the longest prefix match.\n");
    struct sr_rt *longest_match_rt;
    longest_match_rt = sr_longest_prefix_match(sr, ip_hdr->ip_dst);
    if (longest_match_rt == NULL) {
      /* No match found, send an ICMP net unreachable message back to the
       * sender. */
      send_icmp_response(sr, packet, len, interface, 3, 0, ip_interface);
      return;
    }

    /*
      Check the ARP cache for the next-hop MAC address corresponding to the
      next-hop IP.
    */
    printf("Checking the ARP cache.\n");
    struct sr_arpentry *arp_entry;
    arp_entry = sr_arpcache_lookup(&(sr->cache), longest_match_rt->gw.s_addr);
    if (arp_entry) {
      /* If it’s there, forward the packet. */
      printf("ARP entry found. Forward the packet.\n");
      sr_ethernet_hdr_t *eth_hdr = (sr_ethernet_hdr_t *)packet;
      memcpy(eth_hdr->ether_shost, sr_get_interface(sr, longest_match_rt->interface)->addr, ETHER_ADDR_LEN);
      memcpy(eth_hdr->ether_dhost, arp_entry->mac, ETHER_ADDR_LEN);

      int res = sr_send_packet(sr, packet, len, longest_match_rt->interface);
      printf("## start free\n");
      free(arp_entry);
      if (res == -1) {
        printf("Failed to send packet.\n");
        return;
      }

    } else {
      /*
        Otherwise, send an ARP request for
        the next-hop IP (if one hasn’t been sent within the last second), and
        add the packet to the queue of packets waiting on this ARP request.
      */
      printf("ARP entry not found. Send an ARP request.\n");
      struct sr_arpreq *arp_req;
      arp_req =
          sr_arpcache_queuereq(&(sr->cache), longest_match_rt->gw.s_addr, packet, len, longest_match_rt->interface);
      handle_arpreq(sr, arp_req);
    }
  }
}

/* [x] (Wei Zheyuan): Implements this function. */
void handle_arp_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface) {
  uint8_t *response;
  struct sr_if *iface;
  struct sr_packet *arp_reply_packet;
  sr_ethernet_hdr_t *packet_eth_hdr, *request_eth_hdr, *response_eth_hdr;
  sr_arp_hdr_t *packet_arp_hdr, *response_arp_hdr;

  iface = sr_get_interface(sr, interface);
  packet_eth_hdr = (sr_ethernet_hdr_t *)packet;
  packet_arp_hdr = (sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));

  if (ntohs(packet_arp_hdr->ar_op) == arp_op_request) {
    printf("#### Handling ARP request\n");
    if (packet_arp_hdr->ar_tip == iface->ip) {
      response = malloc(len);
      memset(response, 0, len);
      response_eth_hdr = (sr_ethernet_hdr_t *)response;
      response_arp_hdr = (sr_arp_hdr_t *)(response + sizeof(sr_ethernet_hdr_t));

      /* ETH */
      memcpy(response_eth_hdr->ether_shost, iface->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
      memcpy(response_eth_hdr->ether_dhost, packet_eth_hdr->ether_shost, sizeof(uint8_t) * ETHER_ADDR_LEN);
      response_eth_hdr->ether_type = htons(ethertype_arp);
      memcpy(response_eth_hdr->ether_dhost, packet_eth_hdr->ether_shost, sizeof(uint8_t) * ETHER_ADDR_LEN);
      response_eth_hdr->ether_type = htons(ethertype_arp);

      /* ARP */
      memcpy(response_arp_hdr, packet_arp_hdr, sizeof(sr_arp_hdr_t));
      response_arp_hdr->ar_op = htons(arp_op_reply);
      memcpy(response_arp_hdr->ar_tha, packet_arp_hdr->ar_sha, ETHER_ADDR_LEN);
      memcpy(response_arp_hdr->ar_sha, iface->addr, ETHER_ADDR_LEN);
      response_arp_hdr->ar_sip = iface->ip;
      response_arp_hdr->ar_tip = packet_arp_hdr->ar_sip;

      sr_send_packet(sr, response, len, interface);
      free(response);
    }
  } else if (ntohs(packet_arp_hdr->ar_op) == arp_op_reply) {
    printf("#### Handling ARP reply\n");

    struct sr_arpreq *cached_arp_req = sr_arpcache_insert(&(sr->cache), packet_arp_hdr->ar_sha, packet_arp_hdr->ar_sip);
    if (cached_arp_req) {
      arp_reply_packet = cached_arp_req->packets;

      while (arp_reply_packet) {
        uint8_t *send_packet = malloc(arp_reply_packet->len);
        memcpy(send_packet, arp_reply_packet->buf, arp_reply_packet->len);

        response_eth_hdr = (sr_ethernet_hdr_t *)send_packet;
        memcpy(response_eth_hdr->ether_dhost, packet_arp_hdr->ar_sha, ETHER_ADDR_LEN);
        memcpy(response_eth_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);

        sr_send_packet(sr, send_packet, arp_reply_packet->len, arp_reply_packet->iface);
        free(send_packet);

        arp_reply_packet = arp_reply_packet->next;
      }
      sr_arpreq_destroy(&(sr->cache), cached_arp_req);
      /* sr_arpcache_destroy(&(sr->cache)); */
    }
  }
}

/*
  [x] send_arp_reply
  @param sr the router instance
  @param arp_hdr the ARP header
  @param interface the interface to send the ARP reply
*/
void send_arp_reply(struct sr_instance *sr, sr_arp_hdr_t *arp_hdr, char *interface) {
  printf("Sending ARP reply.\n");

  /* Allocate memory for the ARP reply */
  unsigned int arp_reply_len = sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_arp_hdr);
  uint8_t *arp_reply = (uint8_t *)malloc(arp_reply_len);

  /* Cache sr_get_interface(sr, interface) */
  struct sr_if *iface = sr_get_interface(sr, interface);
  if (!iface) {
    fprintf(stderr, "Interface not found.\n");
    free(arp_reply);
    return;
  }

  /* Fill Ethernet Header */
  struct sr_ethernet_hdr *eth_hdr = (struct sr_ethernet_hdr *)arp_reply;
  memcpy(eth_hdr->ether_dhost, arp_hdr->ar_sha, ETHER_ADDR_LEN);
  memcpy(eth_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);
  eth_hdr->ether_type = htons(ethertype_arp);

  /* Fill ARP Header */
  struct sr_arp_hdr *new_arp_hdr = (struct sr_arp_hdr *)(arp_reply + sizeof(struct sr_ethernet_hdr));
  new_arp_hdr->ar_hrd = htons(arp_hrd_ethernet);
  new_arp_hdr->ar_pro = htons(ethertype_ip);
  new_arp_hdr->ar_hln = ETHER_ADDR_LEN;
  new_arp_hdr->ar_pln = sizeof(uint32_t);
  new_arp_hdr->ar_op = htons(arp_op_reply);
  memcpy(new_arp_hdr->ar_sha, iface->addr, ETHER_ADDR_LEN);
  new_arp_hdr->ar_sip = iface->ip;
  memcpy(new_arp_hdr->ar_tha, arp_hdr->ar_sha, ETHER_ADDR_LEN);
  new_arp_hdr->ar_tip = arp_hdr->ar_sip;

  /* Send the ARP reply */
  sr_send_packet(sr, arp_reply, arp_reply_len, interface);

  /* Free the memory */
  free(arp_reply);

  printf("ARP reply sent.\n");
}

struct sr_if *get_dst_interface(const struct sr_instance *sr, const sr_ip_hdr_t *ip_hdr) {
  struct sr_if *if_walker = sr->if_list;
  while (if_walker != NULL) {
    if (if_walker->ip == ip_hdr->ip_dst) {
      return if_walker;
    }
    if_walker = if_walker->next;
  }
  return NULL;
}

static void send_icmp_response(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface, uint8_t type,
                               uint8_t code, struct sr_if *ip_interface) {
  printf("## sending icmp response\n");
  uint8_t *response;
  unsigned int response_len;
  sr_ip_hdr_t *request_ip_hdr, *response_ip_hdr;
  sr_ethernet_hdr_t *request_eth_hdr, *response_eth_hdr;
  sr_icmp_t3_hdr_t *response_icmp_hdr;
  struct sr_if *out_interface;
  uint32_t ip_src;

  /* Sanity-check the packet (meets minimum length). */
  if (len < sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t)) {
    fprintf(stderr, "Error: Packet length is too small for IP header.\n");
    return;
  }
  printf("Size of sr_ethernet_hdr_t: %zu\n", sizeof(sr_ethernet_hdr_t));
  printf("Size of sr_ip_hdr_t: %zu\n", sizeof(sr_ip_hdr_t));
  printf("Size of sr_icmp_t3_hdr_t: %zu\n", sizeof(sr_icmp_t3_hdr_t));
  printf("Offset of data in sr_icmp_t3_hdr_t: %zu\n", offsetof(sr_icmp_t3_hdr_t, data));

  request_eth_hdr = (sr_ethernet_hdr_t *)packet;
  request_ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));

  if (type == 0) {
    response_len = len;
  } else {
    response_len = sizeof(sr_icmp_t3_hdr_t) + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t);
  }

  printf("response_len: %d\n", response_len);

  response = (uint8_t *)calloc(1, response_len);

  printf("## getting iface\n");
  out_interface = sr_get_interface(sr, interface);
  printf("## sending icmp response.type: %d\n", type);
  /* ICMP */
  if (type == 0) {
    /* Fix: use `sr_icmp_hdr_t` in type 0 */
    sr_icmp_hdr_t *response_icmp_hdr = (sr_icmp_hdr_t *)(response + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    memcpy(response_icmp_hdr, packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t),
           response_len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));
    response_icmp_hdr->icmp_type = type;
    response_icmp_hdr->icmp_code = code;
    response_icmp_hdr->icmp_sum = 0;
    response_icmp_hdr->icmp_sum =
        cksum(response_icmp_hdr, response_len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));
  } else {
    sr_icmp_t3_hdr_t *response_icmp_hdr =
        (sr_icmp_t3_hdr_t *)(response + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    response_icmp_hdr->next_mtu = 0;
    response_icmp_hdr->icmp_type = type;
    response_icmp_hdr->icmp_code = code;
    response_icmp_hdr->unused = 0;

    memcpy(response_icmp_hdr->data, request_ip_hdr, ICMP_DATA_SIZE);
    response_icmp_hdr->icmp_sum = 0;
    response_icmp_hdr->icmp_sum = cksum(response_icmp_hdr, sizeof(sr_icmp_t3_hdr_t));
  }
  printf("response_icmp_hdr icmp_sum: %d\n", response_icmp_hdr->icmp_sum);

  /* IP */
  printf("## copying ip header\n");
  if (ip_interface == NULL) {
    ip_src = out_interface->ip;
  } else {
    ip_src = ip_interface->ip;
  }
  memcpy(response_ip_hdr, request_ip_hdr, sizeof(sr_ip_hdr_t));
  response_ip_hdr->ip_ttl = INIT_TTL;
  response_ip_hdr->ip_p = ip_protocol_icmp;
  response_ip_hdr->ip_src = ip_src;
  response_ip_hdr->ip_dst = request_ip_hdr->ip_src;
  response_ip_hdr->ip_len = htons(response_len - sizeof(sr_ethernet_hdr_t));
  response_ip_hdr->ip_sum = 0;
  response_ip_hdr->ip_sum = cksum(response_ip_hdr, sizeof(sr_ip_hdr_t));

  /* ETH */
  printf("## copying eth header\n");
  memcpy(response_eth_hdr->ether_shost, out_interface->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
  memcpy(response_eth_hdr->ether_dhost, request_eth_hdr->ether_shost, sizeof(uint8_t) * ETHER_ADDR_LEN);
  response_eth_hdr->ether_type = htons(ethertype_ip);

  printf("## sending packet\n");
  sr_send_packet(sr, response, response_len, interface);

  printf("## free\n");
  free(response);
}