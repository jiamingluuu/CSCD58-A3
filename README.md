## Names and details of the team members
### Name: Jiaming Lu 

Student number: 1008494620

Email: jiaming.lu@mail.utoronto.ca

Contributions: Worked on handling IP packet with Yawen, mainly focused on the ICMP protocol handling in function `handle_ip_packet` and the relative helper funcitons such as `send_icmp_response` and `get_dst_interface`, as well as debug and test works for the whole router project.

### Name: Zheyuan Wei
Student number: 1007626133

Email: zheyuan.wei@mail.utoronto.ca

Contributions: Worked on hadnling ARP packet, implemented the function `handle_arp_packet` and the relative helper functions, such as funcitons in `sr_arpcache.c`,  as well as debug and test works for the whole router project.

### Name: Yawen Zhang
Student number: 1006739772

Email: weng.zhang@mail.utoronto.ca

Contributions: Worked on handling IP packet with Jiaming, mainly focused on the ARP protocol handling in function `handle_ip_packet` and the relative helper funcitons, such as functions in `sr_rt.c`, as well as debug and test works for the whole router project.

## Description and documentation for the functions that implemented the required and missed functionalities in the starter code
- The router must successfully route packets between the Internet and the application servers.
  
  
- The router must correctly handle ARP requests and replies.
- The router must correctly handle traceroutes through it (where it is not the end host) and to it
(where it is the end host).
- The router must respond correctly to ICMP echo requests.
- The router must handle TCP/UDP packets sent to one of its interfaces. In this case the router
should respond with an ICMP port unreachable.
- The router must maintain an ARP cache whose entries are invalidated after a timeout period
(timeouts should be on the order of 15 seconds).
- The router must queue all packets waiting for outstanding ARP replies. If a host does not respond
to 5 ARP requests, the queued packet is dropped and an ICMP host unreachable message is sent
back to the source of the queued packet.
- The router must not needlessly drop packets (for example when waiting for an ARP reply)
- The router must enforce guarantees on timeouts–that is, if an ARP request is not responded to
within a fixed period of time, the ICMP host unreachable message is generated even if no more packets
arrive at the router. (Note: You can guarantee this by implementing the sr arpcache sweepreqs
function in sr arpcache.c correctly.)
12


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