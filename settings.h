#ifndef SETTINGS_H
#define SETTINGS_H

#include <map>
#include <string>

class Gamecontrol;
class Logging;

class Settings
{
	public:
		/** Constructor with Filename */
		Settings(Logging& log);

		void get(const std::string& setting, int&);
		void get(const std::string& setting, std::string&);

		void readFile(const std::string& filename);

	protected:
		Logging& log;

		std::map<std::string, int> dataInt;
		std::map<std::string, std::string> dataString;
};

#endif

