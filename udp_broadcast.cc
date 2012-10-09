#pragma GCC diagnostic ignored "-Wold-style-cast"

#include "udp_broadcast.h"
#include "logging.h"
#ifndef WIN32
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#else
#include <windows.h>
#include <winsock.h>
#pragma comment(lib, "wsock32.lib")
#endif
#include <cstdio>
#include <cstring>

#ifndef IPPROTO_UDP
#define IPPROTO_UDP 0
#endif

UDP_Broadcast::IOError::IOError(const Glib::ustring& msg) :
	Glib::IOChannelError(Glib::IOChannelError::IO_ERROR, msg)
{
}

UDP_Broadcast::IOError::IOError(const Glib::ustring& msg, int err) :
	Glib::IOChannelError(Glib::IOChannelError::IO_ERROR, Glib::ustring::compose(u8"%1: %2", msg, std::strerror(err)))
{
}


UDP_Broadcast::UDP_Broadcast(Logging& log) :
	log(log),
	sock(-1)
{
#ifdef WIN32
	{
		WORD sockVersion;
		WSADATA wsaData;
		sockVersion = MAKEWORD(1,1);
		WSAStartup(sockVersion, &wsaData);
	}
#endif

#ifndef WIN32
	//get pointer to list of network interfaces (does not work with WIN32)
	ifaddrs *ifap = 0;
	if (getifaddrs(&ifap) < 0)
	{
		log.add(u8"could not get list of network interfaces");
		throw UDP_Broadcast::IOError("Could not get list of network interfaces", errno);
	}
	ifaddrs *it = ifap;
	while (it)
	{
		if (it->ifa_addr && it->ifa_addr->sa_family == AF_INET && it->ifa_name != std::string("lo"))
		{
			ifaddr.push_back(reinterpret_cast<const sockaddr_in *>(it->ifa_addr)->sin_addr);
			ifname.push_back(Glib::locale_to_utf8(it->ifa_name));
			log.add(Glib::ustring::compose(u8"IPv4_Multicast: Interface found: %1 (%2)", it->ifa_name, inet_ntoa(ifaddr.back())));
		}

		it = it->ifa_next;
	}

#else
	log.add(u8"ERROR(WIN32): Can not get list of network interfaces. "
			"NOT IMPLEMENTED YET!");
#endif

	// create socket
	sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( sock < 0 )
	{
		std::perror("could not create socket");
		throw UDP_Broadcast::IOError(u8"Could not create socket", errno);
	}

	// permit broadcasts
	static const int one = 1;
	setsockopt( sock, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one) );
}

UDP_Broadcast::~UDP_Broadcast()
{
#ifndef WIN32
	close(sock);
#else
	closesocket(sock);
#endif
}


void UDP_Broadcast::set_destination(const std::string& host, const uint16_t port)
{
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
#ifndef WIN32
	if (! inet_aton(host.c_str(), &addr.sin_addr))
	{
		throw IOError(Glib::ustring::compose(u8"inet_aton failed. Invalid address: %1", Glib::locale_to_utf8(host)));
	}
#else
	addr.sin_addr.s_addr = inet_addr(host.c_str());
	if (addr.sin_addr.s_addr == 0)
	{
		throw IOError(Glib::ustring::compose(u8"Invalid address: %1", host));
	} 
#endif

	// allow packets to be received on this host        
#ifndef WIN32
	static const int one = 1;
	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &one, sizeof(one)) < 0)
	{
		throw IOError(u8"could not set multicast loop", errno);
	}
#endif
}

void UDP_Broadcast::send(const void* buffer, const size_t buflen)
{   
#ifndef WIN32
	for(std::size_t i = 0; i < ifaddr.size(); ++i)
	{
		//select network interface
		struct ip_mreqn mreqn;
		mreqn.imr_ifindex = 0;
		mreqn.imr_multiaddr = addr.sin_addr;
		mreqn.imr_address = ifaddr[i];

		if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &mreqn, sizeof(mreqn)) < 0)
		{
			throw UDP_Broadcast::IOError(u8"setsockopt() failed", errno);

		}

		//send data
		ssize_t result = ::sendto(sock, buffer, buflen, MSG_NOSIGNAL, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
		if (result < 0)
		{
			throw UDP_Broadcast::IOError(Glib::ustring::compose(u8"send() failed for iface %1", ifname[i]), errno);
		}
	}
	if (ifaddr.empty())
	{
		//no usable interfaces; the user is probably using loopback and has no network connection
		ssize_t result = ::sendto(sock, buffer, buflen, MSG_NOSIGNAL, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
		if (result < 0)
		{
			throw UDP_Broadcast::IOError(u8"send() failed", errno);
		}
	}
#else
	//send data
	ssize_t result = ::sendto(sock, buffer, buflen, 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
	if (result < 0)
	{
		throw UDP_Broadcast::IOError(u8"send() failed", errno);
	}
#endif //WIN32
}

