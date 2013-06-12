#include "mainwindow.h"
#include <algorithm>
#include <initializer_list>
#include <iomanip>
#include <cairomm/context.h>
#include <cairomm/refptr.h>
#include <gdkmm/general.h>
#include <glibmm/refptr.h>
#include <gtkmm/main.h>
#include <pangomm/fontdescription.h>
#include <pangomm/layout.h>
#include <sigc++/functors/mem_fun.h>

// Set to 1 to show rectangles around layout elements.
// Useful for debugging the rectangle calculation code.
#define SHOW_LAYOUT 0

namespace {
	Glib::ustring format_time_deciseconds(uint64_t micros) {
		uint64_t decis = micros / 100000U;
		uint64_t seconds = decis / 10;
		decis %= 10;
		uint64_t minutes = seconds / 60;
		seconds %= 60;
		return Glib::ustring::compose(u8"%1:%2.%3", minutes, Glib::ustring::format(std::setw(2), std::setfill(L'0'), seconds), decis);
	}

	Glib::RefPtr<Gdk::Pixbuf> find_image(const image_database_t &db, const Glib::ustring &team) {
		Glib::RefPtr<Gdk::Pixbuf> ret;
		const std::string &ckey = team.casefold_collate_key();
		const image_database_t::const_iterator iter = db.find(ckey);
		if (iter != db.end()) {
			ret = iter->second;
		}
		return ret;
	}

	void split_rect_horizontal(const Pango::Rectangle &container, const std::initializer_list<Pango::Rectangle *> &rectangles, const std::initializer_list<double> &fractions) {
		int x = container.get_x();
		int y = container.get_y();
		int w = container.get_width();
		int h = container.get_height();
		decltype(rectangles.begin()) i, iend;
		decltype(fractions.begin()) j;
		for (i = rectangles.begin(), iend = rectangles.end(), j = fractions.begin(); i != iend; ++i, ++j) {
			(*i)->set_x(x);
			(*i)->set_y(y);
			(*i)->set_width(static_cast<int>(w * *j));
			(*i)->set_height(h);
			x += static_cast<int>(w * *j);
		}
	}

	void split_rect_vertical(const Pango::Rectangle &container, const std::initializer_list<Pango::Rectangle *> &rectangles, const std::initializer_list<double> &fractions) {
		int x = container.get_x();
		int y = container.get_y();
		int w = container.get_width();
		int h = container.get_height();
		decltype(rectangles.begin()) i, iend;
		decltype(fractions.begin()) j;
		for (i = rectangles.begin(), iend = rectangles.end(), j = fractions.begin(); i != iend; ++i, ++j) {
			(*i)->set_x(x);
			(*i)->set_y(y);
			(*i)->set_width(w);
			(*i)->set_height(static_cast<int>(h * *j));
			y += static_cast<int>(h * *j);
		}
	}

	void pad_rect(const Pango::Rectangle &outer, Pango::Rectangle &inner, int padding) {
		inner.set_x(outer.get_x() + padding);
		inner.set_y(outer.get_y() + padding);
		inner.set_width(outer.get_width() - 2 * padding);
		inner.set_height(outer.get_height() - 2 * padding);
	}

	void draw_text(Cairo::RefPtr<Cairo::Context> ctx, const Pango::Rectangle &rect, int padding, const Glib::ustring &text) {
		Pango::FontDescription fd;
		fd.set_family(u8"monospace");
		fd.set_style(Pango::STYLE_NORMAL);
		fd.set_variant(Pango::VARIANT_NORMAL);
		fd.set_size(24 * Pango::SCALE);

		Glib::RefPtr<Pango::Layout> layout(Pango::Layout::create(ctx));
		layout->set_text(text);
		layout->set_font_description(fd);
		Pango::Rectangle orig_pixel_extents = layout->get_pixel_logical_extents();

		int target_width = rect.get_width() - 2 * padding;
		int target_height = rect.get_height() - 2 * padding;

		if (target_width <= 0 || target_height <= 0) {
			return;
		}

		double xscale = static_cast<double>(target_width) / orig_pixel_extents.get_width();
		double yscale = static_cast<double>(target_height) / orig_pixel_extents.get_height();
		double scale = std::min(xscale, yscale);

		fd.set_size(static_cast<int>(24 * Pango::SCALE * scale));
		layout->set_font_description(fd);
		Pango::Rectangle new_pixel_extents = layout->get_pixel_logical_extents();

		ctx->move_to(rect.get_x() + padding + (target_width - new_pixel_extents.get_width()) / 2, rect.get_y() + padding + (target_height - new_pixel_extents.get_height()) / 2);
		layout->show_in_cairo_context(ctx);
	}

	void draw_image(Cairo::RefPtr<Cairo::Context> ctx, const Pango::Rectangle &rect, int padding, Glib::RefPtr<Gdk::Pixbuf> image) {
		Pango::Rectangle target;
		pad_rect(rect, target, padding);

		if (rect.get_width() <= 0 || rect.get_height() <= 0) {
			return;
		}

		ctx->save();
		ctx->translate(target.get_x() + (target.get_width() - image->get_width()) / 2, target.get_y() + (target.get_height() - image->get_height()) / 2);
		Gdk::Cairo::set_source_pixbuf(ctx, image, 0, 0);
		ctx->paint();
		ctx->restore();
	}
}

MainWindow::MainWindow(GameState &state, const image_database_t &flags, const image_database_t &logos, const Glib::KeyFile &config) :
		state(state),
		flags(flags),
		logos(logos),
		config(config),
		is_fullscreen(false) {
	set_title(u8"Scoreboard (press F to toggle fullscreen");

	Gtk::Main::signal_key_snooper().connect(sigc::mem_fun(this, &MainWindow::key_snoop));

	state.signal_updated.connect(sigc::mem_fun(this, &MainWindow::handle_state_updated));

	set_default_size(400, 400);
	show_all();
}

bool MainWindow::on_expose_event(GdkEventExpose *evt) {
	// Pass to superclass.
	Gtk::Window::on_expose_event(evt);

	// Get dimensions and create context.
	int width, height;
	get_window()->get_size(width, height);
	Cairo::RefPtr<Cairo::Context> ctx = get_window()->create_cairo_context();

	// Fill background with black.
	ctx->set_source_rgb(0.0, 0.0, 0.0);
	ctx->paint();

	// Compute how big a padding area should be (2% of whichever dimension is smaller).
	int padding = std::min(width, height) / 50;

	// If the current data is not valid, just show a big message and stop.
	if (!state.ok) {
		ctx->set_source_rgb(1.0, 1.0, 1.0);
		Pango::Rectangle window_rect(0, 0, width, height);
		draw_text(ctx, window_rect, padding, u8"No Signal");
		return true;
	}

	// Compute a rectangle that will hold each part of the display.
	Pango::Rectangle window_rect(0, 0, width, height);
	// window_rect
		Pango::Rectangle top_rect, bottom_rect;
		split_rect_vertical(window_rect, {&top_rect, &bottom_rect}, {0.2, 0.8});
		// top_rect
			Pango::Rectangle clock_rect, stages_rect;
			split_rect_horizontal(top_rect, {&clock_rect, &stages_rect}, {0.7, 0.3});
			// clock_rect is terminal
			// stages_rect
				Pango::Rectangle stages_top_rect, stages_bottom_rect;
				split_rect_vertical(stages_rect, {&stages_top_rect, &stages_bottom_rect}, {0.5, 0.5});
				// stages_top_rect
					Pango::Rectangle half_time_rect, first_half_rect, second_half_rect;
					split_rect_horizontal(stages_top_rect, {&half_time_rect, &first_half_rect, &second_half_rect}, {0.333, 0.333, 0.334});
					// these are terminal
				// stages_bottom_rect
					Pango::Rectangle overtime_first_half_rect, overtime_second_half_rect, penalty_shootout_rect;
					split_rect_horizontal(stages_bottom_rect, {&overtime_first_half_rect, &overtime_second_half_rect, &penalty_shootout_rect}, {0.333, 0.333, 0.334});
					// these are terminal
		// bottom_rect
			Pango::Rectangle yellow_rect, blue_rect;
			split_rect_horizontal(bottom_rect, {&yellow_rect, &blue_rect}, {0.5, 0.5});
			// yellow_rect
				Pango::Rectangle yellow_inner_rect;
				pad_rect(yellow_rect, yellow_inner_rect, padding);
			// blue_rect
				Pango::Rectangle blue_inner_rect;
				pad_rect(blue_rect, blue_inner_rect, padding);

#if SHOW_LAYOUT
	// Show the rectangles making up the layout.
	ctx->set_source_rgb(1.0, 1.0, 1.0);
	std::initializer_list<const Pango::Rectangle *> rects{&clock_rect, &half_time_rect, &first_half_rect, &second_half_rect, &overtime_first_half_rect, &overtime_second_half_rect, &penalty_shootout_rect, &blue_rect, &blue_name_rect, &blue_flag_rect, &blue_logo_rect, &blue_score_rect};
	for (auto i : rects) {
		ctx->rectangle(i->get_x(), i->get_y(), i->get_width(), i->get_height());
	}
	ctx->stroke();
#endif

	// Draw the coloured outlines for the team info.
	ctx->set_line_width(padding);
	ctx->set_source_rgb(1.0, 1.0, 0.0);
	ctx->rectangle((yellow_rect.get_x() + yellow_inner_rect.get_x()) / 2, (yellow_rect.get_y() + yellow_inner_rect.get_y()) / 2, (yellow_rect.get_width() + yellow_inner_rect.get_width()) / 2, (yellow_rect.get_height() + yellow_inner_rect.get_height()) / 2);
	ctx->stroke();
	ctx->set_source_rgb(0.0, 0.0, 1.0);
	ctx->rectangle((blue_rect.get_x() + blue_inner_rect.get_x()) / 2, (blue_rect.get_y() + blue_inner_rect.get_y()) / 2, (blue_rect.get_width() + blue_inner_rect.get_width()) / 2, (blue_rect.get_height() + blue_inner_rect.get_height()) / 2);
	ctx->stroke();
	ctx->set_line_width(1);

	// Draw the common texts.
	ctx->set_source_rgb(1.0, 1.0, 1.0);
	draw_text(ctx, clock_rect, padding, state.referee.stage_time_left() < 0 ? u8"0:00.0" : format_time_deciseconds(state.referee.stage_time_left()));
	{
		static const Glib::ustring STAGE_TEXTS[6] = { u8"HT", u8"N1", u8"N2", u8"O1", u8"O2", u8"PS" };
		static const int PROTOBUF_TO_STAGE_MAPPING[14] = { 1, 1, 0, 2, 2, 0, 3, 3, 0, 4, 4, 0, 5, -1 };
		const Pango::Rectangle * const stage_rects[6] = { &half_time_rect, &first_half_rect, &second_half_rect, &overtime_first_half_rect, &overtime_second_half_rect, &penalty_shootout_rect };
		for (int i = 0; i < 6; ++i) {
			if (PROTOBUF_TO_STAGE_MAPPING[state.referee.stage()] == i) {
				ctx->set_source_rgb(1.0, 1.0, 1.0);
			} else {
				ctx->set_source_rgb(0.2, 0.2, 0.2);
			}
			draw_text(ctx, *stage_rects[i], padding, STAGE_TEXTS[i]);
		}
	}

	// Draw the team information panels.
	draw_team_rectangle(state.referee.yellow().name(), yellow_logo_cache, yellow_flag_cache, yellow_inner_rect, ctx, padding, state.referee.yellow().score());
	draw_team_rectangle(state.referee.blue().name(), blue_logo_cache, blue_flag_cache, blue_inner_rect, ctx, padding, state.referee.blue().score());

	return true;
}

bool MainWindow::on_window_state_event(GdkEventWindowState *evt) {
	is_fullscreen = !!(evt->new_window_state & GDK_WINDOW_STATE_FULLSCREEN);
	return true;
}

void MainWindow::handle_state_updated() {
	const Glib::RefPtr<Gdk::Window> win(get_window());
	if (win) {
		win->invalidate(false);
	}
}

int MainWindow::key_snoop(Widget *, GdkEventKey *event) {
	if (event->type == GDK_KEY_PRESS && (event->keyval == GDK_F || event->keyval == GDK_f)) {
		if (is_fullscreen) {
			unfullscreen();
		} else {
			fullscreen();
		}
	}
	return 0;
}

void MainWindow::on_size_allocate(Gdk::Rectangle &) {
	const Glib::RefPtr<Gdk::Window> win(get_window());
	if (win) {
		win->invalidate(false);
	}
}

Glib::RefPtr<Gdk::Pixbuf> MainWindow::resize_image(Glib::RefPtr<Gdk::Pixbuf> image, int width, int height, ImageCache &cache, const Glib::ustring &team) {
	if (!cache.image || cache.image->get_width() != width || cache.image->get_height() != height || cache.team != team) {
		double xscale = static_cast<double>(width) / image->get_width();
		double yscale = static_cast<double>(height) / image->get_height();
		double scale = std::min(xscale, yscale);
		cache.image = image->scale_simple(std::min(width, static_cast<int>(image->get_width() * scale)), std::min(height, static_cast<int>(image->get_height() * scale)), Gdk::INTERP_BILINEAR);
		cache.team = team;
	}
	return cache.image;
}

void MainWindow::draw_team_rectangle(const Glib::ustring &name, ImageCache &logo_cache, ImageCache &flag_cache, Pango::Rectangle inner_rect, Cairo::RefPtr<Cairo::Context> ctx, int padding, unsigned int score) {
	// Find the logo and flag images for the team.
	Glib::RefPtr<Gdk::Pixbuf> logo = find_image(logos, name);
	Glib::RefPtr<Gdk::Pixbuf> flag = find_image(flags, name);

	// Decide whether to show the team name.
	bool show_name = config.has_key(u8"shownames", name) ? config.get_boolean(u8"shownames", name) : !logo;
	if (!show_name && !flag && !logo) {
		show_name = true;
	}

	// inner_rect
		Pango::Rectangle top_rect, score_rect;
		split_rect_vertical(inner_rect, {&top_rect, &score_rect}, {0.6, 0.4});
		// top_rect
			Pango::Rectangle name_rect, images_rect, logo_rect, flag_rect;
			if (show_name && (flag || logo)) {
				split_rect_vertical(top_rect, {&name_rect, &images_rect}, {0.35, 0.65});
			} else if (flag || logo) {
				images_rect = top_rect;
			} else {
				name_rect = top_rect;
			}
			// name_rect is terminal
			// images_rect
				if (flag && logo) {
					int fwidth = flag->get_width();
					int fheight = flag->get_height();
					int lwidth = logo->get_width();
					int lheight = logo->get_height();
					int hwidth = fwidth + lwidth, hheight = std::max(fheight, lheight);
					int vwidth = std::max(fwidth, lwidth), vheight = fheight + lheight;
					double hscale = std::min(static_cast<double>(images_rect.get_width()) / hwidth, static_cast<double>(images_rect.get_height()) / hheight);
					double vscale = std::min(static_cast<double>(images_rect.get_width()) / vwidth, static_cast<double>(images_rect.get_height()) / vheight);
					if (hscale >= vscale) {
						split_rect_horizontal(images_rect, {&logo_rect, &flag_rect}, {0.5, 0.5});
					} else {
						split_rect_vertical(images_rect, {&logo_rect, &flag_rect}, {0.5, 0.5});
					}
					// logo_rect is terminal
					// flag_rect is terminal
				} else if (logo) {
					logo_rect = images_rect;
					// logo_rect is terminal
				} else if (flag) {
					flag_rect = images_rect;
					// flag_rect is terminal
				}
		// score_rect is terminal

#if SHOW_LAYOUT
	// Show the rectangles making up the layout.
	ctx->set_source_rgb(1.0, 1.0, 1.0);
	std::initializer_list<const Pango::Rectangle *> rects{&clock_rect, &half_time_rect, &first_half_rect, &second_half_rect, &overtime_first_half_rect, &overtime_second_half_rect, &penalty_shootout_rect, &rect, &name_rect, &flag_rect, &logo_rect, &score_rect, &blue_rect, &blue_name_rect, &blue_flag_rect, &blue_logo_rect, &blue_score_rect};
	for (auto i : rects) {
		ctx->rectangle(i->get_x(), i->get_y(), i->get_width(), i->get_height());
	}
	ctx->stroke();
#endif

	ctx->set_source_rgb(1.0, 1.0, 1.0);
	draw_text(ctx, name_rect, padding, name);
	if (logo) {
		draw_image(ctx, logo_rect, padding, resize_image(logo, logo_rect.get_width(), logo_rect.get_height(), logo_cache, name));
	}
	if (flag) {
		draw_image(ctx, flag_rect, padding, resize_image(flag, flag_rect.get_width(), flag_rect.get_height(), flag_cache, name));
	}
	ctx->set_source_rgb(1.0, 1.0, 1.0);
	draw_text(ctx, score_rect, padding, Glib::ustring::format(score));
}

