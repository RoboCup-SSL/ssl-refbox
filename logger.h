#ifndef LOGGER_H
#define LOGGER_H

#include <chrono>
#include <fstream>
#include <string>
#include <glibmm/ustring.h>

class Logger {
	public:
		Logger(const std::string &log_filename);
		void write(const Glib::ustring &message);

	private:
		std::chrono::high_resolution_clock::time_point start_time;
		std::ofstream ofs;
};

#endif

