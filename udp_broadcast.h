#ifndef UDP_BROADCAST_H
#define UDP_BROADCAST_H

#include <cstddef>
#include <glibmm.h>
#include <stdint.h>
#include <string>
#include <vector>

#ifndef WIN32
#include <sys/socket.h>
#include <netinet/ip.h>
#else
#include <winsock.h>
#endif

class Logging;

class UDP_Broadcast
{
	public:
		class IOError : public Glib::IOChannelError
		{
			public:
				IOError(const Glib::ustring& msg);
				IOError(const Glib::ustring& msg, int err);
		};

		UDP_Broadcast(Logging& log);
		~UDP_Broadcast();

		void set_destination(const std::string& host, uint16_t port);

		void send(const void* buffer, std::size_t buflen);

	protected:
		Logging& log;
		struct sockaddr_in addr;
		std::vector<in_addr> ifaddr;
		std::vector<Glib::ustring> ifname;
		int sock;
};

#endif

