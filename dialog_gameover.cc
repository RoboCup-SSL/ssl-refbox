#include <gtkmm.h>
#include "gameinfo.h"
#include "sound.h"
#include "dialog_gameover.h"

Dialog_Gameover::Dialog_Gameover(Gtk::Window *parent)
{
    dialog = new Gtk::MessageDialog(*parent,
                                    "Game over.\nResult: ?? : ??",
                                    false, //use markup
                                    Gtk::MESSAGE_INFO,
                                    Gtk::BUTTONS_OK,
                                    true //modal
        );
}

Dialog_Gameover::~Dialog_Gameover()
{
    delete dialog;
}

bool Dialog_Gameover::update_from_gameinfo(const GameInfo* gameinfo)
{
    char buf[1024];

    sound_play("logout.wav");

    snprintf(buf, 1024,
             "Game over.\nTime taken: %02.2f minutes.",
             gameinfo->data.time_taken / 60.);
    dialog->set_message(buf);


    snprintf(buf, 1024,
             "%s (yellow):\n"
             "\t%i Goals\n"
             "\t%i Red cards\n"

             "%s (yellow):\n"
             "\t%i Goals\n"
             "\t%i Red cards\n",
             gameinfo->data.teamnames[Yellow],
             gameinfo->data.goals[Yellow],
             gameinfo->data.redcards[Yellow],

             gameinfo->data.teamnames[Blue],
             gameinfo->data.goals[Blue],
             gameinfo->data.redcards[Blue]);



    dialog->set_secondary_text(buf);

    return true;
}

void Dialog_Gameover::show()
{
    dialog->show();
    dialog->run(); //wait for click on button
    dialog->hide();
}
