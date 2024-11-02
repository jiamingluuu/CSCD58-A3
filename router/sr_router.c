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
static void build_ip_header(sr_ip_hdr_t *ip_hdr, uint16_t ip_len, uint32_t src, uint32_t dst, uint8_t ip_p);
static void build_icmp_t3_header(sr_icmp_t3_hdr_t *icmp_t3_hdr, uint8_t type, uint8_t code, uint8_t *data);
static void build_icmp_header(sr_icmp_hdr_t *icmp_hdr, uint8_t type, uint8_t code, unsigned int len);

void sr_handlepacket(struct sr_instance *sr, uint8_t *packet /* lent */, unsigned int len, char *interface /* lent */) {
  uint16_t type;

  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d \n", len);
  type = ethertype(packet);
  if (type == ethertype_ip) {
    handle_ip_packet(sr, packet, len, interface);
  } else if (type == ethertype_arp) {
    handle_arp_packet(sr, packet, len, interface);
  } else {
    // Ignored.
  }
} /* end sr_ForwardPacket */

// TODO: Implements this function.
void handle_ip_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface) {
  uint16_t header_checksum;
  sr_ip_hdr_t *ip_hdr;
  sr_icmp_hdr_t *icmp_hdr;
  struct sr_if *ip_interface, *eth_interface;
  uint8_t protocol;

  // REQUIRES
  assert(sr);
  assert(packet);
  assert(interface);

  // Sanity-check the packet (meets minimum length and has correct checksum).
  if (len < sizeof(struct sr_ethernet_hdr) + sizeof(sr_ip_hdr_t)) {
    LOG_DEBUG("IP packet does not meet expected length.")
    return;
  }
  ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(struct sr_ethernet_hdr));
  header_checksum = ip_hdr->ip_sum;
  if (header_checksum != cksum((const void *)ip_hdr, sizeof(sr_ip_hdr_t))) {
    LOG_DEBUG("IP: Wrong header checksum.");
    return;
  }

  // If the packet is sending to one of our interface.
  ip_interface = get_dst_interface(sr, ip_hdr);
  if (ip_interface != NULL) {
    struct sr_if *iface = sr_get_interface(sr, interface);
    protocol = ip_protocol(packet);
    // TODO(Lu Jiaming): Finish the if statement block.
    if (protocol == ip_protocol_icmp) {
      // The packet is an ICMP echo request, send an ICMP echo reply to the
      // sending host.
      icmp_hdr = (sr_icmp_hdr_t *)(packet + sizeof(struct sr_ethernet_hdr) + sizeof(sr_ip_hdr_t));
      header_checksum = cksum(icmp_hdr, len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));
      if (header_checksum != icmp_hdr->icmp_sum) {
        LOG_INFO("ICMP: Wrong header checksum.");
        return;
      }
      if (icmp_hdr->icmp_type != (uint8_t)8) {
        LOG_INFO("Received packet is not an ICMP echo request.");
        return;
      }
      send_icmp_response(sr, packet, len, interface, 0, 0, ip_interface);
    } else if (protocol == ip_protocol_tcp || protocol == ip_protocol_udp) {
      // The packet contains a TCP or UDP payload, send an ICMP port unreachable
      // to the sending host. Otherwise, ignore the packet. Packets destined
      // elsewhere should be forwarded using your normal forwarding logic.
      send_icmp_response(sr, packet, len, interface, 3, 3, ip_interface);
    }
  } else {
    // TODO(Zhang Yawen): Finish the else statement block
    // Otherwise we need to forward the packet.

    // Decrement the TTL by 1, and recompute the packet checksum over the
    // modified header.

    // Find out which entry in the routing table has the longest prefix match
    // with the destination IP address.

    // Check the ARP cache for the next-hop MAC address corresponding to the
    // next-hop IP. If it’s there, send it. Otherwise, send an ARP request for
    // the next-hop IP (if one hasn’t been sent within the last second), and add
    // the packet to the queue of packets waiting on this ARP request.
    // Obviously, this is a very simplified version of the forwarding process,
    // and the low-level details follow. For example, if an error occurs in any
    // of the above steps, you will have to send an ICMP message back to the
    // sender notifying them of an error. You may also get an ARP request or
    // reply, which has to interact with the ARP cache correctly.
  }
}

// TODO(Wei Zheyuan): Implements this function.
void handle_arp_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface) {
  // if the packet is an ARP request
  // - send an ARP reply if the target IP address is one of your router’s IP
  //   addresses.
  // else if the packet is an ARP response
  // - cache the entry if the target IP address is one of your router’s IP
  //   addresses
  return;
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
  uint8_t *response;
  unsigned int response_len;
  sr_ip_hdr_t *request_ip_hdr, *response_ip_hdr;
  sr_ethernet_hdr_t *request_eth_hdr, *response_eth_hdr;
  sr_icmp_hdr_t *response_icmp_hdr;
  struct sr_if *out_interface;
  uint32_t ip_src;

  request_eth_hdr = (sr_ethernet_hdr_t *)packet;
  request_ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));

  if (type == 0) {
    response_len = len;
  } else {
    response_len = sizeof(sr_icmp_hdr_t) + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t);
  }

  response = (uint8_t *)calloc(1, response_len);
  response_eth_hdr = (sr_ethernet_hdr_t *)response;
  response_ip_hdr = (sr_ip_hdr_t *)(response + sizeof(sr_ethernet_hdr_t));
  response_icmp_hdr = (sr_icmp_hdr_t *)(response + sizeof(sr_ip_hdr_t) + sizeof(sr_ethernet_hdr_t));
  out_interface = sr_get_interface(sr, interface);

  // ICMP
  if (type == 0) {
    memcpy(response_icmp_hdr, (sr_icmp_hdr_t *)(packet + sizeof(sr_ip_hdr_t) + sizeof(sr_ethernet_hdr_t)),
           response_len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));
  } else {
    memcpy(response_icmp_hdr, request_ip_hdr, ICMP_DATA_SIZE);
  }

  response_icmp_hdr->icmp_code = code;
  response_icmp_hdr->icmp_type = type;
  response_icmp_hdr->icmp_sum = 0;

  if (type == 0) {
    response_icmp_hdr->icmp_sum = cksum(response_icmp_hdr, len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));
  } else {
    response_icmp_hdr->icmp_sum = cksum(response_icmp_hdr, sizeof(sr_icmp_hdr_t));
  }

  // IP
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

  // ETH
  memcpy(response_eth_hdr->ether_shost, out_interface->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
  memcpy(response_eth_hdr->ether_dhost, request_eth_hdr->ether_shost, sizeof(uint8_t) * ETHER_ADDR_LEN);
  response_eth_hdr->ether_type = htons(ethertype_ip);
  sr_send_packet(sr, response, response_len, interface);
  free(response);
}

void build_ip_header(sr_ip_hdr_t *ip_hdr, uint16_t ip_len, uint32_t src, uint32_t dst, uint8_t ip_p) {
  ip_hdr->ip_len = ip_len;
  ip_hdr->ip_src = src;
  ip_hdr->ip_dst = dst;
  if (ip_p != 3) {
    ip_hdr->ip_ttl = INIT_TTL;
  }
  ip_hdr->ip_p = ip_p;
  ip_hdr->ip_sum = 0;
  ip_hdr->ip_sum = cksum(ip_hdr, sizeof(sr_ip_hdr_t));
}

void build_icmp_header(sr_icmp_hdr_t *icmp_hdr, uint8_t type, uint8_t code, unsigned int len) {
  icmp_hdr->icmp_type = type;
  icmp_hdr->icmp_code = code;
  icmp_hdr->icmp_sum = 0;
  icmp_hdr->icmp_sum = cksum(icmp_hdr, len);
}

void build_icmp_t3_header(sr_icmp_t3_hdr_t *icmp_t3_hdr, uint8_t type, uint8_t code, uint8_t *data) {
  memcpy(icmp_t3_hdr->data, data, ICMP_DATA_SIZE);
  icmp_t3_hdr->icmp_type = type;
  icmp_t3_hdr->icmp_code = code;
  icmp_t3_hdr->icmp_sum = 0;
  icmp_t3_hdr->unused = 0;
  icmp_t3_hdr->next_mtu = 0;
  icmp_t3_hdr->icmp_sum = cksum(icmp_t3_hdr, sizeof(sr_icmp_t3_hdr_t));
}