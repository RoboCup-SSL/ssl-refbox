#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#ifdef WIN32
#include <crtdefs.h>
#endif

#include "logging.h"




Logging::Logging()
{
    time(&time_startup);
}


void Logging::add(const char* str,...)
{
    va_list ap;
    time_t now;
    char timestamp[128];
    char message[1024];

    va_start(ap,str);

    time(&now);
    snprintf(timestamp, 127,"%5is", (int)(now - time_startup));
    vsnprintf(message,1023,str,ap);

    //message to stdout
    puts(message);

    timestamps.push_back(timestamp);
    messages.push_back(message);

    while(messages.size() > 1024)
        messages.erase(messages.begin());

    while(timestamps.size() > 1024)
        timestamps.erase(timestamps.begin());

    va_end(ap);
}
