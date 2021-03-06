/* 
 * Copyright (c) 2012, Ismael Gomez-Miguelez <ismael.gomez@tsc.upc.edu>.
 * This file is part of ALOE++ (http://flexnets.upc.edu/)
 * 
 * ALOE++ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ALOE++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with ALOE++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <oesr.h>
#include <rtdal.h>
#include <rtdal_datafile.h>
#include <params.h>
#include <skeleton.h>
#include <complex.h>
#include "udp_source.h"

static int fd;

pmid_t blen_id;

int create_server_upd(char *address, int port) {
	int sockfd,n;
	struct sockaddr_in me;

	modinfo_msg("Opening socket at %s:%d\n",address,port);

	sockfd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	if (sockfd == -1) {
		perror("socket");
		return -1;
	}
	bzero(&me,sizeof(struct sockaddr_in));
	me.sin_family = AF_INET;
	me.sin_addr.s_addr=inet_addr(address);
	me.sin_port=htons(port);

	if (bind(sockfd,(struct sockaddr*) &me, sizeof(me))) {
		perror("bind");
		return -1;
	}
	/*
	int flags = fcntl(sockfd,F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(sockfd,F_SETFL,flags);
	*/
	return sockfd;

}


/**
 * @ingroup udp_source
 *
 * \param address IP address to connect
 * \param port	Port to connect
 */
int initialize() {
	char address[64];
	var_t pm;
	int i;
	int port;

	blen_id = param_id("block_length");

	if (param_get_int_name("port",&port)) {
		moderror("Port undefined\n");
		return -1;
	}

#ifdef _COMPILE_ALOE
	pm = oesr_var_param_get(ctx, "address");
	if (!pm) {
		moderror("Parameter file_name undefined\n");
		return -1;
	}

	if (oesr_var_param_get_value(ctx, pm, address, 64) == -1) {
		moderror("Error getting file_name value\n");
		return -1;
	}
#endif

	fd = create_server_upd(address,port);
	if (fd == -1) {
		moderror("Creating socket\n");
		return -1;
	}
	modinfo_msg("sock=%d\n",fd);

	return 0;
}


/**
 * @ingroup udp_source
 *
 *  Writes the received samples to the dac output buffer
 *
 */
int work(void **inp, void **out) {
	struct sockaddr_in other;
	socklen_t addrsz = sizeof(other);
	int n;
	int block_length,rcv_len,i;
	output_t *output = out[0];

	if (!out[0]) {
		moderror("output buffer not ready\n");
		return 0;
	}

	block_length=1;
	param_get_int(blen_id,&block_length);

	rcv_len = 0;
	n=1;
	while(rcv_len<block_length && n) {
		n = recvfrom(fd,&output[rcv_len],OUTPUT_MAX_SAMPLES-rcv_len,
				MSG_DONTWAIT,(struct sockaddr*) &other, &addrsz);
		if (n == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				n=0;
			} else {
				perror("recvfrom");
				return -1;
			}
		} else if (n == 0) {
			moderror("connection lost\n");
			return -1;
		}
		rcv_len += n;
	}

	if (rcv_len<block_length) {
		modinfo_msg("Received only %d/%d bytes\n",rcv_len,block_length);
	}

	moddebug("received %d bytes\n",rcv_len);

	return rcv_len;
}

/**  Deallocates resources created during initialize().
 * @return 0 on success -1 on error
 */
int stop() {
	close(fd);
	return 0;
}

