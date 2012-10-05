#ifndef DIALOG_GAMEOVER_H
#define DIALOG_GAMEOVER_H

namespace Gtk
{
    class MessageDialog;
    class Window;
};

class GameInfo;

class Dialog_Gameover
{
public:
    Dialog_Gameover(Gtk::Window *parent);
    ~Dialog_Gameover();

    bool update_from_gameinfo(const GameInfo*);

    void show();

protected:
    Gtk::MessageDialog *dialog;
};

#endif
