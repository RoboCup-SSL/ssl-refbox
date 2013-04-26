#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/menu.h>
#include <gtkmm/menubar.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/window.h>
#include <sigc++/connection.h>

class GameController;

class MainWindow : public Gtk::Window {
	public:
		MainWindow(GameController &controller);

	private:
		void on_timeout_time_changed();
		void on_game_clock_changed();
		void on_yellow_card_time_changed();
		void on_other_changed();

		void on_teamname_yellow_changed();
		void on_teamname_blue_changed();

		void on_goalie_yellow_changed();
		void on_goalie_blue_changed();

		bool on_goalie_yellow_commit();
		bool on_goalie_blue_commit();

		void update_sensitivities();

		// The game controller.
		GameController &controller;

		sigc::connection goalie_yellow_commit_connection, goalie_blue_commit_connection;

		// Elements
		Gtk::VBox big_vbox;

		Gtk::MenuBar menu_bar;
		Gtk::Menu config_menu;
		Gtk::CheckMenuItem ignore_rules_menu_item;

		Gtk::HBox halt_stop_hbox;
		Gtk::Button halt_but;
		Gtk::Button stop_but;
		Gtk::HBox start_hbox;
		Gtk::Button force_start_but;
		Gtk::Button normal_start_but;

		Gtk::Frame goal_frame;
		Gtk::VBox goal_vbox;
		Gtk::HBox goal_hbox;
		Gtk::Label yellow_goal;
		Gtk::Label blue_goal;
		Gtk::Label vs;
		Gtk::HBox teamname_hbox;
		Gtk::ComboBoxText teamname_yellow;
		Gtk::ComboBoxText teamname_blue;
		Gtk::Button switch_colours_but;

		Gtk::HBox game_status_hbox;
		Gtk::VBox game_status_vbox;

		Gtk::Frame game_control_frame;
		Gtk::Label stage_label;
		Gtk::Label time_label;
		Gtk::Label timeleft_label;
		Gtk::Label game_status_label;
		Gtk::VButtonBox game_control_box;
		Gtk::Button cancel_but;
		Gtk::HBox yellow_goal_box;
		Gtk::Button yellow_goal_but;
		Gtk::Button yellow_subgoal_but;
		Gtk::HBox blue_goal_box;
		Gtk::Button blue_goal_but;
		Gtk::Button blue_subgoal_but;

		Gtk::VBox game_stage_control_left_vbox;
		Gtk::VBox game_stage_control_right_vbox;
		Gtk::Label next_stage_label_left;
		Gtk::Label next_stage_label_right;
		Gtk::Button firsthalf_start_but;
		Gtk::Button halftime_start_but;
		Gtk::Button secondhalf_start_but;
		Gtk::Button overtime1_start_but;
		Gtk::Button overtime2_start_but;
		Gtk::Button penaltyshootout_start_but;
		Gtk::Button gameover_start_but;

		Gtk::HBox team_hbox;

		Gtk::Frame yellow_frame;
		Gtk::Table yellow_team_table;
		Gtk::Button yellow_timeout_start_but;
		Gtk::Label yellow_timeout_time_label;
		Gtk::Label yellow_timeout_time_text;
		Gtk::Label yellow_timeouts_left_label;
		Gtk::Label yellow_timeouts_left_text;
		Gtk::Label yellow_goalie_label;
		Gtk::SpinButton yellow_goalie_spin;
		Gtk::Button yellow_kickoff_but;
		Gtk::Button yellow_freekick_but;
		Gtk::Button yellow_penalty_but;
		Gtk::Button yellow_indirect_freekick_but;
		Gtk::Button yellow_yellowcard_but;
		Gtk::Button yellow_redcard_but;

		Gtk::Frame blue_frame;
		Gtk::Table blue_team_table;
		Gtk::Button blue_timeout_start_but;
		Gtk::Label blue_timeout_time_label;
		Gtk::Label blue_timeout_time_text;
		Gtk::Label blue_timeouts_left_label;
		Gtk::Label blue_timeouts_left_text;
		Gtk::Label blue_goalie_label;
		Gtk::SpinButton blue_goalie_spin;
		Gtk::Button blue_kickoff_but;
		Gtk::Button blue_freekick_but;
		Gtk::Button blue_penalty_but;
		Gtk::Button blue_indirect_freekick_but;
		Gtk::Button blue_yellowcard_but;
		Gtk::Button blue_redcard_but;
};

#endif

