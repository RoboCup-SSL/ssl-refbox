#include "settings.h"
#include "logging.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

Settings::Settings(Logging& log): log(log)
{
}


void Settings::get(const std::string& setting, std::string& value)
{
	if (dataString.find(setting) == dataString.end())
	{
		throw std::runtime_error(Glib::locale_from_utf8(Glib::ustring::compose(u8"Setting \"%1\" missing in settings file", Glib::locale_to_utf8(setting))));
	}

	value = dataString[setting];
}


void Settings::get(const std::string& setting, int& value)
{
	if (dataInt.find(setting) == dataInt.end())
	{
		throw std::runtime_error(Glib::locale_from_utf8(Glib::ustring::compose(u8"Setting \"%1\" missing or noninteger in settings file", Glib::locale_to_utf8(setting))));
	}

	value = dataInt[setting];
}


void Settings::readFile(const std::string& filename)
{
	std::ifstream ifs;
	ifs.exceptions(std::ios_base::badbit | std::ios_base::eofbit | std::ios_base::failbit);
	ifs.open(filename);
	ifs.exceptions(std::ios_base::goodbit);

	while (ifs) {
		std::string line;
		std::getline(ifs, line);
		std::string name, sep, value;
		std::istringstream iss(line);
		if (iss >> name >> sep >> value && sep == "=") {
			log.add(Glib::ustring::compose(u8"Settings: %1 = %2", name, value));
			dataString[name] = value;

			try {
				int intvalue = std::stoi(value);
				dataInt[name] = intvalue;
				log.add(Glib::ustring::compose(u8"Settings: \"%1\" has integer value %2.", name, intvalue));
			} catch (const std::invalid_argument&) {
				// Swallow: not an integer
			}
		}
	}
}

