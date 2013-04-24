#include "udpbroadcast.h"
#include "addrinfolist.h"
#include "exception.h"
#include "logger.h"
#include "noncopyable.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <glibmm/convert.h>
#include <glibmm/ustring.h>

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

namespace {
	class InterfaceList;

	class InterfaceInfo {
		public:
			bool configure_socket(const Socket &sock, Logger &logger) const;
			const std::string &name() const;
			int family() const;

			static std::vector<InterfaceInfo> all();

		private:
			friend class InterfaceList;

			std::string name_;
			int family_;

#ifdef WIN32
			InterfaceInfo(int family);
#else
			unsigned int ifindex;

			InterfaceInfo(const std::string &name, int family, unsigned int ifindex);
#endif
	};
}



bool InterfaceInfo::configure_socket(const Socket &sock, Logger &logger) const {
#ifdef WIN32
	return true;
#else
	if (family_ == AF_INET) {
		ip_mreqn mreq;
		mreq.imr_ifindex = ifindex;
		if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &mreq, sizeof(mreq)) < 0) {
			int rc = errno;
			logger.write(Glib::ustring::compose(u8"Cannot set IPv4 socket to send multicast packet on interface %1: %2", Glib::locale_to_utf8(name_), Glib::locale_to_utf8(std::strerror(rc))));
			return false;
		}
		return true;
	} else if (family_ == AF_INET6) {
		ipv6_mreq mreq;
		mreq.ipv6mr_interface = ifindex;
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, &mreq, sizeof(mreq)) < 0) {
			int rc = errno;
			logger.write(Glib::ustring::compose(u8"Cannot set IPv6 socket to send multicast packet on interface %1: %2", Glib::locale_to_utf8(name_), Glib::locale_to_utf8(std::strerror(rc))));
			return false;
		}
		return true;
	} else {
		throw std::logic_error("InterfaceInfo::configure_socket() has unknown family!");
	}
#endif
}

const std::string &InterfaceInfo::name() const {
	return name_;
}

int InterfaceInfo::family() const {
	return family_;
}

std::vector<InterfaceInfo> InterfaceInfo::all() {
	std::vector<InterfaceInfo> vec;
#ifdef WIN32
	vec.push_back(InterfaceInfo(AF_INET));
	vec.push_back(InterfaceInfo(AF_INET6));
#else
	ifaddrs *ifs = 0;
	if (getifaddrs(&ifs) < 0) {
		throw SystemError("Cannot get network interface list");
	}
	try {
		for (const ifaddrs *i = ifs; i; i = i->ifa_next) {
			if ((i->ifa_flags & IFF_UP) && (i->ifa_flags & IFF_MULTICAST) && i->ifa_addr) {
				if (i->ifa_addr->sa_family == AF_INET || i->ifa_addr->sa_family == AF_INET6) {
					unsigned int ifindex = if_nametoindex(i->ifa_name);
					if (ifindex) {
						vec.push_back(InterfaceInfo(i->ifa_name, i->ifa_addr->sa_family, ifindex));
					}
				}
			}
		}
		freeifaddrs(ifs);
	} catch (...) {
		freeifaddrs(ifs);
		throw;
	}
#endif
	return vec;
}

#ifdef WIN32
InterfaceInfo::InterfaceInfo(int family) : name_(family == AF_INET ? "inet" : "inet6"), family_(family) {
}
#else
InterfaceInfo::InterfaceInfo(const std::string &name, int family, unsigned int ifindex) : name_(name), family_(family), ifindex(ifindex) {
}
#endif



UDPBroadcast::UDPBroadcast(Logger &logger, const std::string &host, const std::string &port, const std::string &interface) : logger(logger), interface(interface) {
	// Initialize the sockets subsystem.
	Socket::init_system();

	// Look up the target host/IP and port.
	addrinfo hints;
	hints.ai_flags = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = 0;
	AddrInfoList ai(host.c_str(), port.c_str(), &hints);

	// Construct a socket for each destination.
	for (const addrinfo *i = ai.get(); i; i = i->ai_next) {
		// We only handle IPv4 and IPv6, because we do not know how to do multicast configuration sockopts for other families.
		if (i->ai_family == AF_INET || i->ai_family == AF_INET6) {
			// Do a reverse lookup to get the numeric host and port.
			char host[256], serv[256];
			if (getnameinfo(i->ai_addr, i->ai_addrlen, host, sizeof(host), serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
				try {
					// Create the socket.
					Socket sock(i->ai_family, i->ai_socktype, i->ai_protocol);

					// Permit broadcasts, but don’t worry if it fails.
					static const int one = 1;
					setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));

					// Permit multicast loop to the local machine, but don’t worry if it fails (Windows/UNIX disagree on whether this happens on the send or the receive path).
					if (i->ai_family == AF_INET) {
						setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &one, sizeof(one));
					} else {
						setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &one, sizeof(one));
					}

					// Lock in a default destination address.
					if (connect(sock, i->ai_addr, i->ai_addrlen) < 0) {
						throw SystemError("Cannot connect socket");
					}

					// Drop the socket into the map keyed by family.
					sockets[i->ai_family].push_back(std::make_pair(std::make_pair(std::string(host), std::string(serv)), std::move(sock)));
				} catch (const SystemError &exp) {
					logger.write(Glib::ustring::compose(u8"Failed to create socket for destination address %1 and port %2: %3", Glib::locale_to_utf8(host), Glib::locale_to_utf8(serv), Glib::locale_to_utf8(exp.what())));
				}
			}
		}
	}
}

void UDPBroadcast::send(const void *data, size_t length) {
	// Go through the interfaces.
	static std::vector<InterfaceInfo> interfaces = InterfaceInfo::all();
	for (const InterfaceInfo &i : interfaces) {
		// If the interface name was provided in the configuration file, ignore any interface that does not match that name.
		if (!interface.empty() && i.name() != interface) {
			continue;
		}

		// Go through all the sockets whose families match the family of this interface.
		auto socks = sockets.find(i.family());
		if (socks == sockets.end()) {
			continue;
		}
		for (const std::pair<std::pair<std::string, std::string>, Socket> &sock : socks->second) {
			if (i.configure_socket(sock.second, logger)) {
				// The socket was set up to send to this interface.
				// Now send data.
				ssize_t ssz = ::send(sock.second, data, length, MSG_NOSIGNAL);
				if (ssz < 0) {
					int rc = errno;
					logger.write(Glib::ustring::compose(u8"Failed to send on interface %1 to address %2 and port %3: %4", Glib::locale_to_utf8(i.name()), Glib::locale_to_utf8(sock.first.first), Glib::locale_to_utf8(sock.first.second), Glib::locale_to_utf8(std::strerror(rc))));
				} else if (ssz != static_cast<ssize_t>(length)) {
					logger.write(Glib::ustring::compose(u8"Short write sending on interface %1 to address %2 and port %3!", Glib::locale_to_utf8(i.name()), Glib::locale_to_utf8(sock.first.first), Glib::locale_to_utf8(sock.first.second)));
				}
			}
		}
	}
}

