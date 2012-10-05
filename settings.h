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
    Settings(Logging* log);

    ~Settings();

    bool get(const std::string& setting, int&);
    bool get(const std::string& setting, std::string&);

    bool set(const std::string& setting, const int);
    bool set(const std::string& setting, const std::string&);

    void clear();
    bool readFile(const std::string& filename);
protected:

    Logging* log;
    //bool writeFile(const std::string& filename);

    std::map<std::string, int> dataInt;
    std::map<std::string, std::string> dataString;
};

#endif
