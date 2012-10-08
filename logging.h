#ifndef LOGGING_H
#define LOGGING_H

#include <ctime>
#include <glibmm.h>

class Logging
{
	public:
		Logging();
		void add(const Glib::ustring& message);

	protected:
		std::time_t time_startup;
};


#endif

