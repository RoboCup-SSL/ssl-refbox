#include "dialog_gameover.h"
#include "gameinfo.h"
#include <iomanip>

Dialog_Gameover::Dialog_Gameover(Gtk::Window& parent) :
	dialog(parent, u8"", false /*use markup*/, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true /*modal*/)
{
}

void Dialog_Gameover::update_from_gameinfo(const GameInfo& gameinfo)
{
	dialog.set_message(Glib::ustring::compose(u8"Game over.\nTime taken: %1:%2.",
				static_cast<unsigned int>(gameinfo.data.time_taken / 60), Glib::ustring::format(std::setw(2), std::setfill(L'0'), static_cast<unsigned int>(gameinfo.data.time_taken) % 60)));

	dialog.set_secondary_text(Glib::ustring::compose(u8"%1 (yellow):\n\t%2 Goals\n\t%3 Red cards\n%4 (blue):\n\t%5 Goals\n\t%6 Red cards",
				gameinfo.data.teamnames[Yellow], gameinfo.data.goals[Yellow], gameinfo.data.redcards[Yellow],
				gameinfo.data.teamnames[Blue], gameinfo.data.goals[Blue], gameinfo.data.redcards[Blue]));
}

void Dialog_Gameover::show()
{
	dialog.show();
	dialog.run(); //wait for click on button
	dialog.hide();
}

