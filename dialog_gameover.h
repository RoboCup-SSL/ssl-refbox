#ifndef DIALOG_GAMEOVER_H
#define DIALOG_GAMEOVER_H

#include <gtkmm.h>

class GameInfo;

class Dialog_Gameover
{
	public:
		Dialog_Gameover(Gtk::Window& parent);

		bool update_from_gameinfo(const GameInfo&);

		void show();

	protected:
		Gtk::MessageDialog dialog;
};

#endif
