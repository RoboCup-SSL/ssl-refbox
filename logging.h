#ifndef LOGGING_H
#define LOGGING_H

#include <chrono>
#include <glibmm.h>

class Logging
{
	public:
		Logging();
		void add(const Glib::ustring& message);

	protected:
		std::chrono::system_clock::time_point time_startup;
};


#endif

