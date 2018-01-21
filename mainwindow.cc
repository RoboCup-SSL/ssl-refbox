#include "mainwindow.h"
#include "configuration.h"
#include "gamecontroller.h"
#include "rconsrv.h"
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
		u8"Placing (Y)",
		u8"Placing (B)",
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

const MainWindow::GameControlButtonInfo MainWindow::game_control_button_info[] = {
	{ &MainWindow::halt_but, -1, SSL_Referee::HALT },
	{ &MainWindow::stop_but, -1, SSL_Referee::STOP },
	{ &MainWindow::force_start_but, -1, SSL_Referee::FORCE_START },
	{ &MainWindow::normal_start_but, -1, SSL_Referee::NORMAL_START },
	{ &MainWindow::yellow_kickoff_but, -1, SSL_Referee::PREPARE_KICKOFF_YELLOW },
	{ &MainWindow::blue_kickoff_but, -1, SSL_Referee::PREPARE_KICKOFF_BLUE },
	{ &MainWindow::yellow_freekick_but, -1, SSL_Referee::DIRECT_FREE_YELLOW },
	{ &MainWindow::blue_freekick_but, -1, SSL_Referee::DIRECT_FREE_BLUE },
	{ &MainWindow::yellow_indirect_freekick_but, -1, SSL_Referee::INDIRECT_FREE_YELLOW },
	{ &MainWindow::blue_indirect_freekick_but, -1, SSL_Referee::INDIRECT_FREE_BLUE },
	{ &MainWindow::yellow_penalty_but, -1, SSL_Referee::PREPARE_PENALTY_YELLOW },
	{ &MainWindow::blue_penalty_but, -1, SSL_Referee::PREPARE_PENALTY_BLUE },
	{ &MainWindow::yellow_timeout_start_but, -1, SSL_Referee::TIMEOUT_YELLOW },
	{ &MainWindow::blue_timeout_start_but, -1, SSL_Referee::TIMEOUT_BLUE },
	{ &MainWindow::yellow_goal_but, -1, SSL_Referee::GOAL_YELLOW },
	{ &MainWindow::blue_goal_but, -1, SSL_Referee::GOAL_BLUE },

	{ &MainWindow::firsthalf_start_but, SSL_Referee::NORMAL_FIRST_HALF_PRE, -1 },
	{ &MainWindow::secondhalf_start_but, SSL_Referee::NORMAL_SECOND_HALF_PRE, -1 },
	{ &MainWindow::overtime1_start_but, SSL_Referee::EXTRA_FIRST_HALF_PRE, -1 },
	{ &MainWindow::overtime2_start_but, SSL_Referee::EXTRA_SECOND_HALF_PRE, -1 },
	{ &MainWindow::penaltyshootout_start_but, SSL_Referee::PENALTY_SHOOTOUT, -1 },
	{ &MainWindow::gameover_start_but, SSL_Referee::POST_GAME, -1 },
};

MainWindow::MainWindow(GameController &controller) :
		Gtk::Window(),
		controller(controller),

		ignore_rules_menu_item(u8"_Ignore Rules", true),
		enable_rcon_menu_item(u8"Enable _Remote Control", true),

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
		teamSide_yellow(teamSide_group, u8"Yellow on positive half"),
		teamSide_blue(teamSide_group, u8"Blue on positive half"),

		game_control_frame(u8"Game Status"),
		stage_label(u8"??.??:?? left"),
		time_label(u8"00.00:00"),
		timeleft_label(u8"??.??:?? left"),
		game_status_label(u8"Stopped"),
		game_control_box(Gtk::BUTTONBOX_DEFAULT_STYLE, 10),
		cancel_but(u8"Cancel\n"),
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
		yellow_freekick_but(u8"Direct (KP_7)"),
		yellow_penalty_but(u8"Penalty"),
		yellow_indirect_freekick_but(u8"Indirect (KP_4)"),

		blue_frame(u8"Blue Team"),
		blue_team_table(7, 2),
		blue_timeout_time_label(u8"Timeout Clock: "),
		blue_timeouts_left_label(u8"Timeouts left: "),
		blue_goalie_label(u8"Goalie: "),
		blue_kickoff_but(u8"Kickoff (KP_3)"),
		blue_freekick_but(u8"Direct (KP_9)"),
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

	// Add Tooltips
	halt_but.set_tooltip_text(u8"Robots must completely stop moving\nStage, card, and timeout timers freeze");
	force_start_but.set_tooltip_text(u8"Both teams may approach the ball");
	normal_start_but.set_tooltip_text(u8"Selected team may approach the ball");
	teamname_yellow.set_tooltip_text(u8"Name of yellow team");
	teamname_blue.set_tooltip_text(u8"Name of blue team");
	switch_colours_but.set_tooltip_text(u8"Teams switch marker colours");
	teamSide_yellow.set_tooltip_text(u8"The yellow team plays on the positive half of the field");
	teamSide_blue.set_tooltip_text(u8"The blue team plays on the positive half of the field");
	yellow_goal_but.set_tooltip_text(u8"Award goal to yellow");
	yellow_subgoal_but.set_tooltip_text(u8"Remove goal from yellow");
	blue_goal_but.set_tooltip_text(u8"Award goal to blue");
	blue_subgoal_but.set_tooltip_text(u8"Remove goal from blue");
	firsthalf_start_but.set_tooltip_text(u8"Prepare for first half");
	halftime_start_but.set_tooltip_text(u8"Start half-time");
	secondhalf_start_but.set_tooltip_text(u8"Prepare for second half");
	overtime1_start_but.set_tooltip_text(u8"Prepare for first overtime half");
	overtime2_start_but.set_tooltip_text(u8"Prepare for second overtime half");
	penaltyshootout_start_but.set_tooltip_text(u8"Start penalty shootout");
	gameover_start_but.set_tooltip_text(u8"End game");
	yellow_goalie_spin.set_tooltip_text(u8"Robot number of yellow goalie");
	yellow_kickoff_but.set_tooltip_text(u8"Prepare for yellow kickoff");
	yellow_freekick_but.set_tooltip_text(u8"Execute yellow direct free kick, corner kick, or goal kick");
	yellow_penalty_but.set_tooltip_text(u8"Prepare for yellow penalty kick");
	yellow_indirect_freekick_but.set_tooltip_text(u8"Execute yellow indirect free kick or throw-in");
	yellow_yellowcard_but.set_tooltip_text(u8"Show yellow card to yellow");
	yellow_redcard_but.set_tooltip_text(u8"Show red card to yellow");
	blue_goalie_spin.set_tooltip_text(u8"Robot number of blue goalie");
	blue_kickoff_but.set_tooltip_text(u8"Prepare for blue kickoff");
	blue_freekick_but.set_tooltip_text(u8"Execute blue direct free kick, corner kick, or goal kick");
	blue_penalty_but.set_tooltip_text(u8"Prepare for blue penalty kick");
	blue_indirect_freekick_but.set_tooltip_text(u8"Execute blue indirect free kick or throw-in");
	blue_yellowcard_but.set_tooltip_text(u8"Show yellow card to blue");
	blue_redcard_but.set_tooltip_text(u8"Show red card to blue");

	// Goalie fields should not be editable because if the user types a number on the numpad, it will be treated as an accelerator for a command instead
	// This does not change the visual style, but it rejects all keyboard edits, so users will learn not to use the keyboard and will not accidentally hit a command key
	yellow_goalie_spin.set_editable(false);
	blue_goalie_spin.set_editable(false);

	// Menu
	Gtk::Menu::MenuList &menulist = config_menu.items();
	menulist.push_back(ignore_rules_menu_item);
	menulist.push_back(enable_rcon_menu_item);
	menu_bar.items().push_back(Gtk::Menu_Helpers::MenuElem(u8"_Config", config_menu));
	enable_rcon_menu_item.set_sensitive(controller.configuration.rcon_port);

	// Connecting the one million signals
	controller.signal_timeout_time_changed.connect(sigc::mem_fun(this, &MainWindow::on_timeout_time_changed));
	controller.signal_game_clock_changed.connect(sigc::mem_fun(this, &MainWindow::on_game_clock_changed));
	controller.signal_yellow_card_time_changed.connect(sigc::mem_fun(this, &MainWindow::on_yellow_card_time_changed));
	controller.signal_teamname_changed.connect(sigc::mem_fun(this, &MainWindow::on_teamname_changed));
	controller.signal_other_changed.connect(sigc::mem_fun(this, &MainWindow::on_other_changed));

	ignore_rules_menu_item.signal_toggled().connect(sigc::mem_fun(this, &MainWindow::update_sensitivities));
	enable_rcon_menu_item.signal_toggled().connect(sigc::mem_fun(this, &MainWindow::on_enable_rcon_clicked));

	for (const GameControlButtonInfo &i : game_control_button_info) {
		(this->*i.button).signal_clicked().connect(sigc::bind(sigc::mem_fun(this, &MainWindow::on_game_control_button_clicked), &i));
	}

	switch_colours_but.signal_clicked().connect(sigc::mem_fun(controller, &GameController::switch_colours));
	teamSide_yellow.signal_clicked().connect(sigc::mem_fun(this, &MainWindow::on_switch_sides));
	teamname_yellow.signal_changed().connect(sigc::mem_fun(this, &MainWindow::on_teamname_yellow_changed));
	teamname_blue.signal_changed().connect(sigc::mem_fun(this, &MainWindow::on_teamname_blue_changed));

	cancel_but.signal_clicked().connect(sigc::mem_fun(controller, &GameController::cancel));

	yellow_subgoal_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::subtract_goal), SaveState::TEAM_YELLOW));
	blue_subgoal_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::subtract_goal), SaveState::TEAM_BLUE));

	halftime_start_but.signal_clicked().connect(sigc::mem_fun(this, &MainWindow::on_half_time_button_clicked));

	yellow_goalie_spin.signal_value_changed().connect(sigc::mem_fun(this, &MainWindow::on_goalie_yellow_changed));
	yellow_yellowcard_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::yellow_card), SaveState::TEAM_YELLOW));
	yellow_redcard_but.signal_clicked().connect(sigc::bind(sigc::mem_fun(controller, &GameController::red_card), SaveState::TEAM_YELLOW));

	blue_goalie_spin.signal_value_changed().connect(sigc::mem_fun(this, &MainWindow::on_goalie_blue_changed));
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
	yellow_team_table.attach(yellow_freekick_but, 0, 1, 4, 5, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_indirect_freekick_but, 0, 1, 5, 6, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_kickoff_but, 0, 1, 6, 7, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_penalty_but, 1, 2, 4, 5, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	yellow_team_table.attach(yellow_yellowcard_but, 1, 2, 5, 6, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
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
	blue_team_table.attach(blue_freekick_but, 0, 1, 4, 5, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_indirect_freekick_but, 0, 1, 5, 6, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_kickoff_but, 0, 1, 6, 7, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_penalty_but, 1, 2, 4, 5, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
	blue_team_table.attach(blue_yellowcard_but, 1, 2, 5, 6, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL);
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

	teamSide_hbox.pack_start(teamSide_yellow, Gtk::PACK_EXPAND_WIDGET, 10);
	teamSide_hbox.pack_start(teamSide_blue, Gtk::PACK_EXPAND_WIDGET, 10);

	goal_vbox.add(teamname_hbox);
	goal_vbox.add(teamSide_hbox);
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
	on_teamname_changed();
	on_other_changed();
	update_sensitivities();

	show_all();
}

MainWindow::~MainWindow() = default;

void MainWindow::on_timeout_time_changed() {
	yellow_timeout_time_text.set_text(format_time_deciseconds(controller.state.referee().yellow().timeout_time()));
	blue_timeout_time_text.set_text(format_time_deciseconds(controller.state.referee().blue().timeout_time()));
}

void MainWindow::on_game_clock_changed() {
	// The penalty shootout renders in a special way; there is no game clock during that time, instead, it shows a penalty goal count.
	// Thus, only show the clock if we are not in the penalty shootout.
	const SaveState &state = controller.state;
	if (state.referee().stage() != SSL_Referee::PENALTY_SHOOTOUT) {
		time_label.set_text(format_time_deciseconds(static_cast<uint64_t>(state.time_taken())));
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

void MainWindow::on_teamname_changed() {
	// Sensitivities may be affected because we may be operating in a mode where we prevent starting the game if a team name is missing.
	update_sensitivities();
}

void MainWindow::on_other_changed() {
	// If a goalie change has been requested but not yet committed, we must push that goalie change through now.
	// Otherwise, two things might happen:
	// (1) A new command might be issued with the old goalie number, when we really should have committed the new goalie number no later than when the new command is sent, and
	// (2) The new goalie number might be discarded because it is only present in the UI and not yet in the game state, and the UI will be updated from the game state.
	if (goalie_yellow_commit_connection) {
		goalie_yellow_commit_connection.disconnect();
		on_goalie_yellow_commit();
	}
	if (goalie_blue_commit_connection) {
		goalie_blue_commit_connection.disconnect();
		on_goalie_blue_commit();
	}

	// Update all the other things as well as they can be affected by these changes.
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
	{
		const Glib::ustring &yellow_name = ref.yellow().name(), &blue_name = ref.blue().name();
		teamname_yellow.set_active_text(yellow_name);
		teamname_yellow.get_entry()->set_text(yellow_name);
		teamname_blue.set_active_text(blue_name);
		teamname_blue.get_entry()->set_text(blue_name);
	}

	// Update stage and command.
	stage_label.set_text(STAGE_NAMES[ref.stage()]);
	game_status_label.set_text(COMMAND_NAMES[ref.command()]);

	// Update penalty goals if in penalty shootout.
	if (ref.stage() == SSL_Referee::PENALTY_SHOOTOUT) {
		time_label.set_text(Glib::ustring::compose(u8"Penalties:\n %1 - %2\n", state.yellow_penalty_goals(), state.blue_penalty_goals()));
		timeleft_label.set_text(u8"0:00.0");
	}

	// Update timeout start button texts based on whether a timeout is already running.
	if (state.has_timeout() && state.timeout().team() == SaveState::TEAM_YELLOW) {
		yellow_timeout_start_but.set_label(u8"Timeout Resume");
		yellow_timeout_start_but.set_tooltip_text(u8"Resume timer for currently running timeout");
	} else {
		yellow_timeout_start_but.set_label(u8"Timeout Start");
		yellow_timeout_start_but.set_tooltip_text(u8"Start timeout for yellow");
	}
	if (state.has_timeout() && state.timeout().team() == SaveState::TEAM_BLUE) {
		blue_timeout_start_but.set_label(u8"Timeout Resume");
		blue_timeout_start_but.set_tooltip_text(u8"Resume timer for currently running timeout");
	} else {
		blue_timeout_start_but.set_label(u8"Timeout Start");
		blue_timeout_start_but.set_tooltip_text(u8"Start timeout for blue");
	}

	// Update tooltip on stop button based on whether a timeout is running.
	if (state.has_timeout()) {
		stop_but.set_tooltip_text(u8"End current timeout and return to game\nRobots must keep distance from the ball");
	} else {
		stop_but.set_tooltip_text(u8"Robots must keep distance from the ball");
	}

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
	controller.set_goalie(SaveState::TEAM_YELLOW, static_cast<unsigned int>(yellow_goalie_spin.get_value_as_int()));
	return false;
}

bool MainWindow::on_goalie_blue_commit() {
	controller.set_goalie(SaveState::TEAM_BLUE, static_cast<unsigned int>(blue_goalie_spin.get_value_as_int()));
	return false;
}

void MainWindow::on_game_control_button_clicked(const GameControlButtonInfo *button) {
	if (button->new_stage != -1) {
		controller.enter_stage(static_cast<SSL_Referee::Stage>(button->new_stage));
	}
	if (button->new_command != -1) {
		controller.set_command(static_cast<SSL_Referee::Command>(button->new_command));
	}
}

void MainWindow::on_half_time_button_clicked() {
	controller.enter_stage(controller.next_half_time());
}

void MainWindow::on_enable_rcon_clicked() {
	if (enable_rcon_menu_item.get_active()) {
		rcon_server.reset(new RConServer(controller));
	} else {
		rcon_server.reset();
	}
}

void MainWindow::on_switch_sides() {
    controller.switch_sides(teamSide_blue.get_active());
}

void MainWindow::update_sensitivities() {
	// Extract the things we might need.
	const SaveState &ss = controller.state;
	const SSL_Referee &ref = ss.referee();

	// In post-game, lock down *EVERYTHING*, even the Ignore Rules command.
	if (ref.stage() == SSL_Referee::POST_GAME) {
		ignore_rules_menu_item.set_active(false);
		ignore_rules_menu_item.set_sensitive(false);
	}

	// Check if we are ignoring rules.
	bool ign = ignore_rules_menu_item.get_active();

	// Update sensitivities of the game control buttons based on whether the game controller allows the actions.
	for (const GameControlButtonInfo &i : game_control_button_info) {
		bool ok = true;
		if (i.new_stage != -1) {
			ok = ok && controller.can_enter_stage(static_cast<SSL_Referee::Stage>(i.new_stage));
		}
		if (i.new_command != -1) {
			ok = ok && controller.can_set_command(static_cast<SSL_Referee::Command>(i.new_command));
		}
		ok = ok || ign;
		(this->*i.button).set_sensitive(ok);
	}
	{
		SSL_Referee::Stage stage = controller.next_half_time();
		halftime_start_but.set_sensitive(ign || (stage != SSL_Referee::POST_GAME && controller.can_enter_stage(stage)));
	}
	{
		Gtk::Button *card_buts[] = { &yellow_yellowcard_but, &blue_yellowcard_but, &yellow_redcard_but, &blue_redcard_but };
		bool ok = ign || controller.can_issue_card();
		for (Gtk::Button *but : card_buts) {
			but->set_sensitive(ok);
		}
	}
	yellow_subgoal_but.set_sensitive(ign || controller.can_subtract_goal(SaveState::TEAM_YELLOW));
	blue_subgoal_but.set_sensitive(ign || controller.can_subtract_goal(SaveState::TEAM_BLUE));
	yellow_goalie_spin.set_sensitive(ign || controller.can_set_goalie());
	blue_goalie_spin.set_sensitive(ign || controller.can_set_goalie());
	switch_colours_but.set_sensitive(ign || controller.can_switch_colours());
	teamSide_blue.set_sensitive(ign || controller.can_switch_sides());
	teamSide_yellow.set_sensitive(ign || controller.can_switch_sides());

	// You can edit the game names when you are halted except post-game.
	teamname_yellow.set_sensitive(ign || (ref.stage() != SSL_Referee::POST_GAME && ref.command() == SSL_Referee::HALT));
	teamname_blue.set_sensitive(ign || (ref.stage() != SSL_Referee::POST_GAME && ref.command() == SSL_Referee::HALT));

	// Update the cancel button’s sensitivity, label, and tooltip based on what can be cancelled.
	switch (controller.cancel_type()) {
		case GameController::CancelType::NONE:
			cancel_but.set_sensitive(false);
			cancel_but.set_label(u8"Cancel\n");
			cancel_but.set_has_tooltip(false);
			break;

		case GameController::CancelType::CARD:
			cancel_but.set_sensitive();
			cancel_but.set_label(u8"Cancel\nlast card");
			cancel_but.set_tooltip_text(u8"Destroy last-issued yellow or red card\nas if card was never issued");
			break;

		case GameController::CancelType::TIMEOUT:
			cancel_but.set_sensitive();
			cancel_but.set_label(u8"Cancel\ncurrent timeout");
			cancel_but.set_tooltip_text(u8"Cancel timeout currently running as if\ntimeout was never taken; restore previous\ntimeouts left and remaining time");
			break;

		case GameController::CancelType::TIMEOUT_END:
			cancel_but.set_sensitive();
			cancel_but.set_label(u8"Cancel\nend of timeout");
			cancel_but.set_tooltip_text(u8"Cancel ending timeout most recently ended\nas if timeout was never ended; resume timeout");
			break;
	}
}

