#ifndef LOGGING_H
#define LOGGING_H

#include <vector>
#include <string>

class Logging
{
public:
    Logging();
    void add(const char* str,...);
    size_t get_number_of_messages() const { return messages.size(); }


    std::string get_timestamp(const size_t number) const
    {
        return timestamps[number];
    }


    std::string get_message(const size_t number) const
    {
        return messages[number];
    }

protected:
    std::vector<std::string> timestamps;
    std::vector<std::string> messages;
    time_t time_startup;
};

#endif
