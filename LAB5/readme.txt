[run]:
./a.out [interface] [dstPort] [dstIpv4]

interface: eth0/wlan0/other

/* If dstIpv4 is specfied then multicast chat
 * else -  broadcast chat
 *
 * requires 'xterm' app for execute
 * all ipv4 multicast addr:
 * 224.0.0.0 - 239.255.255.255
 * multicast addr for test: 234.5.6.7
 */