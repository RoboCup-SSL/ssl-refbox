#ifndef FRAME_LOG_H
# define FRAME_LOG_H

#include <gtkmm.h>

namespace Gtk
{
    class ListViewText;
};

class Logging;

class Frame_Log: public Gtk::ScrolledWindow
{
public:
    Frame_Log(Logging* log);
    ~Frame_Log();
    bool update();

protected:
    Logging* log;
    size_t last_number;
    Gtk::ListViewText *list;
};


#endif
