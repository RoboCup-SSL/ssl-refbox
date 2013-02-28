#ifndef SOCKET_H
#define SOCKET_H

#include "noncopyable.h"

class Socket : public NonCopyable {
	public:
		Socket(int domain, int type, int protocol);
		Socket(Socket &&moveref);
		~Socket();
		Socket &operator=(Socket &&moveref);
		operator int() const;

		static void init_system();

	private:
		int sock;
};



inline Socket::Socket(Socket &&moveref) {
	sock = moveref.sock;
	moveref.sock = -1;
}

inline Socket::operator int() const {
	return sock;
}

#endif

