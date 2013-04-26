#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <string>
#include <vector>
#include <glibmm/ustring.h>

class Logger;

class Configuration {
	public:
		// [normal] section
		unsigned int normal_half_seconds;
		unsigned int normal_half_time_seconds;
		unsigned int normal_timeout_seconds;
		unsigned int normal_timeouts;

		// [overtime] section
		unsigned int overtime_break_seconds;
		unsigned int overtime_half_seconds;
		unsigned int overtime_half_time_seconds;
		unsigned int overtime_timeout_seconds;
		unsigned int overtime_timeouts;

		// [shootout] section
		unsigned int shootout_break_seconds;

		// [global] section
		unsigned int yellow_card_seconds;

		// [files] section
		std::string save_filename;
		std::string log_filename;

		// [ip] section
		std::string address;
		std::string legacy_port;
		std::string protobuf_port;
		std::string interface;

		// [teams] section
		std::vector<Glib::ustring> teams;

		Configuration(const std::string &filename);
		void dump(Logger &logger);
};

#endif

