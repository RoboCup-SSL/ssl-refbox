#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <stdexcept>
#include <string>

class SystemError : public std::runtime_error {
	public:
		SystemError(const std::string &message);
		SystemError(const std::string &message, int rc);
};

class GAIError : public std::runtime_error {
	public:
		GAIError(const std::string &message, int rc);
};

#endif

