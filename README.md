# Group Members

| Name        | Student Number | Email                        |
|-------------|----------------|------------------------------|
| Jiaming Lu  | 1008494620     | jiaming.lu@mail.utoronto.ca  |
| Zheyuan Wei | 1007626133     | zheyuan.wei@mail.utoronto.ca |
| Yawen Zhang | 1006739772     | weng.zhang@mail.utoronto.ca  |

# Contributions
- Jiaming Lu: Worked on handling IP packet with Yawen, mainly focused on the ICMP protocol handling in function `handle_ip_packet` and the relative helper funcitons such as `send_icmp_response` and `get_dst_interface`, as well as debug and test works for the whole router project.
- Zheyuan Wei: Worked on handling ARP packet, implemented the function `handle_arp_packet` and the relative helper functions, such as funcitons in `sr_arpcache.c`,  as well as debug and test works for the whole router project.
- Yawen Zhang: Worked on handling IP packet with Jiaming, mainly focused on the ARP protocol handling in function `handle_ip_packet` and the relative helper funcitons, such as functions in `sr_rt.c`, as well as debug and test works for the whole router project.

# Project Description
## Function documentation 
### In `sr_router.c`
Handles IP packets.
```c
static void handle_ip_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface);
```

Handles ARP packets.
```c
static void handle_arp_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface);
```

Helper function used in `hanle_ip_packet` to lookup interface with matching IP destination address.
```c
static struct sr_if *get_dst_interface(const struct sr_instance *sr, const sr_ip_hdr_t *ip_hdr);
```

Send ICMP packets to `dst_interface`. If `dst_interface == NULL`, it means the destination of the IP packet is one of our router's interface, we therefore find the destination interface via `interface` and send a response accordingly.
```c
static void send_icmp_response(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface, uint8_t type, uint8_t code, struct sr_if *dst_interface);
```

### In `sr_arpcache.c`


## List of tests cases run and results
According to the instructions in the handout, we mainly tested three commands `ping`, `traceroute` and `wget` to test the functionalities of the router. During the test, we used Wireshark as well as the console printing output to verify the correctness of the router.

To test each of these commands, we executed them with different combinations of the servers and client, as the testing results shown below:
```
mininet> client ping -c 3 192.168.2.2
PING 192.168.2.2 (192.168.2.2) 56(84)
bytes of data.
64 bytes from 192.168.2.2: icmp_seq=1
ttl=63 time-118 ms
64 bytes from 192.168.2.2: icp_seq=2
ttl=63 time-92.9 ms
64 bytes from 192.168.2.2: icmp_seq=3 ttl-63 time=50.3 ms
192.168.2.2 ping statistics ---
3 packets transmitted,
3 recelved, 0% расket Loss, time 2001ms
rtt min/avg/max/mdev = 50.326/87. 143/118.190/28.004 ms

mininet> server2 ping -c 3 10.0.1.100
appending output to 'nohup.out:
PING 10.0.1.100 (10.0.1.100) 56(84) bytes of data.
64 bytes from 10.0.1.100: imp seg=1 tt1=63 time=106 ms
64 bytes from 10.0.1.100: imp_seq=2 ttl=63 time=32.2 ms
64 bytes from 10.0.1.100: icmp_seq=3 ttl=63
time=86.9 ms
--- 10.0.1.100 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2003ms
rtt min/avg/max/mdev = 32.177/75.092/106.227/31.357 ms


```