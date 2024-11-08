#include "sr_arpcache.h"

#include <netinet/in.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "sr_if.h"
#include "sr_protocol.h"
#include "sr_router.h"

/* [x] sr_arpcache_sweepreqs
  This function gets called every second. For each request sent out, we keep
  checking whether we should resend an request or destroy the arp request.
  See the comments in the header file for an idea of what it should look like.
*/
void sr_arpcache_sweepreqs(struct sr_instance *sr) {
  struct sr_arpreq *req = sr->cache.requests;
  while (req != NULL) {
    handle_arpreq(sr, req);
    req = req->next;
  }
}

/* [x] handle_arpreq
@param sr the router instance
@param req the arp request
*/
void handle_arpreq(struct sr_instance *sr, struct sr_arpreq *req) {
  time_t now = time(NULL);

  /* 1s timeout */
  if (difftime(now, req->sent) >= 1.0) {
    /* 5 retries */
    if (req->times_sent >= 5) {
      struct sr_packet *pkt = req->packets;
      while (pkt != NULL) {
        sr_send_icmp_t3(sr, pkt->buf, pkt->len, pkt->iface, 3, 1);
        pkt = pkt->next;
      }
      /* destroy the request */
      sr_arpreq_destroy(&sr->cache, req);
    } else {
      /* resend the request */
      uint8_t *arp_req = create_arp_request(sr, req->ip, req->packets->iface);
      sr_send_packet(sr, arp_req, ARP_PACKET_LEN, req->packets->iface);
      free(arp_req);
      req->sent = now;
      req->times_sent++;
    }
  }
}

/* [x] create_arp_request
@param sr the router instance
@param ip the ip address of the destination
*/
uint8_t *create_arp_request(struct sr_instance *sr, uint32_t ip, const char *iface) {
  /* ARP Packet Length = Ethernet Header Length + ARP Header Length */ 
  unsigned int packet_len = ARP_PACKET_LEN;
  uint8_t *packet = (uint8_t *)malloc(packet_len);

  /* Allocate headers */
  struct sr_ethernet_hdr *eth_hdr = (struct sr_ethernet_hdr *)packet;
  struct sr_arp_hdr *arp_hdr = (struct sr_arp_hdr *)(packet + sizeof(struct sr_ethernet_hdr));

  /* Fill Ethernet Header */
  /* Broadcast MAC address */
  memset(eth_hdr->ether_dhost, 0xff, ETHER_ADDR_LEN);
  /* Source (sender) MAC address */
  memcpy(eth_hdr->ether_shost, sr_get_interface(sr, iface)->addr, ETHER_ADDR_LEN);
  /* ARP */
  eth_hdr->ether_type = htons(ethertype_arp);

  /* Fill ARP Header */
  /* Ethernet */
  arp_hdr->ar_hrd = htons(arp_hrd_ethernet);
  /* IP */
  arp_hdr->ar_pro = htons(ethertype_ip);
  /* MAC address length */
  arp_hdr->ar_hln = ETHER_ADDR_LEN;
  /*IP address length*/
  arp_hdr->ar_pln = sizeof(uint32_t);
  /* ARP Request */
  arp_hdr->ar_op = htons(arp_op_request);

  /* Source MAC and IP address */
  memcpy(arp_hdr->ar_sha, sr_get_interface(sr, iface)->addr, ETHER_ADDR_LEN);
  arp_hdr->ar_sip = sr_get_interface(sr, iface)->ip;

  /* Target MAC and IP address */
  memset(arp_hdr->ar_tha, 0x00, ETHER_ADDR_LEN);
  arp_hdr->ar_tip = ip;

  return packet;
}

/*
[x] This function implements `sr_router.c:send_icmp_response`.
[ ] Move this function to the right place.
@param sr the router instance
@param packet the packet to send
@param len the length of the packet
@param iface the interface to send the packet
@param type the type of the ICMP packet
@param code the code of the ICMP packet
 */
void sr_send_icmp_t3(struct sr_instance *sr, uint8_t *packet, unsigned int len, const char *iface, uint8_t type,
                     uint8_t code) {
  /* Get the Ethernet and IP headers */
  struct sr_ethernet_hdr *eth_hdr = (struct sr_ethernet_hdr *)packet;
  struct sr_ip_hdr *ip_hdr = (struct sr_ip_hdr *)(packet + sizeof(struct sr_ethernet_hdr));

  /* Allocate memory for the ICMP packet */
  unsigned int icmp_packet_len =
      sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_ip_hdr) + sizeof(struct sr_icmp_t3_hdr);
  uint8_t *icmp_packet = (uint8_t *)malloc(icmp_packet_len);

  /* Fill Ethernet Header */
  struct sr_ethernet_hdr *new_eth_hdr = (struct sr_ethernet_hdr *)icmp_packet;
  memcpy(new_eth_hdr->ether_dhost, eth_hdr->ether_shost, ETHER_ADDR_LEN);
  memcpy(new_eth_hdr->ether_shost, sr_get_interface(sr, iface)->addr, ETHER_ADDR_LEN);
  new_eth_hdr->ether_type = htons(ethertype_ip);

  /* Fill IP Header */
  struct sr_ip_hdr *new_ip_hdr = (struct sr_ip_hdr *)(icmp_packet + sizeof(struct sr_ethernet_hdr));
  new_ip_hdr->ip_v = 4;
  new_ip_hdr->ip_hl = sizeof(struct sr_ip_hdr) / 4;
  new_ip_hdr->ip_tos = 0;
  new_ip_hdr->ip_len = htons(sizeof(struct sr_ip_hdr) + sizeof(struct sr_icmp_t3_hdr));
  new_ip_hdr->ip_id = 0;
  new_ip_hdr->ip_off = htons(IP_DF);
  new_ip_hdr->ip_ttl = 64;
  new_ip_hdr->ip_p = ip_protocol_icmp;
  new_ip_hdr->ip_src = sr_get_interface(sr, iface)->ip;
  new_ip_hdr->ip_dst = ip_hdr->ip_src;
  new_ip_hdr->ip_sum = 0;
  new_ip_hdr->ip_sum = cksum(new_ip_hdr, sizeof(struct sr_ip_hdr));

  /* Fill ICMP Header */
  struct sr_icmp_t3_hdr *icmp_hdr =
      (struct sr_icmp_t3_hdr *)(icmp_packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_ip_hdr));
  icmp_hdr->icmp_type = type;
  icmp_hdr->icmp_code = code;
  icmp_hdr->unused = 0;
  icmp_hdr->next_mtu = 0;
  memcpy(icmp_hdr->data, ip_hdr, ICMP_DATA_SIZE);
  icmp_hdr->icmp_sum = 0;
  icmp_hdr->icmp_sum = cksum(icmp_hdr, sizeof(struct sr_icmp_t3_hdr));

  sr_send_packet(sr, icmp_packet, icmp_packet_len, iface);

  free(icmp_packet);
}

/* You should not need to touch the rest of this code. */

/* Checks if an IP->MAC mapping is in the cache. IP is in network byte
   order. You must free the returned structure if it is not NULL. */
struct sr_arpentry *sr_arpcache_lookup(struct sr_arpcache *cache, uint32_t ip) {
  pthread_mutex_lock(&(cache->lock));

  struct sr_arpentry *entry = NULL, *copy = NULL;

  int i;
  for (i = 0; i < SR_ARPCACHE_SZ; i++) {
    if ((cache->entries[i].valid) && (cache->entries[i].ip == ip)) {
      entry = &(cache->entries[i]);
    }
  }

  /* Must return a copy b/c another thread could jump in and modify
     table after we return. */
  if (entry) {
    copy = (struct sr_arpentry *)malloc(sizeof(struct sr_arpentry));
    memcpy(copy, entry, sizeof(struct sr_arpentry));
  }

  pthread_mutex_unlock(&(cache->lock));

  return copy;
}

/* Adds an ARP request to the ARP request queue. If the request is already on
   the queue, adds the packet to the linked list of packets for this sr_arpreq
   that corresponds to this ARP request. You should free the passed *packet.

   A pointer to the ARP request is returned; it should not be freed. The caller
   can remove the ARP request from the queue by calling sr_arpreq_destroy. */
struct sr_arpreq *sr_arpcache_queuereq(struct sr_arpcache *cache, uint32_t ip, uint8_t *packet, /* borrowed */
                                       unsigned int packet_len, char *iface) {
  pthread_mutex_lock(&(cache->lock));

  struct sr_arpreq *req;
  for (req = cache->requests; req != NULL; req = req->next) {
    if (req->ip == ip) {
      break;
    }
  }

  /* If the IP wasn't found, add it */
  if (!req) {
    req = (struct sr_arpreq *)calloc(1, sizeof(struct sr_arpreq));
    req->ip = ip;
    req->next = cache->requests;
    cache->requests = req;
  }

  /* Add the packet to the list of packets for this request */
  if (packet && packet_len && iface) {
    struct sr_packet *new_pkt = (struct sr_packet *)malloc(sizeof(struct sr_packet));

    new_pkt->buf = (uint8_t *)malloc(packet_len);
    memcpy(new_pkt->buf, packet, packet_len);
    new_pkt->len = packet_len;
    new_pkt->iface = (char *)malloc(sr_IFACE_NAMELEN);
    strncpy(new_pkt->iface, iface, sr_IFACE_NAMELEN);
    new_pkt->next = req->packets;
    req->packets = new_pkt;
  }

  pthread_mutex_unlock(&(cache->lock));

  return req;
}

/* This method performs two functions:
   1) Looks up this IP in the request queue. If it is found, returns a pointer
      to the sr_arpreq with this IP. Otherwise, returns NULL.
   2) Inserts this IP to MAC mapping in the cache, and marks it valid. */
struct sr_arpreq *sr_arpcache_insert(struct sr_arpcache *cache, unsigned char *mac, uint32_t ip) {
  pthread_mutex_lock(&(cache->lock));

  struct sr_arpreq *req, *prev = NULL, *next = NULL;
  for (req = cache->requests; req != NULL; req = req->next) {
    if (req->ip == ip) {
      if (prev) {
        next = req->next;
        prev->next = next;
      } else {
        next = req->next;
        cache->requests = next;
      }

      break;
    }
    prev = req;
  }

  int i;
  for (i = 0; i < SR_ARPCACHE_SZ; i++) {
    if (!(cache->entries[i].valid)) {
      break;
    }
  }

  if (i != SR_ARPCACHE_SZ) {
    memcpy(cache->entries[i].mac, mac, 6);
    cache->entries[i].ip = ip;
    cache->entries[i].added = time(NULL);
    cache->entries[i].valid = 1;
  }

  pthread_mutex_unlock(&(cache->lock));

  return req;
}

/* Frees all memory associated with this arp request entry. If this arp request
   entry is on the arp request queue, it is removed from the queue. */
void sr_arpreq_destroy(struct sr_arpcache *cache, struct sr_arpreq *entry) {
  pthread_mutex_lock(&(cache->lock));

  if (entry) {
    struct sr_arpreq *req, *prev = NULL, *next = NULL;
    for (req = cache->requests; req != NULL; req = req->next) {
      if (req == entry) {
        if (prev) {
          next = req->next;
          prev->next = next;
        } else {
          next = req->next;
          cache->requests = next;
        }

        break;
      }
      prev = req;
    }

    struct sr_packet *pkt, *nxt;

    for (pkt = entry->packets; pkt; pkt = nxt) {
      nxt = pkt->next;
      if (pkt->buf) {
        free(pkt->buf);
      }
      if (pkt->iface) {
        free(pkt->iface);
      }
      free(pkt);
    }

    free(entry);
  }

  pthread_mutex_unlock(&(cache->lock));
}

/* Prints out the ARP table. */
void sr_arpcache_dump(struct sr_arpcache *cache) {
  fprintf(stderr, "\nMAC            IP         ADDED                      VALID\n");
  fprintf(stderr, "-----------------------------------------------------------\n");

  int i;
  for (i = 0; i < SR_ARPCACHE_SZ; i++) {
    struct sr_arpentry *cur = &(cache->entries[i]);
    unsigned char *mac = cur->mac;
    fprintf(stderr, "%.1x%.1x%.1x%.1x%.1x%.1x   %.8x   %.24s   %d\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
            ntohl(cur->ip), ctime(&(cur->added)), cur->valid);
  }

  fprintf(stderr, "\n");
}

/* Initialize table + table lock. Returns 0 on success. */
int sr_arpcache_init(struct sr_arpcache *cache) {
  /* Seed RNG to kick out a random entry if all entries full. */
  srand(time(NULL));

  /* Invalidate all entries */
  memset(cache->entries, 0, sizeof(cache->entries));
  cache->requests = NULL;

  /* Acquire mutex lock */
  pthread_mutexattr_init(&(cache->attr));
  pthread_mutexattr_settype(&(cache->attr), PTHREAD_MUTEX_RECURSIVE);
  int success = pthread_mutex_init(&(cache->lock), &(cache->attr));

  return success;
}

/* Destroys table + table lock. Returns 0 on success. */
int sr_arpcache_destroy(struct sr_arpcache *cache) {
  return pthread_mutex_destroy(&(cache->lock)) && pthread_mutexattr_destroy(&(cache->attr));
}

/* Thread which sweeps through the cache and invalidates entries that were added
   more than SR_ARPCACHE_TO seconds ago. */
void *sr_arpcache_timeout(void *sr_ptr) {
  struct sr_instance *sr = sr_ptr;
  struct sr_arpcache *cache = &(sr->cache);

  while (1) {
    sleep(1.0);

    pthread_mutex_lock(&(cache->lock));

    time_t curtime = time(NULL);

    int i;
    for (i = 0; i < SR_ARPCACHE_SZ; i++) {
      if ((cache->entries[i].valid) && (difftime(curtime, cache->entries[i].added) > SR_ARPCACHE_TO)) {
        cache->entries[i].valid = 0;
      }
    }

    sr_arpcache_sweepreqs(sr);

    pthread_mutex_unlock(&(cache->lock));
  }

  return NULL;
}
