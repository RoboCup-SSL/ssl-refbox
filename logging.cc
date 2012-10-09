#include "logging.h"
#include <glibmm.h>
#include <iomanip>
#include <iostream>

Logging::Logging() :
	time_startup(std::chrono::system_clock::now())
{
}


void Logging::add(const Glib::ustring& message)
{
	std::chrono::system_clock::duration diff = std::chrono::system_clock::now() - time_startup;
	std::cout << Glib::ustring::format(std::setw(5), std::chrono::duration_cast<std::chrono::duration<unsigned int, std::ratio<1>>>(diff).count()) << "s: " << message << '\n';
}

