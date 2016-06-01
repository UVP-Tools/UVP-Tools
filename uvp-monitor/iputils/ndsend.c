/*
 * Copyright (C) 2000-2008, Parallels, Inc. All rights reserved.
 *
 * This code is public domain.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/if.h>
#include <netinet/icmp6.h>

#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <getopt.h>
#include <unistd.h>
#include <signal.h>

#define EXC_OK 0
#define EXC_USAGE	1
#define EXC_SYS		2
#define EXC_RECV	3
#define EXC_NORECV	4

#define NAME "ndsend: "

char* iface = NULL;

struct in6_addr src_ipaddr;
int ifindex;
__u8 real_hwaddr[6];

struct nd_packet {
	__u8		icmp6_type;
	__u8		icmp6_code;
	__u16		icmp6_cksum;

	__u32		reserved:5,
			override:1,
			solicited:1,
			router:1,
			reserved2:24;
	struct in6_addr target;
	__u8		otype;
	__u8		ospace;
	__u8		obits[6];
};

static int init_device_addresses(int sock, const char* device)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, device, IFNAMSIZ-1);

	if (ioctl(sock, SIOCGIFINDEX, &ifr) != 0) {
		fprintf(stderr, NAME "Unknown network interface %s: %m\n",
				device);
		return -1;
	}
	ifindex = ifr.ifr_ifindex;

	/* get interface HW address */
	if (ioctl(sock, SIOCGIFHWADDR, &ifr) != 0) {
		fprintf(stderr, NAME "Can not get the MAC address of "
				"network interface %s: %m\n", device);
		return -1;
	}
	memcpy(real_hwaddr, ifr.ifr_hwaddr.sa_data, 6);

	return 0;
}

int sock;
struct nd_packet pkt;

static void create_nd_packet(struct nd_packet* pkt)
{
	pkt->icmp6_type = ND_NEIGHBOR_ADVERT;
	pkt->icmp6_code = 0;
	pkt->icmp6_cksum = 0;
	pkt->reserved = 0;
	pkt->override = 1;
	pkt->solicited = 0;
	pkt->router = 0;
	pkt->reserved = 0;
	memcpy(&pkt->target, &src_ipaddr, 16);
	pkt->otype = ND_OPT_TARGET_LINKADDR;
	pkt->ospace = 1;
	memcpy(&pkt->obits, real_hwaddr, 6);
}

static void sender(void)
{
	struct sockaddr_in6 to;

	to.sin6_family = AF_INET6;
	to.sin6_port = 0;
	((__u32*)&to.sin6_addr)[0] = htonl(0xFF020000);
	((__u32*)&to.sin6_addr)[1] = 0;
	((__u32*)&to.sin6_addr)[2] = 0;
	((__u32*)&to.sin6_addr)[3] = htonl(0x1);
	to.sin6_scope_id = ifindex;

	if (sendto(sock, &pkt, sizeof(pkt), 0,
	    (struct sockaddr*) &to, sizeof(to)) < 0) {
		fprintf(stderr, NAME "Error in sendto(): %m\n");
		exit(EXC_SYS);
	}
}

static void usage()
{
	printf(
"ndsend sends an unsolicited Neighbor Advertisement ICMPv6 multicast packet\n"
"announcing a given IPv6 address to all IPv6 nodes as per RFC4861.\n\n"
"Usage: ndsend <address> <interface> (example: ndsend 2001:DB8::1 eth0)\n\n"
"Note: ndsend is called by arpsend -U -i, see arpsend(8) man page.\n"
	);
}

int main(int argc,char** argv)
{
	int value;

	if (argc != 3) {
		usage();
		exit(EXC_USAGE);
	}

	if (inet_pton(AF_INET6, argv[1], &src_ipaddr) <= 0)
		return -1;
	iface = argv[2];

	sock = socket(PF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	if (sock < 0) {
		fprintf(stderr, NAME "Error in socket(): %m\n");
		exit(EXC_SYS);
	}

	if (init_device_addresses(sock, iface) < 0)
		exit(EXC_SYS);

	value = 255;
	setsockopt (sock, SOL_IPV6, IPV6_MULTICAST_HOPS,
		    &value, sizeof (value));

	create_nd_packet(&pkt);
	sender();
	exit(EXC_OK);
}
