#ifndef UDP_BROADCAST_H
#define UDP_BROADCAST_H

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>
#include "socket.h"

class Logger;

class UDPBroadcast {
	public:
		UDPBroadcast(Logger &logger, const std::string &host, const std::string &port, const std::string &interface);
		void send(const void *data, std::size_t length);

	private:
		Logger &logger;
		std::string interface;
		std::unordered_map<int, std::vector<std::pair<std::pair<std::string, std::string>, Socket>>> sockets;
};

#endif

