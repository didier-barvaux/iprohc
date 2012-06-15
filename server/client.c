#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#include "log.h"

#include "tun_helpers.h"
#include "rohc_tunnel.h"
#include "client.h"

void close_tunnel(void* v_tunnel)
{
	struct tunnel* tunnel = (struct tunnel*) v_tunnel ;
	trace(LOG_INFO, "[%s] Properly close client", inet_ntoa(tunnel->dest_address)) ;
	close(tunnel->raw_socket) ;
	tunnel->alive = -1 ; /* Mark to be deleted */
}

int new_client(int socket, int tun, struct client** clients, int max_clients, struct server_opts server_opts) {
	int conn; 
	struct	sockaddr_in src_addr;
	socklen_t src_addr_len = sizeof(src_addr);
	struct in_addr local;
	int i = 0 ;
	int raw ;

	/* New client */
	conn = accept(socket, (struct sockaddr*)&src_addr, &src_addr_len) ;
	if (conn < 0) {
		perror("Fail accept\n") ;
	}
	trace(LOG_INFO, "Connection from %s (%d)\n", inet_ntoa(src_addr.sin_addr), src_addr.sin_addr.s_addr) ;

	/* client creation parameters */
	trace(LOG_DEBUG, "Creation of client") ;
	
	while (clients[i] != NULL && i < max_clients) { i++; }
	if (i == max_clients) {
		return -2 ;
	}

	clients[i] = malloc(sizeof(struct client)) ;
	trace(LOG_DEBUG, "Allocating %p", clients[i]) ;
	clients[i]->tcp_socket = conn ;

	/* dest_addr */
	clients[i]->tunnel.dest_address  = src_addr.sin_addr ;
	/* local_addr */
	local.s_addr = htonl(ntohl(server_opts.local_address) + i + 10) ;
	clients[i]->local_address = local ;

	/* set tun */
	clients[i]->tunnel.tun = tun ; /* real tun device */
	if (socketpair(AF_UNIX, SOCK_RAW, 0, clients[i]->tunnel.fake_tun) < 0) {
		perror("Can't open pipe for tun") ;
		/* TODO  : Flush */
		return 1 ;
	}

	/* set raw */
	raw = create_raw() ;
	if (raw < 0) {
		trace(LOG_ERR, "Unable to create raw socket") ;
	}
	clients[i]->tunnel.raw_socket = raw ;
	if (socketpair(AF_UNIX, SOCK_RAW, 0, clients[i]->tunnel.fake_raw) < 0) {
		perror("Can't open pipe for raw") ;
		/* TODO  : Flush */
		return -1 ;
	}

	memcpy(&(clients[i]->tunnel.params),  &(server_opts.params), sizeof(struct tunnel_params)) ;
	clients[i]->tunnel.params.local_address = local.s_addr ;
	clients[i]->tunnel.alive =   0 ;
	clients[i]->tunnel.close_callback = close_tunnel ;

	clients[i]->last_keepalive.tv_sec = -1 ;

	trace(LOG_DEBUG, "Created") ;

	return i ;
}


int start_client_tunnel(struct client* client)
{
	/* Go threads, go ! */
	pthread_create(&(client->thread_tunnel), NULL, new_tunnel, (void*)(&(client->tunnel))) ;
	return 0 ;
}


