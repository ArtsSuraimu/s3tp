/*
 * eth.c
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>


#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <netinet/ip.h>
#include <signal.h>
#include <linux/if.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/if_arp.h>
#include <netdb.h>

#include "eth.h"


int open_dom(
		struct sockaddr_un *sa,
		const char *filename)
{
	int sd,slen;

	memset(sa,0,sizeof(*sa));
	sa->sun_family = AF_UNIX;
	strncpy(sa->sun_path,filename,sizeof(sa->sun_path));
	unlink(sa->sun_path);
	slen = sizeof(sa->sun_family) + strlen(sa->sun_path);

	if (0 > (sd=socket(AF_UNIX,SOCK_SEQPACKET,0))) {
		perror("socket() failed");
		return -1;
	}

	if (0 > bind(sd,(struct sockaddr *)sa,slen)) {
		perror("bind() failed");
		return -1;
	}

	if (0 > listen(sd,0)) {
		perror("listen() failed");
		return -1;
	}

	return sd;
}

int open_raw_l2(
		struct sockaddr_ll *sa,
		const char *ifname)
{
	int sd;
	struct ifreq ifr;

	if (0 > (sd=socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL)))) {
		perror("socket() failed");
		return -1;
	}

	memset(&ifr,0,sizeof(ifr));
	strncpy(ifr.ifr_name,ifname,IFNAMSIZ);
	if (0 > ioctl(sd,SIOCGIFINDEX,&ifr)) {
		perror("ioctl() failed");
		return -1;
	}

	memset(sa,0,sizeof(*sa));
	sa->sll_family		= PF_PACKET;
	sa->sll_protocol	= htons(ETH_P_ALL);
	sa->sll_ifindex		= ifr.ifr_ifindex;
	sa->sll_hatype		= ARPHRD_ETHER;
	sa->sll_pkttype		= PACKET_OTHERHOST;
	sa->sll_halen		= ETH_ALEN;

	memset(&ifr,0,sizeof(ifr));
	strncpy(ifr.ifr_name,ifname,IFNAMSIZ);
	if (0 > ioctl(sd,SIOCGIFHWADDR,&ifr)) {
		perror("ioctl() failed");
		return -1;
	}
	memcpy(sa->sll_addr,ifr.ifr_hwaddr.sa_data,ETH_ALEN);

	if (0 > bind(sd,(struct sockaddr *)sa,sizeof(*sa))) {
		perror("bind() failed");
		return -1;
	}

	return sd;
}

int open_raw_l3(
		struct sockaddr_in *sa,
		const char *ifname)
{
	int sd,optval;
	struct ifreq ifr;

	if (0 > (sd=socket(AF_INET,SOCK_RAW,IPPROTO_RAW))) {
		perror("socket() failed");
		return -1;
	}

	optval = 1;
	if( 0 > setsockopt(sd,IPPROTO_IP,IP_HDRINCL,&optval,
					sizeof(int)) ) {
	perror("setsockopt() failed");
	return -1;
	}

	memset(&ifr,0,sizeof(ifr));
	strncpy(ifr.ifr_name,ifname,IFNAMSIZ);
	if (0 > ioctl(sd,SIOCGIFADDR,&ifr)) {
		perror("ioctl() failed");
		return -1;
	}

	memset(sa,0,sizeof(*sa));
	sa->sin_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;

	printf("%s\n", inet_ntoa(sa->sin_addr));
	sa->sin_family = AF_INET;

	if (0 > bind(sd,(struct sockaddr *)sa,sizeof(*sa))) {
		perror("bind() failed");
		return -1;
	}

	return sd;
}
