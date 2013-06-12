#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "gamestate.h"
#include "imagedb.h"
#include <cairomm/context.h>
#include <cairomm/refptr.h>
#include <glibmm/keyfile.h>
#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/entry.h>
#include <gtkmm/table.h>
#include <gtkmm/window.h>
#include <pangomm/fontdescription.h>
#include <pangomm/rectangle.h>

class MainWindow : public Gtk::Window {
	public:
		MainWindow(GameState &state, const image_database_t &flags, const image_database_t &logos, const Glib::KeyFile &config);

	protected:
		bool on_expose_event(GdkEventExpose *);
		bool on_window_state_event(GdkEventWindowState *);

	private:
		struct ImageCache {
			Glib::ustring team;
			Glib::RefPtr<Gdk::Pixbuf> image;
		};

		GameState &state;
		const image_database_t &flags;
		const image_database_t &logos;
		const Glib::KeyFile &config;

		bool is_fullscreen;

		ImageCache yellow_logo_cache, yellow_flag_cache, blue_logo_cache, blue_flag_cache;

		void handle_state_updated();
		int key_snoop(Widget *, GdkEventKey *);
		void on_size_allocate(Gdk::Rectangle &);
		Glib::RefPtr<Gdk::Pixbuf> resize_image(Glib::RefPtr<Gdk::Pixbuf> image, int width, int height, ImageCache &cache, const Glib::ustring &team);
		void draw_team_rectangle(const Glib::ustring &name, ImageCache &logo_cache, ImageCache &flag_cache, Pango::Rectangle inner_rect, Cairo::RefPtr<Cairo::Context> context, int padding, unsigned int score);
};

#endif

