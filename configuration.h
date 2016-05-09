#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <cstdint>
#include <string>
#include <vector>
#include <glibmm/ustring.h>

class Logger;

class Configuration {
	public:
		// [normal] section
		int normal_half_seconds;
		int normal_half_time_seconds;
		unsigned int normal_timeout_seconds;
		unsigned int normal_timeouts;

		// [overtime] section
		int overtime_break_seconds;
		int overtime_half_seconds;
		int overtime_half_time_seconds;
		unsigned int overtime_timeout_seconds;
		unsigned int overtime_timeouts;

		// [shootout] section
		int shootout_break_seconds;

		// [global] section
		unsigned int yellow_card_seconds;
		bool team_names_required;

		// [files] section
		std::string save_filename;
		std::string log_filename;

		// [ip] section
		std::string address;
		std::string legacy_port;
		std::string protobuf_port;
		std::string interface;
		uint16_t rcon_port;

		// [teams] section
		std::vector<Glib::ustring> teams;

		Configuration(const std::string &filename);
		void dump(Logger &logger);
};

#endif

