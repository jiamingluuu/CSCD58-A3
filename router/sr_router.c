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

// [x] (Wei Zheyuan): Implements this function.
void handle_arp_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface) {
  // if the packet is an ARP request
  // - send an ARP reply if the target IP address is one of your router’s IP
  //   addresses.
  // else if the packet is an ARP response
  // - cache the entry if the target IP address is one of your router’s IP
  //   addresses

  // Separate the Ethernet header and ARP header
  struct sr_ethernet_hdr *eth_hdr = (struct sr_ethernet_hdr *)packet;
  struct sr_arp_hdr *arp_hdr = (struct sr_arp_hdr *)(packet + sizeof(struct sr_ethernet_hdr));

  // Sanity check
  if (len < sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_arp_hdr)) {
    fprintf(stderr, "ARP packet length is less than minimum required length.\n");
    return;
  }

  // Get the target IP address
  uint32_t target_ip = ntohl(arp_hdr->ar_tip);

  struct sr_if *if_walker = sr->if_list;
  // Check if target IP is one of the router's interface IP
  struct sr_if *iface = sr_get_interface(sr, interface);
  if (!iface) {
    fprintf(stderr, "Interface not found.\n");
    return;
  }
  while (if_walker != NULL) {
    if (arp_hdr->ar_op == htons(arp_op_request)) {  // Is ARP request
      if (target_ip == iface->ip) {                 // Match the target IP
        send_arp_reply(sr, arp_hdr, interface);     // Respond
      }
    } else if (arp_hdr->ar_op == htons(arp_op_reply)) {  // Is ARP response
      // Cache & send if match
      struct sr_arpreq *req = sr_arpcache_insert(&sr->cache, arp_hdr->ar_sha, arp_hdr->ar_sip);
      if (req) {  // Found in ARP request queue
        struct sr_packet *waiting_pkt = req->packets;
        while (waiting_pkt != NULL) {
          sr_send_packet(sr, waiting_pkt->buf, waiting_pkt->len, waiting_pkt->iface);
          waiting_pkt = waiting_pkt->next;
        }
        sr_arpreq_destroy(&sr->cache, req);
      }
      // No packet waiting for this ARP reply. Do nothing.
    }
    if_walker = if_walker->next;
  }
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
// [ ] This function has been implemented in `sr_arpcache.c:sr_send_icmp_t3`. Move here.
static void send_icmp_response(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface, uint8_t type,
                               uint8_t code) {
  return;
}