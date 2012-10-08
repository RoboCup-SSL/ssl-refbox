#include "logging.h"
#include <glibmm.h>
#include <iomanip>
#include <iostream>

Logging::Logging()
{
	std::time(&time_startup);
}


void Logging::add(const Glib::ustring& message)
{
	std::time_t now;
	std::time(&now);
	std::cout << Glib::ustring::format(std::setw(5), now - time_startup) << "s: " << Glib::locale_from_utf8(message) << '\n';
}

