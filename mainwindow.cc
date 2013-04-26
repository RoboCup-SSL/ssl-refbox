#include "mainwindow.h"
#include "configuration.h"
#include "gamecontroller.h"
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <glibmm/ustring.h>
#include <sigc++/adaptors/bind.h>
#include <sigc++/functors/mem_fun.h>

namespace {
	// Make sure these stay in the same order as the enums!
	const Glib::ustring STAGE_NAMES[] = {
		u8"Pre-Game",
		u8"First Half",
		u8"Half Time",
		u8"Pre-Second Half",
		u8"Second Half",
		u8"Half Time",
		u8"Pre-Overtime First Half",
		u8"Overtime First Half",
		u8"Half Time",
		u8"Pre-Overtime Second Half",
		u8"Overtime Second Half",
		u8"Half Time",
		u8"Penalty Shootout",
		u8"Post-Game",
	};
	const Glib::ustring COMMAND_NAMES[] = {
		u8"Halted",
		u8"Stopped",
		u8"Running",
		u8"Running",
		u8"Prestart (KY)",
		u8"Prestart (KB)",
		u8"Prestart (PY)",
		u8"Prestart (PB)",
		u8"Running",
		u8"Running",
		u8"Running",
		u8"Running",
		u8"Timeout (Y)",
		u8"Timeout (B)",
		u8"Stopped",
		u8"Stopped",
	};

	Glib::ustring format_time_deciseconds(uint64_t micros) {
		uint64_t decis = micros / 100000U;
		uint64_t seconds = decis / 10;
		decis %= 10;
		uint64_t minutes = seconds / 60;
		seconds %= 60;
		return Glib::ustring::compose(u8"%1:%2.%3", minutes, Glib::ustring::format(std::setw(2), std::setfill(L'0'), seconds), decis);
	}

	Glib::ustring format_time_deciseconds(int64_t micros) {
		if (micros < 0) {
			return Glib::ustring::compose(u8"−%1", format_time_deciseconds(static_cast<uint64_t>(-micros)));
		} else {
			return format_time_deciseconds(static_cast<uint64_t>(micros));
		}
	}

	Glib::ustring format_time_deciseconds(uint32_t micros) {
		return format_time_deciseconds(static_cast<uint64_t>(micros));
	}

	Glib::ustring format_time_deciseconds(int32_t micros) {
		return format_time_deciseconds(static_cast<int64_t>(micros));
	}
}

MainWindow::MainWindow(GameController &controller) :
		Gtk::Window(),
		controller(controller),

		ignore_rules_menu_item(u8"Ignore Rules"),

		halt_but(u8"Halt (KP_Point)"),
		stop_but(u8"Stop Game (KP_0)"),
		force_start_but(u8"Force Start (KP_5)"),
		normal_start_but(u8"Normal Start (KP_Enter)"),

		goal_frame(u8" Yellow vs. Blue "),
		yellow_goal(u8"00"),
		blue_goal(u8"00"),
		vs(u8":"),
		teamname_yellow(true),
		teamname_blue(true),
		switch_colours_but(u8"<-->"),

		game_control_frame(u8"Game Status"),
		stage_label(u8"??.??:?? left"),
		time_label(u8"00.00:00"),
		timeleft_label(u8"??.??:?? left"),
		game_status_label(u8"Stopped"),
		game_control_box(Gtk::BUTTONBOX_DEFAULT_STYLE, 10),
		cancel_but(u8"Cancel\ncard or timeout"),
		yellow_goal_but(u8"Goal Yellow!\n(KP_Div)"),
		yellow_subgoal_but(u8"−"),
		blue_goal_but(u8"Goal Blue!\n(KP_Mult)"),
		blue_subgoal_but(u8"−"),

		next_stage_label_left(u8"Change stage to:"),
		next_stage_label_right(u8""),
		firsthalf_start_but(u8"First Half"),
		halftime_start_but(u8"Half Time"),
		secondhalf_start_but(u8"Second Half"),
		overtime1_start_but(u8"Overtime1"),
		overtime2_start_but(u8"Overtime2"),
		penaltyshootout_start_but(u8"Penalty Shootout"),
		gameover_start_but(u8"End Game"),

		yellow_frame(u8"Yellow Team"),
		yellow_team_table(7, 2),
		yellow_timeout_time_label(u8"Timeout Clock: "),
		yellow_timeouts_left_label(u8"Timeouts left: "),
		yellow_goalie_label(u8"Goalie: "),
		yellow_kickoff_but(u8"Kickoff (KP_1)"),
		yellow_freekick_but(u8"Freekick (KP_7)"),
		yellow_penalty_but(u8"Penalty"),
		yellow_indirect_freekick_but(u8"Indirect (KP_4)"),

		blue_frame(u8"Blue Team"),
		blue_team_table(7, 2),
		blue_timeout_time_label(u8"Timeout Clock: "),
		blue_timeouts_left_label(u8"Timeouts left: "),
		blue_goalie_label(u8"Goalie: "),
		blue_kickoff_but(u8"Kickoff (KP_3)"),
		blue_freekick_but(u8"Freekick (KP_9)"),
		blue_penalty_but(u8"Penalty"),
		blue_indirect_freekick_but(u8"Indirect (KP_6)") {
	set_default_size(600, 700);
	set_title(u8"Small Size League - Referee Box");

	// Add Accelerator Buttons
	stop_but.add_accelerator(u8"activate", get_accel_group(), GDK_KP_0, Gdk::ModifierType(0), Gtk::AccelFlags(0));
	yellow_kickoff_but.add_accelerator(u8"activate", get_accel_group(), GDK_KP_1, Gdk::ModifierType(0), Gtk::AccelFlags(0));
	blue_kickoff_but.add_accelerator(u8"activate", get_accel_group(), GDK_KP_3, Gdk::ModifierType(0), Gtk::AccelFlags(0));
	yellow_indirect_freekick_but.add_accelerator(u8"activate", get_accel_group(), GDK_KP_4, Gdk::ModifierType(0), Gtk::AccelFlags(0));
	force_start_but.add_accelerator(u8"activate", get_accel_group(), GDK_KP_5, Gdk::ModifierType(0), Gtk::AccelFlags(0));
	blue_indirect_freekick_but.add_accelerator(u8"activate", get_accel_group(), GDK_KP_6, Gdk::ModifierType(0), Gtk::AccelFlags(0));
	yellow_freekick_but.add_accelerator(u8"activate", get_accel_group(), GDK_KP_7, Gdk::ModifierType(0), Gtk::AccelFlags(0));
	blue_freekick_but.add_accelerator(u8"activate", get_accel_group(), GDK_KP_9, Gdk::ModifierType(0), Gtk::AccelFlags(0));

	normal_start_but.add_accelerator(u8"activate", get_accel_group(), GDK_KP_Enter, Gdk::ModifierType(0), Gtk::AccelFlags(0));
	halt_but.add_accelerator(u8"activate", get_accel_group(), GDK_KP_Decimal, Gdk::ModifierType(0), Gtk::AccelFlags(0));
	halt_but.add_accelerator(u8"activate", get_accel_group(), GDK_KP_Separator, Gdk::ModifierType(0), Gtk::AccelFlags(0));

	yellow_goal_but.add_accelerator(u8"activate", get_accel_group(), GDK_KP_Divide, Gdk::ModifierType(0), Gtk::AccelFlags(0));
	blue_goal_but.add_accelerator(u8"activate", get_accel_group(), GDK_KP_Multiply, Gdk::ModifierType(0), Gtk::AccelFlags(0));

	// Goalie fields should not be editable because if the user types a number on the numpad, it will be treated as an accelerator for a command instead
	// This does not change the visual style, but it rejects all keyboard edits, so users will learn not to use the keyboard and will not accidentally hit a command key
	yellow_goalie_spin.set_editable(false);
	blue_goalie_spin.set_editable(false);

	// Menu
	Gtk::Menu::MenuList &menulist = config_menu.items();
	menulist.push_back(ignore_rules_menu_item);
	menu_bar.items().push_back(Gtk::Menu_Helpers::MenuElem(u8"_Config", config_menu));

	// Connecting the one million signals
	controller.signal_timeout_time_changed.connect(sigc::mem_fun(this, &MainWindow::on_timeout_time_changed));
	controller.signal_game_clock_changed.connect(sigc::mem_fun(this, &MainWindow::on_game_clock_changed));
	controller.signal_yellow_card_time_changed.connect(sigc::mem_fun(this, &MainWindow::on_yellow_card_time_changed));
	controller.signal_other_changed.connect(sigc::mem_fun(this, &MainWindow::on_other_changed));

	ignore_rules_menu_item.signal_toggled().connect(sigc::mem_fun(this, &MainWindow::update_sensitivities));

	halt_but.signal_clicked().connect(sigc::mem_fun(controller, &GameController::halt));   
	stop_but.signal_clicked().connect(sigc::mem_fun(controller, &GameController::stop));
	force_start_but.signal_clicked().connect(sigc::mem_fun(controller, &GameController::force_start));
	normal_start_but.signal_clicked().connect(sigc::mem_fun(controller, &GameController::normal_start));

	switch_colours_but.signal_clicked().connect(sigc::mem_fun(controller, &GameController::switch_colours));
	teamname_yellow.signal_changed().connect(sigc::mem_fun(this, &MainWindow::on_teamname_yellow_changed));
	teamname_blue.signal_changed().connect(sigc::mem_fun(this, &MainWindow::on_teamname_blue_changed));

	cancel_but.signal_clicked().connect(sigc::mem_fun(controller, &GameController::cancel_card_or_timeout));

	yellow_goal_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::award_goal), SaveState::TEAM_YELLOW));
	blue_goal_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::award_goal), SaveState::TEAM_BLUE));
	yellow_subgoal_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::subtract_goal), SaveState::TEAM_YELLOW));
	blue_subgoal_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::subtract_goal), SaveState::TEAM_BLUE));

	firsthalf_start_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::enter_stage), SSL_Referee::NORMAL_FIRST_HALF_PRE));
	secondhalf_start_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::enter_stage), SSL_Referee::NORMAL_SECOND_HALF_PRE));
	overtime1_start_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::enter_stage), SSL_Referee::EXTRA_FIRST_HALF_PRE));
	overtime2_start_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::enter_stage), SSL_Referee::EXTRA_SECOND_HALF_PRE));
	penaltyshootout_start_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::enter_stage), SSL_Referee::PENALTY_SHOOTOUT));
	gameover_start_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::enter_stage), SSL_Referee::POST_GAME));
	halftime_start_but.signal_clicked().connect(sigc::mem_fun(controller, &GameController::start_half_time));

	yellow_timeout_start_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::timeout_start), SaveState::TEAM_YELLOW));
	yellow_goalie_spin.signal_value_changed().connect(sigc::mem_fun(this, &MainWindow::on_goalie_yellow_changed));
	yellow_kickoff_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::prepare_kickoff), SaveState::TEAM_YELLOW));
	yellow_freekick_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::direct_free_kick), SaveState::TEAM_YELLOW));
	yellow_penalty_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::prepare_penalty), SaveState::TEAM_YELLOW));
	yellow_indirect_freekick_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::indirect_free_kick), SaveState::TEAM_YELLOW));
	yellow_yellowcard_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::yellow_card), SaveState::TEAM_YELLOW));
	yellow_redcard_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::red_card), SaveState::TEAM_YELLOW));

	blue_timeout_start_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::timeout_start), SaveState::TEAM_BLUE));
	blue_goalie_spin.signal_value_changed().connect(sigc::mem_fun(this, &MainWindow::on_goalie_blue_changed));
	blue_kickoff_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::prepare_kickoff), SaveState::TEAM_BLUE));
	blue_freekick_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::direct_free_kick), SaveState::TEAM_BLUE));
	blue_penalty_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::prepare_penalty), SaveState::TEAM_BLUE));
	blue_indirect_freekick_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::indirect_free_kick), SaveState::TEAM_BLUE));
	blue_yellowcard_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::yellow_card), SaveState::TEAM_BLUE));
	blue_redcard_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::red_card), SaveState::TEAM_BLUE));

	// Team frames
	yellow_goalie_spin.get_adjustment()->configure(0, 0, 255, 1, 10, 0);
	yellow_team_table.set_row_spacings(10);
	yellow_team_table.set_col_spacings(10);
	yellow_team_table.attach(yellow_timeout_start_but, 0, 1, 0, 1, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_timeout_time_label, 0, 1, 1, 2, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_timeout_time_text, 1, 2, 1, 2, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_timeouts_left_label, 0, 1, 2, 3, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_timeouts_left_text, 1, 2, 2, 3, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_goalie_label, 0, 1, 3, 4, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_goalie_spin, 1, 2, 3, 4, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_kickoff_but, 0, 1, 4, 5, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_penalty_but, 1, 2, 4, 5, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_freekick_but, 0, 1, 5, 6, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_indirect_freekick_but, 1, 2, 5, 6, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_yellowcard_but, 0, 1, 6, 7, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_redcard_but, 1, 2, 6, 7, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);

	yellow_frame.add(yellow_team_table);
	yellow_frame.modify_bg(Gtk::STATE_NORMAL , Gdk::Color(u8"yellow"));

	team_hbox.pack_start(yellow_frame, Gtk::PACK_EXPAND_WIDGET, 20);

	blue_goalie_spin.get_adjustment()->configure(0, 0, 255, 1, 10, 0);
	blue_team_table.set_row_spacings(10);
	blue_team_table.set_col_spacings(10);
	blue_team_table.attach(blue_timeout_start_but, 0, 1, 0, 1, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_timeout_time_label, 0, 1, 1, 2, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_timeout_time_text, 1, 2, 1, 2, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_timeouts_left_label, 0, 1, 2, 3, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_timeouts_left_text, 1, 2, 2, 3, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_goalie_label, 0, 1, 3, 4, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_goalie_spin, 1, 2, 3, 4, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_kickoff_but, 0, 1, 4, 5, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_penalty_but, 1, 2, 4, 5, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_freekick_but, 0, 1, 5, 6, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_indirect_freekick_but, 1, 2, 5, 6, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_yellowcard_but, 0, 1, 6, 7, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_redcard_but, 1, 2, 6, 7, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);

	blue_frame.add(blue_team_table);
	blue_frame.modify_bg(Gtk::STATE_NORMAL , Gdk::Color(u8"blue"));

	team_hbox.pack_start(blue_frame, Gtk::PACK_EXPAND_WIDGET, 20);

	// Start stop
	halt_stop_hbox.pack_start(halt_but, Gtk::PACK_EXPAND_WIDGET, 20);
	halt_stop_hbox.pack_start(stop_but, Gtk::PACK_EXPAND_WIDGET, 20);
	start_hbox.pack_start(force_start_but, Gtk::PACK_EXPAND_WIDGET, 20);
	start_hbox.pack_start(normal_start_but, Gtk::PACK_EXPAND_WIDGET, 20);

	// Gamestatus Font, etc
	Pango::AttrList pango_attr(u8"<span size=\"xx-large\" weight=\"ultrabold\">Halted Running Stopped 0123456789:</span>");
	game_status_label.set_attributes(pango_attr);
	time_label.set_attributes(pango_attr);
	Pango::AttrList pango_attr2(u8"<span size=\"80000\" weight=\"ultrabold\">0123456789:., </span>");
	yellow_goal.set_attributes(pango_attr2);
	vs.set_attributes(pango_attr2);
	blue_goal.set_attributes(pango_attr2);

	goal_hbox.pack_start(yellow_goal, Gtk::PACK_EXPAND_WIDGET, 10);
	goal_hbox.pack_start(vs, Gtk::PACK_EXPAND_WIDGET, 10);
	goal_hbox.pack_start(blue_goal, Gtk::PACK_EXPAND_WIDGET, 10);
	goal_vbox.add(goal_hbox);

	for (const Glib::ustring &team : controller.configuration.teams) {
		teamname_yellow.append(team);
		teamname_blue.append(team);
	}
	teamname_yellow.get_entry()->modify_base(Gtk::STATE_NORMAL, Gdk::Color(u8"lightyellow"));
	teamname_yellow.get_entry()->modify_base(Gtk::STATE_INSENSITIVE, Gdk::Color(u8"lightyellow"));
	teamname_yellow.get_entry()->set_alignment(Gtk::ALIGN_CENTER);
	teamname_blue.get_entry()->modify_base(Gtk::STATE_NORMAL, Gdk::Color(u8"lightblue"));
	teamname_blue.get_entry()->modify_base(Gtk::STATE_INSENSITIVE, Gdk::Color(u8"lightblue"));
	teamname_blue.get_entry()->set_alignment(Gtk::ALIGN_CENTER);
	teamname_hbox.pack_start(teamname_yellow, Gtk::PACK_EXPAND_WIDGET, 10);
	teamname_hbox.pack_start(switch_colours_but, Gtk::PACK_SHRINK);
	teamname_hbox.pack_start(teamname_blue,   Gtk::PACK_EXPAND_WIDGET, 10);

	goal_vbox.add(teamname_hbox);
	goal_frame.add(goal_vbox);

	game_control_box.set_homogeneous(true);
	game_control_box.pack_start(cancel_but, Gtk::PACK_EXPAND_WIDGET, 10, 10);

	yellow_goal_box.pack_start(yellow_goal_but, Gtk::PACK_EXPAND_WIDGET);
	yellow_goal_box.pack_start(yellow_subgoal_but, Gtk::PACK_EXPAND_WIDGET);
	blue_goal_box.pack_start(blue_goal_but, Gtk::PACK_EXPAND_WIDGET);
	blue_goal_box.pack_start(blue_subgoal_but, Gtk::PACK_EXPAND_WIDGET);

	game_control_box.pack_start(yellow_goal_box, Gtk::PACK_EXPAND_WIDGET, 10, 10);
	game_control_box.pack_start(blue_goal_box, Gtk::PACK_EXPAND_WIDGET, 10, 10);

	game_stage_control_left_vbox.pack_start(next_stage_label_left, Gtk::PACK_SHRINK);
	game_stage_control_right_vbox.pack_start(next_stage_label_right, Gtk::PACK_SHRINK);
	game_stage_control_left_vbox.pack_start(firsthalf_start_but, Gtk::PACK_SHRINK);
	game_stage_control_right_vbox.pack_start(halftime_start_but, Gtk::PACK_SHRINK);
	game_stage_control_left_vbox.pack_start(secondhalf_start_but, Gtk::PACK_SHRINK);
	game_stage_control_right_vbox.pack_start(overtime1_start_but, Gtk::PACK_SHRINK);
	game_stage_control_left_vbox.pack_start(overtime2_start_but, Gtk::PACK_SHRINK);
	game_stage_control_right_vbox.pack_start(penaltyshootout_start_but, Gtk::PACK_SHRINK);
	game_stage_control_left_vbox.pack_start(gameover_start_but, Gtk::PACK_SHRINK);

	game_status_vbox.pack_start(stage_label, Gtk::PACK_EXPAND_WIDGET);
	game_status_vbox.pack_start(time_label, Gtk::PACK_EXPAND_WIDGET);
	game_status_vbox.pack_start(timeleft_label, Gtk::PACK_EXPAND_WIDGET);
	game_status_vbox.pack_start(game_status_label, Gtk::PACK_EXPAND_WIDGET);
	game_status_hbox.pack_start(game_status_vbox, Gtk::PACK_EXPAND_WIDGET);
	game_status_hbox.pack_start(game_control_box, Gtk::PACK_EXPAND_WIDGET);
	game_status_hbox.pack_start(game_stage_control_left_vbox, Gtk::PACK_SHRINK);
	game_status_hbox.pack_start(game_stage_control_right_vbox, Gtk::PACK_SHRINK);
	game_control_frame.add(game_status_hbox);

	big_vbox.pack_start(menu_bar, Gtk::PACK_SHRINK);
	big_vbox.pack_start(halt_stop_hbox, Gtk::PACK_SHRINK, 10);
	big_vbox.pack_start(start_hbox, Gtk::PACK_SHRINK, 10);
	big_vbox.pack_start(goal_frame, Gtk::PACK_EXPAND_WIDGET, 10);   
	big_vbox.pack_start(game_control_frame, Gtk::PACK_SHRINK, 10);
	big_vbox.pack_start(team_hbox, Gtk::PACK_SHRINK, 10);

	add(big_vbox);

	on_timeout_time_changed();
	on_game_clock_changed();
	on_yellow_card_time_changed();
	on_other_changed();
	update_sensitivities();

	show_all();
}

void MainWindow::on_timeout_time_changed() {
	yellow_timeout_time_text.set_text(format_time_deciseconds(controller.state.referee().yellow().timeout_time()));
	blue_timeout_time_text.set_text(format_time_deciseconds(controller.state.referee().blue().timeout_time()));
}

void MainWindow::on_game_clock_changed() {
	// The penalty shootout renders in a special way; there is no game clock during that time, instead, it shows a penalty goal count.
	// Thus, only show the cloc if we are not in the penalty shootout.
	const SaveState &state = controller.state;
	if (state.referee().stage() != SSL_Referee::PENALTY_SHOOTOUT) {
		time_label.set_text(format_time_deciseconds(state.time_taken()));
		timeleft_label.set_text(format_time_deciseconds(state.referee().has_stage_time_left() ? state.referee().stage_time_left() : 0));
	}
}

void MainWindow::on_yellow_card_time_changed() {
	const SSL_Referee &ref = controller.state.referee();
	const SSL_Referee::TeamInfo *teams[2] = { &ref.yellow(), &ref.blue() };
	Gtk::Button *buttons[2] = { &yellow_yellowcard_but, &blue_yellowcard_but };
	for (std::size_t i = 0; i < 2; ++i) {
		int cards = teams[i]->yellow_card_times_size();
		if (cards == 0) {
			buttons[i]->set_label(u8"Yellow Card");
		} else {
			const Glib::ustring &first_time = format_time_deciseconds(teams[i]->yellow_card_times(0));
			if (cards == 1) {
				buttons[i]->set_label(first_time);
			} else {
				buttons[i]->set_label(Glib::ustring::compose(u8"%1 (+%2)", first_time, cards - 1));
			}
		}
	}
}

void MainWindow::on_other_changed() {
	on_timeout_time_changed();
	on_game_clock_changed();
	on_yellow_card_time_changed();
	update_sensitivities();

	// Grab the state.
	const SaveState &state = controller.state;
	const SSL_Referee &ref = state.referee();

	// Update goals.
	// Note we do not want to always subtract penalty_goals.
	// Although this is always zero until you reach the penalty shootout, you might leave the penalty shootout for another state that doesn’t show those goals.
	// Normally that state would be post-game, but with Ignore Rules enabled it could be another normal game state.
	// You would then want to lump those goals in and not lose them.
	yellow_goal.set_text(Glib::ustring::format(ref.yellow().score() - (ref.stage() == SSL_Referee::PENALTY_SHOOTOUT ? state.yellow_penalty_goals() : 0)));
	blue_goal.set_text(Glib::ustring::format(ref.blue().score() - (ref.stage() == SSL_Referee::PENALTY_SHOOTOUT ? state.blue_penalty_goals() : 0)));

	// Update team names.
	teamname_yellow.set_active_text(ref.yellow().name());
	teamname_yellow.get_entry()->set_text(ref.yellow().name());
	teamname_blue.set_active_text(ref.blue().name());
	teamname_blue.get_entry()->set_text(ref.blue().name());

	// Update stage and command.
	stage_label.set_text(STAGE_NAMES[ref.stage()]);
	game_status_label.set_text(COMMAND_NAMES[ref.command()]);

	// Update penalty goals if in penalty shootout.
	if (ref.stage() == SSL_Referee::PENALTY_SHOOTOUT) {
		time_label.set_text(Glib::ustring::compose(u8"Penalties:\n %1 - %2\n", state.yellow_penalty_goals(), state.blue_penalty_goals()));
		timeleft_label.set_text(u8"0:00.0");
	}

	// Update timeout start button texts based on whether a timeout is already running.
	yellow_timeout_start_but.set_label(state.has_timeout() && state.timeout().team() == SaveState::TEAM_YELLOW ? u8"Timeout Resume" : u8"Timeout Start");
	blue_timeout_start_but.set_label(state.has_timeout() && state.timeout().team() == SaveState::TEAM_BLUE ? u8"Timeout Resume" : u8"Timeout Start");

	// Update timeout counts.
	yellow_timeouts_left_text.set_text(Glib::ustring::format(ref.yellow().timeouts()));
	blue_timeouts_left_text.set_text(Glib::ustring::format(ref.blue().timeouts()));

	// Update the goalie indices.
	yellow_goalie_spin.set_value(ref.yellow().goalie());
	blue_goalie_spin.set_value(ref.blue().goalie());

	// If we are in post-game, we display the total number of yellow cards ever issued on the yellow card buttons.
	if (ref.stage() == SSL_Referee::POST_GAME) {
		yellow_yellowcard_but.set_label(Glib::ustring::compose(u8"Yellow Card (%1)", ref.yellow().yellow_cards()));
		blue_yellowcard_but.set_label(Glib::ustring::compose(u8"Yellow Card (%1)", ref.blue().yellow_cards()));
	}

	// Update red card counts.
	yellow_redcard_but.set_label(ref.yellow().red_cards() ? Glib::ustring::compose(u8"Red Card (%1)", ref.yellow().red_cards()) : u8"Red Card");
	blue_redcard_but.set_label(ref.blue().red_cards() ? Glib::ustring::compose(u8"Red Card (%1)", ref.blue().red_cards()) : u8"Red Card");
}

void MainWindow::on_teamname_yellow_changed() {
	controller.set_teamname(SaveState::TEAM_YELLOW, teamname_yellow.get_entry_text());
}

void MainWindow::on_teamname_blue_changed() {
	controller.set_teamname(SaveState::TEAM_BLUE, teamname_blue.get_entry_text());
}

// If we just updated the packet immediately, we would spam changes on every click of the arrow moving through all the intermediate goalie IDs.
// Instead, we only send the new goalie ID if the spinner has not been touched for more than one second, so it should jump directly from the old to the new value.
void MainWindow::on_goalie_yellow_changed() {
	goalie_yellow_commit_connection.disconnect();
	goalie_yellow_commit_connection = Glib::signal_timeout().connect(sigc::mem_fun(this, &MainWindow::on_goalie_yellow_commit), 1000);
}

void MainWindow::on_goalie_blue_changed() {
	goalie_blue_commit_connection.disconnect();
	goalie_blue_commit_connection = Glib::signal_timeout().connect(sigc::mem_fun(this, &MainWindow::on_goalie_blue_commit), 1000);
}

bool MainWindow::on_goalie_yellow_commit() {
	controller.set_goalie(SaveState::TEAM_YELLOW, yellow_goalie_spin.get_value_as_int());
	return false;
}

bool MainWindow::on_goalie_blue_commit() {
	controller.set_goalie(SaveState::TEAM_BLUE, blue_goalie_spin.get_value_as_int());
	return false;
}

void MainWindow::update_sensitivities() {
	// Extract the things we might need.
	const SaveState &ss = controller.state;
	const SSL_Referee &ref = ss.referee();
	bool is_normal_half = ref.stage() == SSL_Referee::NORMAL_FIRST_HALF || ref.stage() == SSL_Referee::NORMAL_SECOND_HALF || ref.stage() == SSL_Referee::EXTRA_FIRST_HALF || ref.stage() == SSL_Referee::EXTRA_SECOND_HALF;
	bool is_break = ref.stage() == SSL_Referee::NORMAL_HALF_TIME || ref.stage() == SSL_Referee::EXTRA_TIME_BREAK || ref.stage() == SSL_Referee::EXTRA_HALF_TIME || ref.stage() == SSL_Referee::PENALTY_SHOOTOUT_BREAK;
	bool is_pre = ref.stage() == SSL_Referee::NORMAL_FIRST_HALF_PRE || ref.stage() == SSL_Referee::NORMAL_SECOND_HALF_PRE || ref.stage() == SSL_Referee::EXTRA_FIRST_HALF_PRE || ref.stage() == SSL_Referee::EXTRA_SECOND_HALF_PRE;
	bool is_stopped = ref.command() == SSL_Referee::STOP || ref.command() == SSL_Referee::GOAL_YELLOW || ref.command() == SSL_Referee::GOAL_BLUE;
	bool is_timeout_running = ref.command() == SSL_Referee::TIMEOUT_YELLOW || ref.command() == SSL_Referee::TIMEOUT_BLUE;
	bool is_prepare_kickoff = ref.command() == SSL_Referee::PREPARE_KICKOFF_YELLOW || ref.command() == SSL_Referee::PREPARE_KICKOFF_BLUE;
	bool is_prepare_penalty = ref.command() == SSL_Referee::PREPARE_PENALTY_YELLOW || ref.command() == SSL_Referee::PREPARE_PENALTY_BLUE;
	bool is_pshootout = ref.stage() == SSL_Referee::PENALTY_SHOOTOUT;

	// In post-game, lock down *EVERYTHING*, even the Ignore Rules command.
	if (ref.stage() == SSL_Referee::POST_GAME) {
		ignore_rules_menu_item.set_active(false);
		ignore_rules_menu_item.set_sensitive(false);
	}

	// Check if we are ignoring rules.
	bool ign = ignore_rules_menu_item.get_active();

	// You can HALT any time you are not already halted.
	halt_but.set_sensitive(ign || ref.command() != SSL_Referee::HALT);

	// You can STOP any time you are not already stopped except in post-game.
	stop_but.set_sensitive(ign || (!is_stopped && ref.stage() != SSL_Referee::POST_GAME));

	// You can FORCE START any time you are stopped in a normal half or a break (for robot testing).
	force_start_but.set_sensitive(ign || ((is_normal_half || is_break) && is_stopped));

	// You can NORMAL START when you are preparing a prepared play (kickoff or penalty kick).
	normal_start_but.set_sensitive(ign || is_prepare_kickoff || is_prepare_penalty);

	// You can edit the game names when you are halted except post-game.
	teamname_yellow.set_sensitive(ign || (ref.stage() != SSL_Referee::POST_GAME && ref.command() == SSL_Referee::HALT));
	teamname_blue.set_sensitive(ign || (ref.stage() != SSL_Referee::POST_GAME && ref.command() == SSL_Referee::HALT));

	// You can switch colours when you are halted in a break or pre-half.
	switch_colours_but.set_sensitive(ign || (ref.command() == SSL_Referee::HALT && (is_break || is_pre)));

	// You can cancel a card whenever there is one outstanding.
	// You can cancel a timeout only when the timeout is running, not if it is in a nested HALT.
	// You can never cancel anything in post-game.
	cancel_but.set_sensitive(ign || (ref.stage() != SSL_Referee::POST_GAME && (controller.state.has_last_card() || is_timeout_running)));

	// You can award goals whenever you are stopped.
	yellow_goal_but.set_sensitive(ign || is_stopped);
	blue_goal_but.set_sensitive(ign || is_stopped);

	// You can subtract goals whenever you are stopped and the team has points.
	yellow_subgoal_but.set_sensitive(ign || (is_stopped && ref.yellow().score()));
	blue_subgoal_but.set_sensitive(ign || (is_stopped && ref.blue().score()));

	// You can get to half time whenever you are stopped in a normal half.
	halftime_start_but.set_sensitive(ign || (is_normal_half && is_stopped));

	// You can only get to first half when you are ignoring rules; otherwise, you start in first half and can never get back there later.
	firsthalf_start_but.set_sensitive(ign);

	// You can get to second half when you are in normal half time.
	secondhalf_start_but.set_sensitive(ign || ref.stage() == SSL_Referee::NORMAL_HALF_TIME);

	// You can get to overtime 1 when you are in extra time break.
	overtime1_start_but.set_sensitive(ign || ref.stage() == SSL_Referee::EXTRA_TIME_BREAK);

	// You can get to overtime 2 when you are in extra time half time.
	overtime2_start_but.set_sensitive(ign || ref.stage() == SSL_Referee::EXTRA_HALF_TIME);

	// You can get to penalty shootout when you are in penalty shootout break.
	penaltyshootout_start_but.set_sensitive(ign || ref.stage() == SSL_Referee::PENALTY_SHOOTOUT_BREAK);

	// You can get to post-game whenever you are stopped or halted except in post-game.
	// This might be needed at any time throughout the game, due to the stop-at-ten-points rule.
	gameover_start_but.set_sensitive(ign || (ref.stage() != SSL_Referee::POST_GAME && (is_stopped || ref.command() == SSL_Referee::HALT)));

	// A team can start a timeout whenever the game is stopped and not in a break or penalty shootout.
	// A team can *resume* a timeout whenever the game is halted and that team already had a timeout in progress before the halt.
	yellow_timeout_start_but.set_sensitive(ign || (!is_break && !is_pshootout && is_stopped) || (ref.command() == SSL_Referee::HALT && ss.has_timeout() && ss.timeout().team() == SaveState::TEAM_YELLOW));
	blue_timeout_start_but.set_sensitive(ign || (!is_break && !is_pshootout && is_stopped) || (ref.command() == SSL_Referee::HALT && ss.has_timeout() && ss.timeout().team() == SaveState::TEAM_BLUE));

	// A team can change goalies whenever the game is stopped or halted except in post-game.
	yellow_goalie_spin.set_sensitive(ign || (ref.stage() != SSL_Referee::POST_GAME && (ref.command() == SSL_Referee::HALT || is_stopped)));
	blue_goalie_spin.set_sensitive(ign || (ref.stage() != SSL_Referee::POST_GAME && (ref.command() == SSL_Referee::HALT || is_stopped)));

	// A team can take a kickoff whenever the game is stopped and not in a break or penalty shootout.
	yellow_kickoff_but.set_sensitive(ign || (!is_break && !is_pshootout && is_stopped));
	blue_kickoff_but.set_sensitive(ign || (!is_break && !is_pshootout && is_stopped));

	// A team can take a free or penalty kick whenever the game is stopped in a normal half.
	Gtk::Button *kick_buts[] = { &yellow_freekick_but, &blue_freekick_but, &yellow_indirect_freekick_but, &blue_indirect_freekick_but };
	for (Gtk::Button *but : kick_buts) {
		but->set_sensitive(ign || (is_normal_half && is_stopped));
	}

	// A team can take a penalty kick whenever the game is stopped in a normal half or during penalty shootout.
	yellow_penalty_but.set_sensitive(ign || ((is_normal_half || is_pshootout) && is_stopped));
	blue_penalty_but.set_sensitive(ign || ((is_normal_half || is_pshootout) && is_stopped));

	// A team can be given a yellow or red card whenever the game is stopped.
	Gtk::Button *card_buts[] = { &yellow_yellowcard_but, &blue_yellowcard_but, &yellow_redcard_but, &blue_redcard_but };
	for (Gtk::Button *but : card_buts) {
		but->set_sensitive(ign || is_stopped);
	}
}

