#pragma GCC diagnostic ignored "-Wold-style-cast"

#include "gamestate.h"
#include "addrinfolist.h"
#include "exception.h"
#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <string>
#include <glibmm/convert.h>
#include <glibmm/main.h>
#include <glibmm/ustring.h>
#include <sigc++/functors/mem_fun.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

GameState::GameState(const std::string &interface, const std::string &group, const std::string &port) : ok(false) {
	// Get a list of addresses to bind to.
	addrinfo hints;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = 0;
	AddrInfoList bind_ail(0, port.c_str(), &hints);

	// Get the address of the multicast group to join.
	hints.ai_flags = 0;
	AddrInfoList mcgroup_ail(group.c_str(), 0, &hints);

#ifdef WIN32
#else
	// Look up the index of the multicast interface.
	unsigned int ifindex = if_nametoindex(interface.c_str());
	if (!ifindex) {
		int err = errno;
		throw SystemError(Glib::ustring::compose(u8"Cannot look up index of network interface %1", interface), err);
	}
#endif

	// Construct a socket for each bind address.
	for (const addrinfo *i = bind_ail.get(); i; i = i->ai_next) {
		// We only handle IPv4 and IPv6, because we do not know how to do multicast configuration sockopts for other families.
		if (i->ai_family == AF_INET || i->ai_family == AF_INET6) {
			// Do a reverse lookup to get the numeric host and port.
			char host[256], serv[256];
			if (getnameinfo(i->ai_addr, i->ai_addrlen, host, sizeof(host), serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
				try {
					// Create the socket.
					Socket sock(i->ai_family, i->ai_socktype, i->ai_protocol);

					// Allow reusing recently used ports.
					static const int ONE = 1;
					if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &ONE, sizeof(ONE)) < 0) {
						throw SystemError("Cannot set SO_REUSEADDR");
					}

					// Bind to the listen address.
					if (bind(sock, i->ai_addr, i->ai_addrlen) < 0) {
						throw SystemError("Cannot bind socket");
					}

					// Try to join the socket to a multicast group.
					bool ok = false;
					for (const addrinfo *j = mcgroup_ail.get(); j; j = j->ai_next) {
						if (j->ai_family == i->ai_family) {
							if (j->ai_family == AF_INET) {
								ip_mreqn req;
								req.imr_multiaddr = reinterpret_cast<const sockaddr_in *>(j->ai_addr)->sin_addr;
								req.imr_address.s_addr = INADDR_ANY;
								req.imr_ifindex = ifindex;
								if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &req, sizeof(req)) < 0) {
									throw SystemError("Cannot join multicast group");
								}
							} else if (j->ai_family == AF_INET6) {
								ipv6_mreq req;
								req.ipv6mr_multiaddr = reinterpret_cast<const sockaddr_in6 *>(j->ai_addr)->sin6_addr;
								req.ipv6mr_interface = ifindex;
								if (setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &req, sizeof(req)) < 0) {
									throw SystemError("Cannot join multicast group");
								}
							}
							char mchost[256];
							getnameinfo(j->ai_addr, j->ai_addrlen, mchost, sizeof(mchost), 0, 0, NI_NUMERICHOST);
							ok = true;
							break;
						}
					}
					if (!ok) {
						continue;
					}

					// Permit multicast loop to the local machine, but don’t worry if it fails (Windows/UNIX disagree on whether this happens on the send or the receive path).
					if (i->ai_family == AF_INET) {
						setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &ONE, sizeof(ONE));
					} else {
						setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &ONE, sizeof(ONE));
					}

					// Add a watch to get a notification on data arriving.
					Glib::signal_io().connect(sigc::mem_fun(this, &GameState::handle_io_ready), sock, Glib::IO_IN);

					// Drop the socket into the vector.
					sockets.push_back(std::move(sock));
				} catch (const SystemError &exp) {
					std::cerr << Glib::ustring::compose(u8"Failed to create socket for bind address %1 and port %2: %3", Glib::locale_to_utf8(host), Glib::locale_to_utf8(serv), Glib::locale_to_utf8(exp.what()));
				}
			}
		}
	}
}

bool GameState::handle_io_ready(Glib::IOCondition) {
	uint8_t buffer[65536];
	bool updated = false;
	for (const Socket &sock : sockets) {
		ssize_t rc = recv(sock, buffer, sizeof(buffer), MSG_DONTWAIT | MSG_NOSIGNAL);
		if (rc < 0) {
			if (errno != EWOULDBLOCK && errno != EAGAIN) {
				throw SystemError("Cannot receive data on socket");
			}
		}
		if (!referee.ParseFromArray(buffer, static_cast<int>(rc))) {
			throw std::runtime_error("Error parsing Protobuf packet");
		}
		updated = true;
	}
	if (updated) {
		ok = true;
		timeout_connection.disconnect();
		timeout_connection = Glib::signal_timeout().connect_seconds(sigc::mem_fun(this, &GameState::handle_timeout), 3);
		signal_updated.emit();
	}
	return true;
}

bool GameState::handle_timeout() {
	ok = false;
	signal_updated.emit();
	return false;
}

