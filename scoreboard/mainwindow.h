#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "gamestate.h"
#include "imagedb.h"
#include <gtkmm/box.h>
#include <gtkmm/entry.h>
#include <gtkmm/table.h>
#include <gtkmm/window.h>
#include <pangomm/fontdescription.h>

class MainWindow : public Gtk::Window {
	public:
		MainWindow(GameState &state, const image_database_t &flags, const image_database_t &logos);

	protected:
		bool on_expose_event(GdkEventExpose *);
		bool on_window_state_event(GdkEventWindowState *);

	private:
		GameState &state;
		const image_database_t &flags;
		const image_database_t &logos;

		bool is_fullscreen;

		void handle_state_updated();
		int key_snoop(Widget *, GdkEventKey *);
		void on_size_allocate(Gdk::Rectangle &);
};

#endif

