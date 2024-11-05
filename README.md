# Group Members

| Name        | Student Number | Email                        |
|-------------|----------------|------------------------------|
| Jiaming Lu  | 1008494620     | jiaming.lu@mail.utoronto.ca  |
| Zheyuan Wei | 1007626133     | zheyuan.wei@mail.utoronto.ca |
| Yawen Zhang | 1006739772     | yaweng.zhang@mail.utoronto.ca  |

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
This function gets called every second, tries to handle each one of the cached ARP request in the request queue.
```c
void sr_arpcache_sweepreqs(struct sr_instance *sr);
```

Invoked by `sr_arpcache_sweepreqs` to handles the corresponding ARP request. It iterates through the ARP request queue and re-sends any outstanding ARP requests that haven’t been sent in the past second. If an ARP request has been sent 5 times with no response, a destination host unreachable should go back to all the sender of packets that were waiting on a reply to this ARP request.
```c
void handle_arpreq(struct sr_instance *sr, struct sr_arpreq *req);
```

Used to send ICMP type 3 response in `handle_arpreq`.
```c
void sr_send_icmp_t3(struct sr_instance *sr, uint8_t *packet, unsigned int len, const char *iface, uint8_t type, uint8_t code);
```

Used to create an ARP request in `handle_arpreq`
```c
uint8_t *create_arp_request(struct sr_instance *sr, uint32_t ip, const char *iface);
```

## List of tests cases run and results
According to the instructions in the handout, we mainly tested three commands `ping`, `traceroute` and `wget` to test the functionalities of the router. During the test, we used Wireshark as well as the console printing output to verify the correctness of the router.

To test each of these commands, we executed them with different combinations of the servers and client, as the testing results shown below:
### `ping` command
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


mininet> server2 ping -c 3 10.0.1.100
appending output to 'nohup.out'
PING 10.0.1.100 (10.0.1.100) 56(84) bytes of data.
64 bytes from 10.0.1.100: imp_seq=1 ttl=63 time=106 ms
64 bytes from 10.0.1.100: imp_ seq=2 ttl=63 time=32.2 ms
64 bytes from 10.0.1.100: 1cmp_seq=3 ttl=63 time=86.9 ms

--- 10.0.1.100 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2003ms
rtt min/avg/max/mdev = 32. 177/75.092/106.227/31.357 ms


mininet> serverl ping -c 3 172.64.3.10
PING 172.64.3.10 (172.64.3.10) 56(84) bytes of data.
64 bytes from 172.64.3.10: imp_seq=1 ttl=63 time=50.5 ms
64 bytes from 172.64.3.10: icmp seq=2 ttl=63 time=49.9 ms
64 bytes from 172.64.3.10: icmp seq=3 ttl=63 time=53.2 ms

--- 172.64.3.10 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2012ms
rtt min/avg/max/mdev = 49.865/51.198/53.243/1.467 ms

```

### `traceroute` command
Restart the router program and run the following commands. Each  `traceroute ` is ran twice:
```
mininet> client traceroute -n 192.168.2.2
traceroute to 192.168.2.2 (192.168.2.2), 30 hops max, 60 byte packets
1   * * *
2   * * *
3   * * *
4   * 192.168.2.2   110.891 ms   111.323 ms 


mininet> client traceroute -n 192.168.2.2
traceroute to 192.168.2.2 (192.168:2.2), 30 hops max, 60 byte packets
1   * * *
2   192.168.2.2   59.063 ms   59.251 ms   59.374 ms


mininet> client traceroute -n 172.64.3.10
traceroute to 172.64.3.10 (172.64.3.10), 30 hops max, 60 byte packets
1   * * *
2   * * *
3   * * *
4   * 172.64.3.10   142.610 ms   143.454 ms


mininet> client traceroute -n 172.64.3.10
traceroute to 172.64.3.10 (172.64.3.10), 30 hops max, 60 byte packets
1   * * *
2   172.64.3.10   68.241 ms   69.139 ms   69.807 ms


mininet> server1 traceroute -n 172.64.3.10
traceroute to 172.64.3.10 (172.64.3.10), 30 hops max, 60 byte packets
1   * * *
2   * * *
3   * * *
4   * 172.64.3.10   110.951 ms   152.087 ms


mininet> server1 traceroute -n 172.64.3.10
traceroute to 172.64.3.10 (172.64.3.10), 30 hops max, 60 byte packets
1   * * *
2   * * *
3   * * *
4   * 172,64.3.10   87.493 ms   87.517 ms
```

### `wget` command
Restart the router program and run the following commands:
```
mininet> client wget http://192.168.2.2
--2024-11-04 19:56:16-- http://192. 168.2.2/ 
Connecting to 192.168.2.2:80... connected.
HTTP request sent, awaiting response... 200 0K
Length: 161 [text/html]
Saving to: 'index.html'

index.html         100%[===================>]     161   --.-KB/s  in 0s

2024-11-04 19:56:16 (60.4 MB/s) - 'index.html' saved [161/161]


mininet> client wget http://172.64.3.10
--2024-11-04 20:00:08-- http://172.64.3.10/ 
Connecting to 172.64.3.10:80... connected.
HTTP request sent, awaiting response... 200 0K
Length: 161 [text/html]
Saving to: 'index.html'

index.html.1        100%[===================>]     161   --.-KB/s  in 0s

2024-11-04 20:00:08 (60.4 MB/s) - 'index.html.1' saved [161/161]


mininet> server1 wget http://172.64.3.10
--2024-11-04 20:02:10-- http://172.64.3.10/ 
Connecting to 172.64.3.10:80... connected.
HTTP request sent, awaiting response... 200 0K
Length: 161 [text/html]
Saving to: 'index.html'

index.html.1        100%[===================>]     161   --.-KB/s  in 0s

2024-11-04 20:02:10 (41.3 MB/s) - 'index.html.1' saved [161/161]

```