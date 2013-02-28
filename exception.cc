#include "exception.h"
#include <cerrno>
#include <cstring>
#include <sstream>

#ifdef WIN32
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

namespace {
	std::string make_message(const std::string &message, int rc) {
		std::ostringstream oss;
		oss << message << ": " << std::strerror(rc);
		return oss.str();
	}

	std::string make_gai_message(const std::string &message, int rc) {
		std::ostringstream oss;
		oss << message << ": " << gai_strerror(rc);
		return oss.str();
	}
}

SystemError::SystemError(const std::string &message) : std::runtime_error(make_message(message, errno)) {
}

SystemError::SystemError(const std::string &message, int rc) : std::runtime_error(make_message(message, rc)) {
}

GAIError::GAIError(const std::string &message, int rc) : std::runtime_error(make_gai_message(message, rc)) {
}

