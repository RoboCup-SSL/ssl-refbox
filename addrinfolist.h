#ifndef ADDRINFOLIST_H
#define ADDRINFOLIST_H

#include "noncopyable.h"

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

class AddrInfoList : public NonCopyable {
	public:
		AddrInfoList(const char *node, const char *service, const addrinfo *hints);
		~AddrInfoList();
		addrinfo *get() const;

	private:
		addrinfo *ai;
};

#endif

