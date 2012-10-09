#include "refereemm.h"
#include "dialog_gameover.h"
#include "gamecontrol.h"
#include "gameinfo.h"
#ifdef WIN32
#include "getopt.h"
#endif
#include <cmath>
#include <iomanip>
#include <iostream>

Refereemm_Main_Window::Refereemm_Main_Window(GameControl& gc_): 
	Gtk::Window(),
	gamecontrol(gc_),
	dialog_gameover(*this),
	start_but(u8"Force Start (KP_5)"),
	stop_but(u8"Stop Game (KP_0)"),
	game_status_label(u8"Stopped"),
	stage_label(u8"??.??:?? left"),
	time_label(u8"00.00:00"),
	timeleft_label(u8"??.??:?? left"),

	goal_frame(u8" Yellow vs. Blue "),
	yellow_goal(u8"00"),
	blue_goal(u8"00"),
	vs(u8":"),
	switch_colors_but(u8"<-->"),
	game_control_frame(u8"Game Status"),
	game_control_box(Gtk::BUTTONBOX_DEFAULT_STYLE, 10 ),
	yellow_goal_but(u8"Goal Yellow!\n(KP_Div)"),
	blue_goal_but(u8"Goal Blue!\n(KP_Mult)"),
	yellow_subgoal_but(u8"-"),
	blue_subgoal_but(u8"-"),
	cancel_but(u8"Cancel\ncard or timeout"),
	halt_but(u8"Halt (KP_Point)"),
	ready_but(u8"Normal Start (KP_Enter)"),

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
	yellow_kickoff_but(u8"Kickoff (KP_1)"),
	yellow_freekick_but(u8"Freekick (KP_7)"),
	yellow_penalty_but(u8"Penalty"),
	yellow_indirect_freekick_but(u8"Indirect (KP_4)"),
	yellow_timeout_start_but(u8"Timeout Start"),
	yellow_timeout_time_label(u8"Timeout Clock: "),
	yellow_timeouts_left_label(u8"Timeouts left: "),
	yellow_yellowcard_but(u8"Yellow Card"),
	yellow_redcard_but(u8"Red card"),
	yellow_card_label(u8"Red/Yellow Card"),
	blue_frame(u8"Blue Team"),
	blue_kickoff_but(u8"Kickoff (KP_3)"),
	blue_freekick_but(u8"Freekick (KP_9)"),
	blue_penalty_but(u8"Penalty"),
	blue_indirect_freekick_but(u8"Indirect (KP_6)"),
	blue_timeout_start_but(u8"Timeout Start"),
	blue_timeout_time_label(u8"Timeout Clock: "),
	blue_timeouts_left_label(u8"Timeouts left: "),
	blue_yellowcard_but(u8"Yellow Card"),
	blue_redcard_but(u8"Red card"),
	blue_card_label(u8"Red/Yellow Card")
{
	set_default_size(600,700);
	set_title(u8"Small Size League - Referee Box");

	// Add Accelerator Buttons
	stop_but.add_accelerator( u8"activate",get_accel_group(),
			GDK_KP_0,Gdk::ModifierType(0),Gtk::AccelFlags(0));
	yellow_kickoff_but.add_accelerator( u8"activate",get_accel_group(),
			GDK_KP_1,Gdk::ModifierType(0),Gtk::AccelFlags(0));
	blue_kickoff_but.add_accelerator( u8"activate",get_accel_group(),
			GDK_KP_3,Gdk::ModifierType(0),Gtk::AccelFlags(0));
	yellow_indirect_freekick_but.add_accelerator( u8"activate",get_accel_group(),
			GDK_KP_4,Gdk::ModifierType(0),Gtk::AccelFlags(0));
	start_but.add_accelerator( u8"activate",get_accel_group(),
			GDK_KP_5,Gdk::ModifierType(0),Gtk::AccelFlags(0));
	blue_indirect_freekick_but.add_accelerator( u8"activate",get_accel_group(),
			GDK_KP_6,Gdk::ModifierType(0),Gtk::AccelFlags(0));
	yellow_freekick_but.add_accelerator( u8"activate",get_accel_group(),
			GDK_KP_7,Gdk::ModifierType(0),Gtk::AccelFlags(0));
	blue_freekick_but.add_accelerator( u8"activate",get_accel_group(),
			GDK_KP_9,Gdk::ModifierType(0),Gtk::AccelFlags(0));

	ready_but.add_accelerator( u8"activate",get_accel_group(),
			GDK_KP_Enter,Gdk::ModifierType(0),Gtk::AccelFlags(0));
	halt_but.add_accelerator( u8"activate",get_accel_group(),
			GDK_KP_Decimal,Gdk::ModifierType(0),Gtk::AccelFlags(0));
	halt_but.add_accelerator( u8"activate",get_accel_group(),
			GDK_KP_Separator,Gdk::ModifierType(0),Gtk::AccelFlags(0));


	yellow_goal_but.add_accelerator( u8"activate",get_accel_group(),
			GDK_KP_Divide,Gdk::ModifierType(0),Gtk::AccelFlags(0));
	blue_goal_but.add_accelerator( u8"activate",get_accel_group(),
			GDK_KP_Multiply,Gdk::ModifierType(0),Gtk::AccelFlags(0));


	// Menu
	Gtk::Menu::MenuList& menulist = config_m.items();
	menulist.push_back( Gtk::Menu_Helpers::CheckMenuElem(u8"Enable Commands",
				sigc::mem_fun(*this, &Refereemm_Main_Window::on_toggle_enable_commands)));
	menulist = file_m.items();
	menulist.push_back( Gtk::Menu_Helpers::MenuElem(u8"Quit",
				sigc::mem_fun(*this, &Refereemm_Main_Window::on_exit_clicked) ) );

	menu_bar.items().push_back( Gtk::Menu_Helpers::MenuElem(u8"_Referee", file_m));
	menu_bar.items().push_back( Gtk::Menu_Helpers::MenuElem(u8"_Config", config_m));

	// Connecting the one million Signals
	switch_colors_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_switch_colors));
	start_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_start_button));
	stop_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_stop_button));
	halt_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_halt));   
	cancel_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_cancel));
	ready_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_ready));

	yellow_goal_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_yellow_goal));
	blue_goal_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_blue_goal));

	yellow_subgoal_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_yellow_subgoal));
	blue_subgoal_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_blue_subgoal));

	yellow_timeout_start_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_yellow_timeout_start));
	yellow_kickoff_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_yellow_kickoff));
	yellow_freekick_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_yellow_freekick));
	yellow_penalty_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_yellow_penalty));
	yellow_indirect_freekick_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_yellow_indirect_freekick));
	yellow_yellowcard_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_yellow_yellowcard));
	yellow_redcard_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_yellow_redcard));

	blue_timeout_start_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_blue_timeout_start));
	blue_kickoff_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_blue_kickoff));
	blue_freekick_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_blue_freekick));
	blue_penalty_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_blue_penalty));
	blue_indirect_freekick_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_blue_indirect_freekick));
	blue_yellowcard_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_blue_yellowcard));
	blue_redcard_but.signal_clicked().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_blue_redcard));

	//game state control
	firsthalf_start_but.signal_clicked().connect(
			sigc::mem_fun(*this, &Refereemm_Main_Window::on_firsthalf_start));
	halftime_start_but.signal_clicked().connect(
			sigc::mem_fun(*this, &Refereemm_Main_Window::on_halftime_start));
	secondhalf_start_but.signal_clicked().connect(
			sigc::mem_fun(*this, &Refereemm_Main_Window::on_secondhalf_start));
	overtime1_start_but.signal_clicked().connect(
			sigc::mem_fun(*this, &Refereemm_Main_Window::on_overtime1_start));
	overtime2_start_but.signal_clicked().connect(
			sigc::mem_fun(*this, &Refereemm_Main_Window::on_overtime2_start));
	penaltyshootout_start_but.signal_clicked().connect(
			sigc::mem_fun(*this, &Refereemm_Main_Window::on_penaltyshootout_start));
	gameover_start_but.signal_clicked().connect(
			sigc::mem_fun(*this, &Refereemm_Main_Window::on_gameover_start));

	//teamname changed
	teamname_yellow.signal_changed().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_teamname_yellow));
	teamname_blue.signal_changed().connect(sigc::mem_fun(*this, &Refereemm_Main_Window::on_teamname_blue));



	// Connecting the idle Signal
	Glib::signal_idle().connect( sigc::bind_return(sigc::mem_fun(*this, &Refereemm_Main_Window::idle), true));

	// Team Frames
	yellow_team_table.resize(7,2);
	yellow_team_table.set_row_spacings(10);
	yellow_team_table.set_col_spacings(10);
	yellow_team_table.attach(yellow_timeout_start_but, 0,1,0,1, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	yellow_team_table.attach(yellow_timeout_time_label, 0,1,1,2, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	yellow_team_table.attach(yellow_timeout_time_text, 1,2,1,2, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	yellow_team_table.attach(yellow_timeouts_left_label, 0,1,2,3, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	yellow_team_table.attach(yellow_timeouts_left_text, 1,2,2,3, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	yellow_team_table.attach(yellow_kickoff_but, 0,1,3,4, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	yellow_team_table.attach(yellow_penalty_but, 1,2,3,4, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	yellow_team_table.attach(yellow_freekick_but, 0,1,4,5, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	yellow_team_table.attach(yellow_indirect_freekick_but, 1,2,4,5, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);

	yellow_team_table.attach(yellow_card_label, 0,1,5,6, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	yellow_team_table.attach(yellow_yellowcard_but, 0,1,6,7, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	yellow_team_table.attach(yellow_redcard_but, 1,2,6,7, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);

	yellow_frame.add(yellow_team_table);
	yellow_frame.modify_bg(Gtk::STATE_NORMAL , Gdk::Color(u8"yellow"));

	team_hbox.pack_start( yellow_frame, Gtk::PACK_EXPAND_WIDGET, 20 );

	blue_team_table.resize(7,2);
	blue_team_table.set_row_spacings(10);
	blue_team_table.set_col_spacings(10);
	blue_team_table.attach(blue_timeout_start_but, 0,1,0,1, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	blue_team_table.attach(blue_timeout_time_label, 0,1,1,2, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	blue_team_table.attach(blue_timeout_time_text, 1,2,1,2, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	blue_team_table.attach(blue_timeouts_left_label, 0,1,2,3, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	blue_team_table.attach(blue_timeouts_left_text, 1,2,2,3, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	blue_team_table.attach(blue_kickoff_but, 0,1,3,4, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	blue_team_table.attach(blue_penalty_but, 1,2,3,4, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	blue_team_table.attach(blue_freekick_but, 0,1,4,5, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	blue_team_table.attach(blue_indirect_freekick_but, 1,2,4,5, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);

	blue_team_table.attach(blue_card_label, 0,1,5,6, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	blue_team_table.attach(blue_yellowcard_but, 0,1,6,7, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);
	blue_team_table.attach(blue_redcard_but, 1,2,6,7, Gtk::EXPAND| Gtk::FILL, Gtk::EXPAND| Gtk::FILL);


	blue_frame.add(blue_team_table);
	blue_frame.modify_bg(Gtk::STATE_NORMAL , Gdk::Color(u8"blue"));

	team_hbox.pack_start( blue_frame, Gtk::PACK_EXPAND_WIDGET, 20 );

	// Start stop
	halt_stop_hbox.pack_start(halt_but, Gtk::PACK_EXPAND_WIDGET, 20);
	halt_stop_hbox.pack_start(stop_but, Gtk::PACK_EXPAND_WIDGET, 20 );
	start_ready_hbox.pack_start(start_but, Gtk::PACK_EXPAND_WIDGET, 20 );
	start_ready_hbox.pack_start(ready_but, Gtk::PACK_EXPAND_WIDGET, 20);


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
	goal_vbox.add ( goal_hbox );

	teamname_yellow.modify_base(Gtk::STATE_NORMAL, Gdk::Color(u8"lightyellow"));
	teamname_yellow.set_has_frame(false);
	teamname_yellow.set_alignment(Gtk::ALIGN_CENTER);
	teamname_blue.modify_base(Gtk::STATE_NORMAL, Gdk::Color(u8"lightblue"));
	teamname_blue.set_has_frame(false);
	teamname_blue.set_alignment(Gtk::ALIGN_CENTER);
	teamname_hbox.pack_start(teamname_yellow, Gtk::PACK_EXPAND_WIDGET, 10);
	teamname_hbox.pack_start(switch_colors_but, Gtk::PACK_SHRINK);
	teamname_hbox.pack_start(teamname_blue,   Gtk::PACK_EXPAND_WIDGET, 10);

	goal_vbox.add ( teamname_hbox );
	goal_frame.add ( goal_vbox );

	game_control_box.set_homogeneous(true);
	game_control_box.pack_start(cancel_but, Gtk::PACK_EXPAND_WIDGET, 10,10);

	yellow_goal_box.pack_start(yellow_goal_but, Gtk::PACK_EXPAND_WIDGET);
	yellow_goal_box.pack_start(yellow_subgoal_but, Gtk::PACK_EXPAND_WIDGET);
	blue_goal_box.pack_start(blue_goal_but, Gtk::PACK_EXPAND_WIDGET);
	blue_goal_box.pack_start(blue_subgoal_but, Gtk::PACK_EXPAND_WIDGET);

	game_control_box.pack_start(yellow_goal_box, Gtk::PACK_EXPAND_WIDGET, 10,10);
	game_control_box.pack_start(blue_goal_box, Gtk::PACK_EXPAND_WIDGET, 10,10);


	game_stage_control_left_vbox.pack_start(next_stage_label_left, Gtk::PACK_SHRINK);
	game_stage_control_right_vbox.pack_start(next_stage_label_right, Gtk::PACK_SHRINK);
	game_stage_control_left_vbox.pack_start(firsthalf_start_but, Gtk::PACK_SHRINK);
	game_stage_control_right_vbox.pack_start(halftime_start_but, Gtk::PACK_SHRINK);
	game_stage_control_left_vbox.pack_start(secondhalf_start_but, Gtk::PACK_SHRINK);
	game_stage_control_right_vbox.pack_start(overtime1_start_but, Gtk::PACK_SHRINK);
	game_stage_control_left_vbox.pack_start(overtime2_start_but, Gtk::PACK_SHRINK);
	game_stage_control_right_vbox.pack_start(penaltyshootout_start_but, Gtk::PACK_SHRINK);
	game_stage_control_left_vbox.pack_start(gameover_start_but, Gtk::PACK_SHRINK);

	game_status_vbox.pack_start( stage_label, Gtk::PACK_EXPAND_WIDGET);
	game_status_vbox.pack_start( time_label, Gtk::PACK_EXPAND_WIDGET);
	game_status_vbox.pack_start( timeleft_label, Gtk::PACK_EXPAND_WIDGET);
	game_status_vbox.pack_start( game_status_label, Gtk::PACK_EXPAND_WIDGET);
	game_status_hbox.pack_start( game_status_vbox, Gtk::PACK_EXPAND_WIDGET);
	game_status_hbox.pack_start( game_control_box, Gtk::PACK_EXPAND_WIDGET);
	game_status_hbox.pack_start( game_stage_control_left_vbox, Gtk::PACK_SHRINK);
	game_status_hbox.pack_start( game_stage_control_right_vbox, Gtk::PACK_SHRINK);
	game_control_frame.add( game_status_hbox );

	big_vbox.pack_start( menu_bar, Gtk::PACK_SHRINK );
	big_vbox.pack_start( halt_stop_hbox, Gtk::PACK_SHRINK, 10 );
	big_vbox.pack_start( start_ready_hbox, Gtk::PACK_SHRINK, 10 );
	big_vbox.pack_start( goal_frame, Gtk::PACK_EXPAND_WIDGET, 10 );   
	big_vbox.pack_start( game_control_frame, Gtk::PACK_SHRINK, 10 );
	big_vbox.pack_start( team_hbox, Gtk::PACK_SHRINK, 10);

	add(big_vbox);

	show_all();
}

// signale
void Refereemm_Main_Window::on_exit_clicked()
{
	std::exit(0);
}

void Refereemm_Main_Window::on_switch_colors()
{
	gamecontrol.switchColors();
}

void Refereemm_Main_Window::on_toggle_enable_commands()
{
	gamecontrol.toggleEnable();   
}

void Refereemm_Main_Window::on_start_button()
{
	gamecontrol.setStart();   
}

void Refereemm_Main_Window::on_stop_button()
{
	gamecontrol.setStop();
}

void Refereemm_Main_Window::on_halt()
{
	gamecontrol.setHalt();
}

void Refereemm_Main_Window::on_cancel()
{
	gamecontrol.setCancel();
}

void Refereemm_Main_Window::on_ready()
{
	gamecontrol.setReady();
}

void Refereemm_Main_Window::on_yellow_goal()
{
	gamecontrol.goalScored(Yellow);
}

void Refereemm_Main_Window::on_blue_goal()
{
	gamecontrol.goalScored(Blue);
}

void Refereemm_Main_Window::on_yellow_subgoal()
{
	gamecontrol.removeGoal(Yellow);
}

void Refereemm_Main_Window::on_blue_subgoal()
{
	gamecontrol.removeGoal(Blue);
}

void Refereemm_Main_Window::on_yellow_kickoff()
{
	gamecontrol.setKickoff(Yellow);
}

void Refereemm_Main_Window::on_yellow_freekick()
{
	gamecontrol.setDirect(Yellow);
}

void Refereemm_Main_Window::on_yellow_penalty()
{
	gamecontrol.setPenalty(Yellow);
}

void Refereemm_Main_Window::on_yellow_indirect_freekick()
{
	gamecontrol.setIndirect(Yellow);
}

void Refereemm_Main_Window::on_yellow_yellowcard()
{
	gamecontrol.awardYellowCard(Yellow);
}

void Refereemm_Main_Window::on_yellow_redcard()
{
	gamecontrol.awardRedCard(Yellow);
}

void Refereemm_Main_Window::on_yellow_timeout_start()
{
	gamecontrol.beginTimeout(Yellow);
}

void Refereemm_Main_Window::on_blue_kickoff()
{
	gamecontrol.setKickoff(Blue);
}

void Refereemm_Main_Window::on_blue_freekick()
{
	gamecontrol.setDirect(Blue);
}

void Refereemm_Main_Window::on_blue_penalty()
{
	gamecontrol.setPenalty(Blue);
}

void Refereemm_Main_Window::on_blue_indirect_freekick()
{
	gamecontrol.setIndirect(Blue);
}


void Refereemm_Main_Window::on_blue_yellowcard()
{
	gamecontrol.awardYellowCard(Blue);
}

void Refereemm_Main_Window::on_blue_redcard()
{
	gamecontrol.awardRedCard(Blue);
}

void Refereemm_Main_Window::on_blue_timeout_start()
{
	gamecontrol.beginTimeout(Blue);
}

void Refereemm_Main_Window::on_teamname_yellow()
{
	gamecontrol.setTeamName(Yellow, teamname_yellow.get_text());
}

void Refereemm_Main_Window::on_teamname_blue()
{
	gamecontrol.setTeamName(Blue, teamname_blue.get_text());
}

void Refereemm_Main_Window::on_firsthalf_start()
{
	gamecontrol.beginFirstHalf();
}

void Refereemm_Main_Window::on_halftime_start()
{
	gamecontrol.beginHalfTime();
}

void Refereemm_Main_Window::on_secondhalf_start()
{
	gamecontrol.beginSecondHalf();
}

void Refereemm_Main_Window::on_overtime1_start()
{
	gamecontrol.beginOvertime1();
}

void Refereemm_Main_Window::on_overtime2_start()
{
	gamecontrol.beginOvertime2();
}

void Refereemm_Main_Window::on_penaltyshootout_start()
{
	gamecontrol.beginPenaltyShootout();
}

void Refereemm_Main_Window::on_gameover_start()
{
	gamecontrol.setHalt();
	const GameInfo &info = gamecontrol.getGameInfo();
	dialog_gameover.update_from_gameinfo(info);
	dialog_gameover.show();
}


void Refereemm_Main_Window::set_active_widgets(const EnableState& es)
{
	switch_colors_but.set_sensitive(es.switch_colors);
	halt_but.set_sensitive(es.halt);
	ready_but.set_sensitive(es.ready);
	stop_but.set_sensitive(es.stop);
	start_but.set_sensitive(es.start);
	cancel_but.set_sensitive(es.cancel);

	// Yellow
	yellow_kickoff_but.set_sensitive(es.kickoff[Yellow]);
	yellow_goal_but.set_sensitive(es.goal[Yellow]);   
	yellow_subgoal_but.set_sensitive(es.subgoal[Yellow]);   
	yellow_freekick_but.set_sensitive(es.direct[Yellow]);
	yellow_indirect_freekick_but.set_sensitive(es.indirect[Yellow]);
	yellow_penalty_but.set_sensitive(es.penalty[Yellow]);
	yellow_timeout_start_but.set_sensitive(es.timeout[Yellow]);
	yellow_yellowcard_but.set_sensitive(es.cards);
	yellow_redcard_but.set_sensitive(es.cards);

	//Blue Team
	blue_kickoff_but.set_sensitive(es.kickoff[Blue]);
	blue_goal_but.set_sensitive(es.goal[Blue]);   
	blue_subgoal_but.set_sensitive(es.subgoal[Blue]);
	blue_freekick_but.set_sensitive(es.direct[Blue]);
	blue_indirect_freekick_but.set_sensitive(es.indirect[Blue]);
	blue_penalty_but.set_sensitive(es.penalty[Blue]);
	blue_timeout_start_but.set_sensitive(es.timeout[Blue]);
	blue_yellowcard_but.set_sensitive(es.cards);
	blue_redcard_but.set_sensitive(es.cards);

	//Game stage control
	firsthalf_start_but.set_sensitive(es.stage_firsthalf);
	halftime_start_but.set_sensitive(es.stage_halftime);
	secondhalf_start_but.set_sensitive(es.stage_secondhalf);
	overtime1_start_but.set_sensitive(es.stage_overtime1);
	overtime2_start_but.set_sensitive(es.stage_overtime2);
	penaltyshootout_start_but.set_sensitive(es.stage_penaltyshootout);
	gameover_start_but.set_sensitive(es.stage_gameover);
}

void Refereemm_Main_Window::idle()
{
	// First get new time step
	gamecontrol.stepTime();
	const GameInfo &gi = gamecontrol.getGameInfo();

	// Update the Gui
	game_status_label.set_text( gi.getStateString() );

	teamname_blue.set_text( gi.data.teamnames[Blue] );
	teamname_yellow.set_text( gi.data.teamnames[Yellow] );


	if (gi.data.stage == PENALTY_SHOOTOUT) {
		// Needs extra handling.
		yellow_goal.set_text(Glib::ustring::format(std::setw(2), gi.data.goals[Yellow]));
		blue_goal.set_text(Glib::ustring::format(std::setw(2), gi.data.goals[Blue]));

		time_label.set_text(Glib::ustring::compose(u8"Penalties:\n %1 - %2\n", gi.data.penaltygoals[Yellow], gi.data.penaltygoals[Blue]));
	} else {
		yellow_goal.set_text(Glib::ustring::format(std::setw(2), gi.data.goals[Yellow]));
		blue_goal.set_text(Glib::ustring::format(std::setw(2), gi.data.goals[Blue]));

		//display game status (stage, time remaining, stage...)
		stage_label.set_text(gi.getStageString());

		time_label.set_text(format_time_deciseconds(gi.timeTaken()));

		timeleft_label.set_text(format_time_deciseconds(gi.timeRemaining()));
	}

	yellow_timeouts_left_text.set_text(Glib::ustring::format(gi.nrTimeouts(Yellow)));
	blue_timeouts_left_text.set_text(Glib::ustring::format(gi.nrTimeouts(Blue)));

	yellow_timeout_time_text.set_text(format_time_deciseconds(gi.timeoutRemaining(Yellow)));

	if (gi.penaltyTimeRemaining(Yellow) > std::chrono::high_resolution_clock::duration::zero()) {
		yellow_yellowcard_but.set_label(format_time_deciseconds(gi.penaltyTimeRemaining(Yellow)));
		yellow_yellowcard_but.set_sensitive(false);
	}
	else {
		yellow_yellowcard_but.set_label(u8"Yellow Card");
		yellow_yellowcard_but.set_sensitive(true);
	}

	blue_timeout_time_text.set_text(format_time_deciseconds(gi.timeoutRemaining(Blue)));

	if (gi.penaltyTimeRemaining(Blue) > std::chrono::high_resolution_clock::duration::zero()) {
		blue_yellowcard_but.set_label(format_time_deciseconds(gi.penaltyTimeRemaining(Blue)));
		blue_yellowcard_but.set_sensitive(false);
	}
	else {
		blue_yellowcard_but.set_label(u8"Yellow Card");
		blue_yellowcard_but.set_sensitive(true);
	}

	set_active_widgets(gamecontrol.getEnableState());

	Glib::usleep(100000);
}



int main(int argc, char* argv[])
{
	int c;
	std::string configfile = "referee.conf";
	std::string logfile = "gamecontrol";
	bool restart = false;


	// Let Gtk take it's arguments
	Gtk::Main kit(argc, argv);

	while ((c = getopt(argc, argv, "hC:l:r")) != EOF) {
		switch (c) {
			case 'C': configfile = optarg; break;
			case 'r': restart = true; break;
			case 'l': logfile = optarg; break;
			case 'h':
			default:
					  std::cerr << "\nSmallSize Referee Program\n";
					  std::cerr << "(c) RoboCup Federation, 2003\n";
					  std::cerr << "\nUSAGE\n";
					  std::cerr << "referee -[hC]\n";
					  std::cerr << "\t-h\t\tthis help message\n";
					  std::cerr << "\t-C <file>\tUse config file <file>\n";
					  std::cerr << "\t-l <file>\tUse logfile prefix <file>\n";
					  std::cerr << "\t-r \trestore previous game\n";
					  return 0;
		}
	}

	GameControl gamecontrol(configfile, logfile, restart);

	// setup the GUI
	Refereemm_Main_Window main(gamecontrol);
	kit.run(main);
	return 0;
}

