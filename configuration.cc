#include "configuration.h"
#include "logger.h"
#include <algorithm>
#include <glibmm/convert.h>
#include <glibmm/keyfile.h>
#include <glibmm/ustring.h>

Configuration::Configuration(const std::string &filename) {
	Glib::KeyFile kf;
	kf.load_from_file(filename);

	normal_half_seconds = kf.get_integer(u8"normal", u8"HALF");
	normal_half_time_seconds = kf.get_integer(u8"normal", u8"HALF_TIME");
	normal_timeout_seconds = kf.get_integer(u8"normal", u8"TIMEOUT_TIME");
	normal_timeouts = kf.get_integer(u8"normal", u8"TIMEOUTS");

	overtime_break_seconds = kf.get_integer(u8"overtime", u8"BREAK");
	overtime_half_seconds = kf.get_integer(u8"overtime", u8"HALF");
	overtime_half_time_seconds = kf.get_integer(u8"overtime", u8"HALF_TIME");
	overtime_timeout_seconds = kf.get_integer(u8"overtime", u8"TIMEOUT_TIME");
	overtime_timeouts = kf.get_integer(u8"overtime", u8"TIMEOUTS");

	shootout_break_seconds = kf.get_integer(u8"shootout", u8"BREAK");

	yellow_card_seconds = kf.get_integer(u8"global", u8"YELLOW_CARD_TIME");

	save_filename = kf.has_key(u8"files", u8"SAVE") ? Glib::filename_from_utf8(kf.get_string(u8"files", u8"SAVE")) : "";
	log_filename = kf.has_key(u8"files", u8"LOG") ? Glib::filename_from_utf8(kf.get_string(u8"files", u8"LOG")) : "";

	address = kf.get_string(u8"ip", u8"ADDRESS");
	legacy_port = kf.has_key(u8"ip", u8"LEGACY_PORT") ? kf.get_string(u8"ip", u8"LEGACY_PORT") : "";
	protobuf_port = kf.has_key(u8"ip", u8"PROTOBUF_PORT") ? kf.get_string(u8"ip", u8"PROTOBUF_PORT") : "";
	interface = kf.has_key(u8"ip", u8"INTERFACE") ? kf.get_string(u8"ip", u8"INTERFACE") : "";

	for (const Glib::ustring &key : kf.get_keys(u8"teams")) {
		teams.push_back(kf.get_string(u8"teams", key));
	}
	std::sort(teams.begin(), teams.end());
}

void Configuration::dump(Logger &logger) {
	logger.write(Glib::ustring::compose(u8"Configuration: Normal halves: %1 seconds.", normal_half_seconds));
	logger.write(Glib::ustring::compose(u8"Configuration: Normal half time: %1 seconds.", normal_half_time_seconds));
	logger.write(Glib::ustring::compose(u8"Configuration: Normal timeouts: %1, totalling up to %2 seconds.", normal_timeouts, normal_timeout_seconds));
	logger.write(Glib::ustring::compose(u8"Configuration: Pre-overtime break: %1 seconds.", overtime_break_seconds));
	logger.write(Glib::ustring::compose(u8"Configuration: Overtime halves: %1 seconds.", overtime_half_seconds));
	logger.write(Glib::ustring::compose(u8"Configuration: Overtime half time: %1 seconds.", overtime_half_time_seconds));
	logger.write(Glib::ustring::compose(u8"Configuration: Overtime timeouts: %1, totalling up to %2 seconds.", overtime_timeouts, overtime_timeout_seconds));
	logger.write(Glib::ustring::compose(u8"Configuration: Pre-shootout break: %1 seconds.", shootout_break_seconds));
	logger.write(Glib::ustring::compose(u8"Configuration: Yellow card: %1 seconds.", yellow_card_seconds));
	if (!save_filename.empty()) {
		logger.write(Glib::ustring::compose(u8"Configuration: State save filename: \"%1\".", Glib::filename_to_utf8(save_filename)));
	}
	if (!log_filename.empty()) {
		logger.write(Glib::ustring::compose(u8"Configuration: Log filename: \"%1\".", Glib::filename_to_utf8(log_filename)));
	}
	logger.write(Glib::ustring::compose(u8"Configuration: Packet destination address: \"%1\".", Glib::locale_to_utf8(address)));
	if (!legacy_port.empty()) {
		logger.write(Glib::ustring::compose(u8"Configuration: Legacy port: \"%1\".", legacy_port));
	}
	if (!protobuf_port.empty()) {
		logger.write(Glib::ustring::compose(u8"Configuration: Protobuf port: \"%1\".", protobuf_port));
	}
	if (!interface.empty()) {
		logger.write(Glib::ustring::compose(u8"Configuration: Network interface: \"%1\".", Glib::locale_to_utf8(interface)));
	}
}

