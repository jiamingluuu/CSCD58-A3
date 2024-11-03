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
                               uint8_t code);

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
  struct sr_if *dst_interface;

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
  dst_interface = get_dst_interface(sr, ip_hdr);
  if (dst_interface != NULL) {
    // TODO(Lu Jiaming): Finish the if statement block.
    if (ip_protocol_icmp == ip_hdr->ip_p) {
      // The packet is an ICMP echo request, send an ICMP echo reply to the
      // sending host.
      icmp_hdr = (sr_icmp_hdr_t *)(packet + sizeof(struct sr_ethernet_hdr) + sizeof(sr_ip_hdr_t));
      if (icmp_hdr->icmp_type == (uint8_t)8) {
        header_checksum = cksum((const void *)icmp_hdr, len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));
        if (header_checksum != icmp_hdr->icmp_sum) {
          LOG_INFO("ICMP: Wrong header checksum.");
          return;
        }
      }
    } else {
      // The packet contains a TCP or UDP payload, send an ICMP port unreachable
      // to the sending host. Otherwise, ignore the packet. Packets destined
      // elsewhere should be forwarded using your normal forwarding logic.
    }
  } else {
    // TODO(Zhang Yawen): Finish the else statement block
    // Otherwise we need to forward the packet.

    // Decrement the TTL by 1, and recompute the packet checksum over the
    // modified header.

    ip_hdr->ip_ttl--;
    if (ip_hdr->ip_ttl == 0) {
      // time out
      send_icmp_response(sr, packet, len, interface, 11, 0);
      return;
    }
    ip_hdr->ip_sum = 0;
    ip_hdr->ip_sum = cksum((const void *)ip_hdr, sizeof(sr_ip_hdr_t));

    // Find out which entry in the routing table has the longest prefix match
    // with the destination IP address.
    struct sr_rt *longest_match_rt;
    longest_match_rt = sr_longest_prefix_match(sr, ip_hdr->ip_dst);
    if (longest_match_rt == NULL) {
      // No match found, send an ICMP net unreachable message back to the sender.
      send_icmp_response(sr, packet, len, interface, 3, 0);
      return;
    }

    // Check the ARP cache for the next-hop MAC address corresponding to the
    // next-hop IP.
    struct sr_arpentry *arp_entry;
    arp_entry = sr_arpcache_lookup(&(sr->cache), longest_match_rt->gw.s_addr);
    if (arp_entry) {
      // If it’s there, forward the packet.
      sr_ethernet_hdr_t *eth_hdr = (sr_ethernet_hdr_t *)packet;
      memcpy(eth_hdr->ether_shost, sr_get_interface(sr, longest_match_rt->interface)->addr, ETHER_ADDR_LEN);
      memcpy(eth_hdr->ether_dhost, arp_entry->mac, ETHER_ADDR_LEN);

      int res = sr_send_packet(sr, packet, len, longest_match_rt->interface);
      free(arp_entry);
      if (res == -1) {
        LOG_ERROR("Failed to send packet.");
        return;
      }

    } else {
      // Otherwise, send an ARP request for
      // the next-hop IP (if one hasn’t been sent within the last second), and add
      // the packet to the queue of packets waiting on this ARP request.
      struct sr_arpreq *arp_req;
      arp_req =
          sr_arpcache_queuereq(&(sr->cache), longest_match_rt->gw.s_addr, packet, len, longest_match_rt->interface);
      sr_handle_arpreq(sr, arp_req);
    }
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

// TODO: Implements this function.
static void send_icmp_response(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface, uint8_t type,
                               uint8_t code) {
  return;
}