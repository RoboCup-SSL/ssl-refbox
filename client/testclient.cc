#include "referee.pb.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MULTICAST_INTERFACE "enp7s0"
#define MULTICAST_ADDRESS "224.5.23.1"
#define MULTICAST_PORT "10003"

int main() {
	// Get a socket address to bind to.
	addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
	addrinfo *ai = 0;
	int gai_err;
	if ((gai_err = getaddrinfo(0, MULTICAST_PORT, &hints, &ai)) != 0) {
		std::cerr << gai_strerror(gai_err);
		return 1;
	}

	// Look up the index of the network interface from its name.
	unsigned int ifindex = if_nametoindex(MULTICAST_INTERFACE);
	if (!ifindex) {
		std::cerr << std::strerror(errno) << '\n';
		return 1;
	}

	// Create and bind a socket.
	int sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (sock < 0) {
		std::cerr << std::strerror(errno) << '\n';
		return 1;
	}
	if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
		std::cerr << std::strerror(errno) << '\n';
		return 1;
	}

	// Join the multicast group.
	ip_mreqn mcreq;
	mcreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDRESS);
	mcreq.imr_address.s_addr = INADDR_ANY;
	mcreq.imr_ifindex = ifindex;
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mcreq, sizeof(mcreq)) < 0) {
		std::cerr << std::strerror(errno) << '\n';
		return 1;
	}

	for (;;) {
		// Receive a packet.
		uint8_t buffer[65536];
		ssize_t len = recv(sock, buffer, sizeof(buffer), 0);
		if (len < 0) {
			std::cerr << std::strerror(errno) << '\n';
			return 1;
		}

		// Parse a Protobuf structure out of it.
		SSL_Referee referee;
		if (!referee.ParseFromArray(buffer, len)) {
			std::cerr << "Protobuf parsing error!\n";
			return 1;
		}

		// Display some information.
		std::cout << "TS=" << referee.packet_timestamp() << ", stage=" << referee.stage() << ", stage_time_left=" << referee.stage_time_left() << ", command=" << referee.command() << ", yscore=" << referee.yellow().score() << ", bscore=" << referee.blue().score() << '\n';
	}
}

