#ifndef GAMECONTROLLER_H
#define GAMECONTROLLER_H

#include "noncopyable.h"
#include "referee.pb.h"
#include "savestate.pb.h"
#include "timing.h"
#include <cstdint>
#include <vector>
#include <glibmm/ustring.h>
#include <sigc++/connection.h>
#include <sigc++/signal.h>

class Configuration;
class GameInfo;
class Logger;
class Publisher;

class GameController : public NonCopyable {
	public:
		SaveState state;
		const Configuration &configuration;
		sigc::signal<void> signal_timeout_time_changed, signal_game_clock_changed, signal_yellow_card_time_changed, signal_other_changed;

		GameController(Logger &logger, const Configuration &configuration, const std::vector<Publisher *> &publishers, bool resume);
		~GameController();

		void enter_stage(SSL_Referee::Stage stage);
		void start_half_time();

		void halt();
		void stop();
		void force_start();
		void normal_start();

		void set_teamname(SaveState::Team team, const Glib::ustring &name);
		void set_goalie(SaveState::Team team, unsigned int goalie);
		void switch_colours();

		void award_goal(SaveState::Team team);
		void subtract_goal(SaveState::Team team);

		void cancel_card_or_timeout();
		void timeout_start(SaveState::Team team);

		void prepare_kickoff(SaveState::Team team);
		void direct_free_kick(SaveState::Team team);
		void indirect_free_kick(SaveState::Team team);
		void prepare_penalty(SaveState::Team team);

		void yellow_card(SaveState::Team team);
		void red_card(SaveState::Team team);

	private:
		Logger &logger;
		const std::vector<Publisher *> &publishers;
		sigc::connection tick_connection;
		MicrosecondCounter timer;
		uint64_t microseconds_since_last_state_save;

		bool tick();
		void set_command(SSL_Referee::Command command, bool no_signal = false);
		void advance_from_pre();
};

#endif

