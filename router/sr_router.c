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
  if (type == ntohs(ethertype_ip)) {
    LOG_DEBUG("Received packet is an IP packet.");
    handle_ip_packet(sr, packet, len, interface);
  } else if (type == ntohs(ethertype_arp)) {
    LOG_DEBUG("Received packet is an ARP packet.");
    handle_arp_packet(sr, packet, len, interface);
  } else {
    /* Ignored. */
    LOG_DEBUG("Received packet is not an IP or ARP packet.");
  }
  LOG_DEBUG("Packet handled.");
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
    LOG_DEBUG("IP packet does not meet expected length.")
    return;
  }
  ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(struct sr_ethernet_hdr));
  header_checksum = ip_hdr->ip_sum;
  if (header_checksum != cksum((const void *)ip_hdr, sizeof(sr_ip_hdr_t))) {
    LOG_DEBUG("IP: Wrong header checksum.");
    return;
  }

  /* If the packet is sending to one of our interface. */
  ip_interface = get_dst_interface(sr, ip_hdr);
  if (ip_interface != NULL) {
    struct sr_if *iface = sr_get_interface(sr, interface);
    protocol = ip_protocol(packet);
    /* TODO(Lu Jiaming): Finish the if statement block. */
    if (protocol == ip_protocol_icmp) {
      /* The packet is an ICMP echo request, send an ICMP echo reply to the
        sending host.
       */
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
      /*
        The packet contains a TCP or UDP payload, send an ICMP port unreachable
        to the sending host. Otherwise, ignore the packet. Packets destined
        elsewhere should be forwarded using your normal forwarding logic.
        send_icmp_response(sr, packet, len, interface, 3, 3, ip_interface);
      */
      send_icmp_response(sr, packet, len, interface, 3, 3, ip_interface);
    }
  } else {
    /*
      [x] Finish the else statement block
      Otherwise we need to forward the packet.
      Decrement the TTL by 1, and recompute the packet checksum over the
      modified header.
     */
    LOG_INFO("Decrementing TTL by 1.");
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
    LOG_DEBUG("Finding the longest prefix match.");
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
    LOG_DEBUG("Checking the ARP cache.");
    struct sr_arpentry *arp_entry;
    arp_entry = sr_arpcache_lookup(&(sr->cache), longest_match_rt->gw.s_addr);
    if (arp_entry) {
      /* If it’s there, forward the packet. */
      LOG_DEBUG("ARP entry found. Forward the packet.");
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
      /*
        Otherwise, send an ARP request for
        the next-hop IP (if one hasn’t been sent within the last second), and
        add the packet to the queue of packets waiting on this ARP request.
      */
      LOG_DEBUG("ARP entry not found. Send an ARP request.");
      struct sr_arpreq *arp_req;
      arp_req =
          sr_arpcache_queuereq(&(sr->cache), longest_match_rt->gw.s_addr, packet, len, longest_match_rt->interface);
      handle_arpreq(sr, arp_req);
    }
  }
}

/* [x] (Wei Zheyuan): Implements this function. */
void handle_arp_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface) {
  /*
    if the packet is an ARP request
    - send an ARP reply if the target IP address is one of your router’s IP
      addresses.
    else if the packet is an ARP response
    - cache the entry if the target IP address is one of your router’s IP
      addresses
  */

  /* Separate the Ethernet header and ARP header */
  struct sr_ethernet_hdr *eth_hdr = (struct sr_ethernet_hdr *)packet;
  struct sr_arp_hdr *arp_hdr = (struct sr_arp_hdr *)(packet + sizeof(struct sr_ethernet_hdr));

  /* Sanity check */
  if (len < sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_arp_hdr)) {
    LOG_DEBUG("ARP packet length is less than minimum required length.");
    return;
  }

  /* Get the target IP address */
  uint32_t target_ip = ntohl(arp_hdr->ar_tip);

  struct sr_if *if_walker = sr->if_list;
  /* Check if target IP is one of the router's interface IP */
  struct sr_if *iface = sr_get_interface(sr, interface);
  if (!iface) {
    LOG_DEBUG("Interface not found.");
    return;
  }
  while (if_walker != NULL) {
    if (arp_hdr->ar_op == htons(arp_op_request)) { /* Is ARP request */
      LOG_DEBUG("Received ARP request.");
      if (target_ip == iface->ip) {             /* Match the target IP */
        send_arp_reply(sr, arp_hdr, interface); /* Respond */
      }
    } else if (arp_hdr->ar_op == htons(arp_op_reply)) { /* Is ARP response */
      LOG_DEBUG("Received ARP reply.");
      /* Cache & send if match */
      struct sr_arpreq *req = sr_arpcache_insert(&sr->cache, arp_hdr->ar_sha, arp_hdr->ar_sip);
      if (req) { /* Found in ARP request queue */
        struct sr_packet *waiting_pkt = req->packets;
        while (waiting_pkt != NULL) {
          sr_send_packet(sr, waiting_pkt->buf, waiting_pkt->len, waiting_pkt->iface);
          waiting_pkt = waiting_pkt->next;
        }
        sr_arpreq_destroy(&sr->cache, req);
      }
      /* No packet waiting for this ARP reply. Do nothing. */
    }
    if_walker = if_walker->next;
  }
}

/*
  [x] send_arp_reply
  @param sr the router instance
  @param arp_hdr the ARP header
  @param interface the interface to send the ARP reply
*/
void send_arp_reply(struct sr_instance *sr, sr_arp_hdr_t *arp_hdr, char *interface) {
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

  /* ICMP */
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

  /* IP */
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