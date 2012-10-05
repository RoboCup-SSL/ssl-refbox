#include <gtkmm.h>
#include "gameinfo.h"
#include "sound.h"
#include "logging.h"
#include "frame_log.h"

Frame_Log::Frame_Log(Logging* log): Gtk::ScrolledWindow(),
                                        log(log),
                                        last_number(0)
{
    list = new Gtk::ListViewText(1, false, Gtk::SELECTION_NONE);

    list->set_column_title(0, "");
    list->set_size_request(400,300);

    add(*list);

    size_t len = log->get_number_of_messages();
    for(size_t i=0; i<len; ++i)
    {
        list->append_text(log->get_message(i));
    }
}

bool Frame_Log::update()
{
    size_t len = log->get_number_of_messages();
    
    if (last_number == (len-1))
    {
        return false; //no new data
    }

    for(size_t i = last_number; i<len; ++i)
    {
        list->append_text(log->get_message(i));
    }
 
    last_number = len-1;
    double upper = get_vadjustment()->get_upper();
//    double lower = get_vadjustment()->get_lower();
//    printf("lower: %f\n", lower);
    get_vadjustment()->set_value(upper + 1000);

    return true; //has new data
}

Frame_Log::~Frame_Log()
{
    delete list;
}

