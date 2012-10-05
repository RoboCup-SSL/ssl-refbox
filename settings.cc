#include <cstdlib> //exit
#include <cstdio>

#include "settings.h"
#include "logging.h"
#include "gamecontrol.h"

Settings::Settings(Logging* log): log(log)
{
}

Settings::~Settings()
{
}


void Settings::clear()
{
    dataString.clear();
    dataInt.clear();
}


bool Settings::set(const std::string& setting, const int value)
{
    dataInt[setting] = value;
    return true;
}


bool Settings::set(const std::string& setting, const std::string& value)
{
    dataString[setting] = value;
    return true;
}


bool Settings::get(const std::string& setting, std::string& value)
{
    if (dataString.find(setting) == dataString.end())
    {
        //err("Settings: Setting(String) for \"%s\" not found.", setting.c_str());
        exit(-1);
        return false;
    }

    value = dataString[setting];
    return true;
}


bool Settings::get(const std::string& setting, int& value)
{
    if (dataInt.find(setting) == dataInt.end())
    {
        //err("Settings: Setting(Integer) for \"%s\" not found.", setting.c_str());
        exit(-1);
        return false;
    }
    value = dataInt[setting];
    return true;
}


bool Settings::readFile(const std::string& filename)
{
    FILE* fd = fopen(filename.c_str(), "r");
    if (!fd)
    {
        //Warn user!
        //err("Settings: Configfile \"%s\" not found.", filename.c_str());
        exit(1);
        return false;
    }

    char tmp_name[128];
    char tmp_strvalue[1024];
    int result;
    
    while(EOF != (result = fscanf(fd, "%127s = %1023s", tmp_name, tmp_strvalue)) )
    {
        if (result == 2)
        {
            log->add("Settings: %s = %s", tmp_name, tmp_strvalue);
            set(tmp_name, tmp_strvalue);

            int tmp_intvalue;
            if (1 == sscanf(tmp_strvalue, "%i", &tmp_intvalue) )
            {
                log->add("Settings: \"%s\" has integer value %i.", tmp_name, tmp_intvalue);
                set(tmp_name, tmp_intvalue);
            } 
        }
    }

    return true;
}
