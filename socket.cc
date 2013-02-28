#include "socket.h"
#include "exception.h"

#ifdef WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

namespace {
#ifdef WIN32
	class WinsockInitializer {
		public:
			WinsockInitializer() {
				WORD sockVersion;
				WSADATA wsaData;
				sockVersion = MAKEWORD(2, 2);
				WSAStartup(sockVersion, &wsaData);
			}

			~WinsockInitializer() {
				WSACleanup();
			}
	};
#endif
}

Socket::Socket(int domain, int type, int proto) {
	init_system();
	sock = socket(domain, type, proto);
	if (sock < 0) {
		throw SystemError("Cannot create socket");
	}
}

Socket::~Socket() {
#ifdef WIN32
	closesocket(sock);
#else
	close(sock);
#endif
}

Socket &Socket::operator=(Socket &&moveref) {
#ifdef WIN32
	closesocket(sock);
#else
	close(sock);
#endif
	sock = moveref.sock;
	moveref.sock = -1;
	return *this;
}

void Socket::init_system() {
#ifdef WIN32
	static WinsockInitializer ws_init;
#endif
}

