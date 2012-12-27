#ifndef REFEREEMM_H
#define REFEREEMM_H

#include "dialog_gameover.h"
#include <gtkmm.h>

class EnableState;
class GameControl;

class Refereemm_Main_Window : public Gtk::Window
{
	public:
		Refereemm_Main_Window(GameControl&);

		// signale
		void on_exit_clicked();

		void on_start_button();
		void on_stop_button();
		void on_cancel();
		void on_halt();
		void on_ready();
		void on_switch_colors();


		void on_yellow_goal();
		void on_yellow_subgoal();
		void on_yellow_kickoff();
		void on_yellow_freekick();
		void on_yellow_penalty();
		void on_yellow_indirect_freekick();
		void on_yellow_timeout_start();
		void on_yellow_yellowcard();
		void on_yellow_redcard();
		void on_yellow_redcard_sub();

		void on_blue_goal();
		void on_blue_subgoal();
		void on_blue_kickoff();
		void on_blue_freekick();
		void on_blue_penalty();
		void on_blue_indirect_freekick();
		void on_blue_timeout_start();
		void on_blue_yellowcard();
		void on_blue_redcard();
		void on_blue_redcard_sub();

		void on_teamname_yellow();
		void on_teamname_blue();

		void on_firsthalf_start();
		void on_halftime_start();
		void on_secondhalf_start();
		void on_overtime1_start();
		void on_overtime2_start();
		void on_penaltyshootout_start();
		void on_gameover_start();

		void on_toggle_enable_commands();

		// Idle Function
		void idle();

		// Gui Sensitive
		void set_active_widgets(const EnableState&);


	protected:
		// Non GUI Elements
		GameControl& gamecontrol;

		// Dialog Windows
		Dialog_Gameover dialog_gameover;

		// Elements
		Gtk::CheckButton enable_commands_but;
		Gtk::Button start_but;
		Gtk::Button stop_but;
		Gtk::MenuBar menu_bar;
		Gtk::Menu file_m, config_m;
		Gtk::Label game_status_label;

		Gtk::Label stage_label;
		Gtk::Label time_label;
		Gtk::Label timeleft_label;



		Gtk::Frame goal_frame;
		Gtk::VBox  goal_vbox;
		Gtk::Label yellow_goal;
		Gtk::Label blue_goal;
		Gtk::Label vs;
		Gtk::Entry teamname_yellow;
		Gtk::Entry teamname_blue;
		Gtk::Button switch_colors_but;

		Gtk::HBox game_status_hbox;
		Gtk::VBox game_status_vbox;

		Gtk::Frame game_control_frame;
		Gtk::VButtonBox game_control_box;
		Gtk::HBox yellow_goal_box;
		Gtk::HBox blue_goal_box;   
		Gtk::Button yellow_goal_but;
		Gtk::Button blue_goal_but;
		Gtk::Button yellow_subgoal_but;
		Gtk::Button blue_subgoal_but;
		Gtk::Button cancel_but;
		Gtk::Button halt_but;
		Gtk::Button ready_but;

		//Game states
		Gtk::VBox game_stage_control_left_vbox;
		Gtk::VBox game_stage_control_right_vbox;
		Gtk::Label  next_stage_label_left;
		Gtk::Label  next_stage_label_right;
		Gtk::Button firsthalf_start_but;
		Gtk::Button halftime_start_but;
		Gtk::Button secondhalf_start_but;
		Gtk::Button overtime1_start_but;
		Gtk::Button overtime2_start_but;
		Gtk::Button penaltyshootout_start_but;
		Gtk::Button gameover_start_but;

		Gtk::Frame yellow_frame;
		Gtk::Button yellow_kickoff_but;
		Gtk::Button yellow_freekick_but;
		Gtk::Button yellow_penalty_but;
		Gtk::Button yellow_indirect_freekick_but;
		Gtk::Button yellow_timeout_start_but;
		Gtk::Label yellow_timeout_time_label;
		Gtk::Label yellow_timeout_time_text;
		Gtk::Label yellow_timeouts_left_label;
		Gtk::Label yellow_timeouts_left_text;
		Gtk::Button yellow_yellowcard_but;
		Gtk::HBox yellow_redcard_box;
		Gtk::Button yellow_redcard_but;
		Gtk::Button yellow_redcard_sub_but;
		Gtk::Label yellow_card_label;

		Gtk::Frame blue_frame;   
		Gtk::Button blue_kickoff_but;
		Gtk::Button blue_freekick_but;
		Gtk::Button blue_penalty_but;
		Gtk::Button blue_indirect_freekick_but;
		Gtk::Button blue_timeout_start_but;
		Gtk::Label blue_timeout_time_label;
		Gtk::Label blue_timeout_time_text;
		Gtk::Label blue_timeouts_left_label;
		Gtk::Label blue_timeouts_left_text;
		Gtk::Button blue_yellowcard_but;
		Gtk::HBox blue_redcard_box;
		Gtk::Button blue_redcard_but;
		Gtk::Button blue_redcard_sub_but;
		Gtk::Label blue_card_label;


		// Arrange Widget
		Gtk::VBox big_vbox;

		Gtk::HBox halt_stop_hbox;
		Gtk::HBox start_ready_hbox;
		Gtk::HBox goal_hbox;
		Gtk::HBox teamname_hbox;
		Gtk::HBox team_hbox;
		Gtk::Table yellow_team_table;
		Gtk::Table blue_team_table;
};

#endif

