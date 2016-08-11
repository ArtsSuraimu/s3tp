/*
 * eth.c
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */

#ifndef DRIVERS_ETH_C_
#define DRIVERS_ETH_C_

int open_dom(struct sockaddr_un *sa, const char *filename);
int open_raw_l2(struct sockaddr_ll *sa, const char *ifname);
int open_raw_l3(struct sockaddr_in *sa,	const char *ifname);


#endif /* DRIVERS_ETH_C_ */
