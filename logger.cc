#include "logger.h"
#include <cwchar>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>

Logger::Logger(const std::string &filename) : start_time(std::chrono::system_clock::now()) {
	if (!filename.empty()) {
		ofs.exceptions(std::ios_base::badbit | std::ios_base::failbit);
		ofs.open(filename, std::ios_base::out | std::ios_base::app);
	}
	std::time_t real_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::wostringstream oss;
	static const wchar_t TIME_PATTERN[] = L"%x %X %Z";
	std::use_facet<std::time_put<wchar_t>>(std::locale()).put(oss, oss, L' ', std::localtime(&real_time), TIME_PATTERN, TIME_PATTERN + std::wcslen(TIME_PATTERN));
	write(Glib::ustring::compose(u8"Referee box started at %1.", oss.str()));
}

void Logger::write(const Glib::ustring &message) {
	std::chrono::high_resolution_clock::duration diff = std::chrono::high_resolution_clock::now() - start_time;
	double seconds = static_cast<unsigned int>(std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()) / 1000.0;
	const Glib::ustring &prefixed = Glib::ustring::compose(u8"[%1] %2\n", Glib::ustring::format(std::fixed, std::setprecision(3), seconds), message);
	std::cout << prefixed;
	ofs << prefixed;
}

