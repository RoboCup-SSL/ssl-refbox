#include "gamecontroller.h"
#include "configuration.h"
#include "logger.h"
#include "publisher.h"
#include "savegame.h"
#include "teams.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <glibmm/convert.h>
#include <glibmm/main.h>
#include <glibmm/ustring.h>
#include <google/protobuf/descriptor.h>
#include <sigc++/functors/mem_fun.h>

namespace {
	const uint32_t STATE_SAVE_INTERVAL = 5000000UL;
}

GameController::GameController(Logger &logger, const Configuration &configuration, const std::vector<Publisher *> &publishers, bool resume) : configuration(configuration), logger(logger), publishers(publishers), tick_connection(Glib::signal_timeout().connect(sigc::mem_fun(this, &GameController::tick), 25)), microseconds_since_last_state_save(0) {
	if (resume) {
		std::ifstream ifs;
		ifs.exceptions(std::ios_base::badbit);
		ifs.open(configuration.save_filename, std::ios_base::in | std::ios_base::binary);
		if (!state.ParseFromIstream(&ifs)) {
			throw std::runtime_error(Glib::locale_from_utf8(Glib::ustring::compose(u8"Protobuf error loading saved game state from file \"%1\"!", Glib::filename_to_utf8(configuration.save_filename))));
		}
		ifs.close();
		set_command(SSL_Referee::HALT);
	} else {
		SSL_Referee &ref = *state.mutable_referee();
		ref.set_packet_timestamp(0);
		ref.set_stage(SSL_Referee::NORMAL_FIRST_HALF_PRE);
		ref.set_command(SSL_Referee::HALT);
		ref.set_command_counter(0);
		ref.set_command_timestamp(static_cast<uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())) * 1000000UL);

		for (unsigned int teami = 0; teami < 2; ++teami) {
			SaveState::Team team = static_cast<SaveState::Team>(teami);
			SSL_Referee::TeamInfo &ti = TeamMeta::ALL[team].team_info(ref);
			ti.set_name(u8"");
			ti.set_score(0);
			ti.set_red_cards(0);
			ti.set_yellow_cards(0);
			ti.set_timeouts(configuration.normal_timeouts);
			ti.set_timeout_time(static_cast<uint32_t>(configuration.normal_timeout_seconds * 1000000UL));
			ti.set_goalie(0);
		}

		state.set_yellow_penalty_goals(0);
		state.set_blue_penalty_goals(0);
		state.set_time_taken(0);
	}
}

GameController::~GameController() {
	// Disconnect the timer connection.
	tick_connection.disconnect();

	// Try to save the current game state.
	try {
		save_game(state, configuration.save_filename);
	} catch (...) {
		// Swallow exceptions.
	}
}

void GameController::enter_stage(SSL_Referee::Stage stage) {
	SSL_Referee &ref = *state.mutable_referee();

	// Record what’s happening.
	logger.write(Glib::ustring::compose(u8"Entering new stage %1", SSL_Referee::Stage_descriptor()->FindValueByNumber(stage)->name()));

	// Set the new stage.
	ref.set_stage(stage);

	// Reset the stage time taken.
	state.set_time_taken(0);

	// Set or remove the stage time left as appropriate for the stage.
	switch (stage) {
		case SSL_Referee::NORMAL_FIRST_HALF:      ref.set_stage_time_left(configuration.normal_half_seconds        * 1000000); break;
		case SSL_Referee::NORMAL_HALF_TIME:       ref.set_stage_time_left(configuration.normal_half_time_seconds   * 1000000); break;
		case SSL_Referee::NORMAL_SECOND_HALF:     ref.set_stage_time_left(configuration.normal_half_seconds        * 1000000); break;
		case SSL_Referee::EXTRA_TIME_BREAK:       ref.set_stage_time_left(configuration.overtime_break_seconds     * 1000000); break;
		case SSL_Referee::EXTRA_FIRST_HALF:       ref.set_stage_time_left(configuration.overtime_half_seconds      * 1000000); break;
		case SSL_Referee::EXTRA_HALF_TIME:        ref.set_stage_time_left(configuration.overtime_half_time_seconds * 1000000); break;
		case SSL_Referee::EXTRA_SECOND_HALF:      ref.set_stage_time_left(configuration.overtime_half_seconds      * 1000000); break;
		case SSL_Referee::PENALTY_SHOOTOUT_BREAK: ref.set_stage_time_left(configuration.shootout_break_seconds     * 1000000); break;
		default: ref.clear_stage_time_left(); break;
	}

	// If we’re going into a pre-game state before either the normal game or overtime, reset the timeouts.
	if (stage == SSL_Referee::NORMAL_FIRST_HALF_PRE || stage == SSL_Referee::EXTRA_FIRST_HALF_PRE) {
		unsigned int count = stage == SSL_Referee::NORMAL_FIRST_HALF_PRE ? configuration.normal_timeouts : configuration.overtime_timeouts;
		unsigned int seconds = stage == SSL_Referee::NORMAL_FIRST_HALF_PRE ? configuration.normal_timeout_seconds : configuration.overtime_timeout_seconds;
		for (unsigned int teami = 0; teami < 2; ++teami) {
			SSL_Referee::TeamInfo &ti = TeamMeta::ALL[teami].team_info(ref);
			ti.set_timeouts(count);
			ti.set_timeout_time(static_cast<uint32_t>(seconds * 1000000UL));
		}
	}

	// Nearly all stage entries correspond to a transition to HALT.
	// The exceptions are game half entries, where a NORMAL START accompanies the entry in an atomic transition from kickoff.
	bool is_half = stage == SSL_Referee::NORMAL_FIRST_HALF || stage == SSL_Referee::NORMAL_SECOND_HALF || stage == SSL_Referee::EXTRA_FIRST_HALF || stage == SSL_Referee::EXTRA_SECOND_HALF;
	if (!is_half) {
		set_command(SSL_Referee::HALT, true);
	}

	// We should save the game state now.
	save_game(state, configuration.save_filename);

	signal_other_changed.emit();
}

void GameController::start_half_time() {
	SSL_Referee &ref = *state.mutable_referee();

	// Which stage to go into depends on which stage we are already in.
	switch (ref.stage()) {
		case SSL_Referee::NORMAL_FIRST_HALF_PRE:
		case SSL_Referee::NORMAL_FIRST_HALF:
			enter_stage(SSL_Referee::NORMAL_HALF_TIME);
			break;
		case SSL_Referee::NORMAL_HALF_TIME:
		case SSL_Referee::NORMAL_SECOND_HALF_PRE:
		case SSL_Referee::NORMAL_SECOND_HALF:
			enter_stage(SSL_Referee::EXTRA_TIME_BREAK);
			break;
		case SSL_Referee::EXTRA_TIME_BREAK:
		case SSL_Referee::EXTRA_FIRST_HALF_PRE:
		case SSL_Referee::EXTRA_FIRST_HALF:
			enter_stage(SSL_Referee::EXTRA_HALF_TIME);
			break;
		case SSL_Referee::EXTRA_HALF_TIME:
		case SSL_Referee::EXTRA_SECOND_HALF_PRE:
		case SSL_Referee::EXTRA_SECOND_HALF:
		case SSL_Referee::PENALTY_SHOOTOUT_BREAK:
		case SSL_Referee::PENALTY_SHOOTOUT:
			enter_stage(SSL_Referee::PENALTY_SHOOTOUT_BREAK);
			break;
		case SSL_Referee::POST_GAME:
			break;
	}
}

void GameController::halt() {
	set_command(SSL_Referee::HALT);
}

void GameController::stop() {
	state.clear_timeout();
	set_command(SSL_Referee::STOP);
}

void GameController::force_start() {
	advance_from_pre();
	set_command(SSL_Referee::FORCE_START);
}

void GameController::normal_start() {
	advance_from_pre();
	set_command(SSL_Referee::NORMAL_START);
}

void GameController::set_teamname(SaveState::Team team, const Glib::ustring &name) {
	SSL_Referee &ref = *state.mutable_referee();
	TeamMeta::ALL[team].team_info(ref).set_name(name.raw());
}

void GameController::set_goalie(SaveState::Team team, unsigned int goalie) {
	SSL_Referee &ref = *state.mutable_referee();
	TeamMeta::ALL[team].team_info(ref).set_goalie(goalie);
}

void GameController::switch_colours() {
	logger.write(u8"Switching colours.");
	SSL_Referee &ref = *state.mutable_referee();

	// Swap the TeamInfo structures.
	ref.yellow().GetReflection()->Swap(ref.mutable_yellow(), ref.mutable_blue());

	// Swap the team to which the last card was given (which can be cancelled), if present.
	if (state.has_last_card()) {
		state.mutable_last_card()->set_team(TeamMeta::ALL[state.last_card().team()].other());
	}

	// Swap which team is currently in a timeout, if any.
	if (state.has_timeout()) {
		state.mutable_timeout()->set_team(TeamMeta::ALL[state.timeout().team()].other());
	}

	signal_other_changed.emit();
}

void GameController::award_goal(SaveState::Team team) {
	SSL_Referee &ref = *state.mutable_referee();
	SSL_Referee::TeamInfo &ti = TeamMeta::ALL[team].team_info(ref);

	// Increase the team’s score.
	ti.set_score(ti.score() + 1);

	// Increase the team’s number of penalty goals if in a penalty shootout.
	if (ref.stage() == SSL_Referee::PENALTY_SHOOTOUT) {
		TeamMeta::ALL[team].set_penalty_goals(state, TeamMeta::ALL[team].penalty_goals(state) + 1);
	}

	// Issue the command.
	set_command(TeamMeta::ALL[team].GOAL_COMMAND);
}

void GameController::subtract_goal(SaveState::Team team) {
	SSL_Referee &ref = *state.mutable_referee();
	SSL_Referee::TeamInfo &ti = TeamMeta::ALL[team].team_info(ref);

	// Subtract a goal.
	if (ti.score()) {
		ti.set_score(ti.score() - 1);
	}

	// If we are in the penalty shootout and have penalty goals, decrement that count as well.
	if (TeamMeta::ALL[team].penalty_goals(state)) {
		TeamMeta::ALL[team].set_penalty_goals(state, TeamMeta::ALL[team].penalty_goals(state) - 1);
	}

	signal_other_changed.emit();
}

void GameController::cancel_card_or_timeout() {
	SSL_Referee &ref = *state.mutable_referee();

	if (ref.command() == SSL_Referee::TIMEOUT_YELLOW || ref.command() == SSL_Referee::TIMEOUT_BLUE) {
		// A timeout is active; cancel it.
		SaveState::Team team = TeamMeta::command_team(ref.command());
		logger.write(Glib::ustring::compose(u8"Cancelling %1 timeout.", TeamMeta::ALL[team].COLOUR));
		SSL_Referee::TeamInfo &ti = TeamMeta::ALL[team].team_info(ref);
		ti.set_timeouts(ti.timeouts() + 1);
		ti.set_timeout_time(state.timeout().left_before());
		stop();
	} else if (state.has_last_card()) {
		// A card is active; cancel it.
		SSL_Referee::TeamInfo &ti = TeamMeta::ALL[state.last_card().team()].team_info(ref);
		switch (state.last_card().card()) {
			case SaveState::CARD_YELLOW:
				if (ti.yellow_card_times_size()) {
					logger.write(Glib::ustring::compose(u8"Cancelling yellow card for %1.", TeamMeta::ALL[state.last_card().team()].COLOUR));
					ti.mutable_yellow_card_times()->RemoveLast();
					ti.set_yellow_cards(ti.yellow_cards() - 1);
				}
				break;

			case SaveState::CARD_RED:
				logger.write(Glib::ustring::compose(u8"Cancelling red card for %1.", TeamMeta::ALL[state.last_card().team()].COLOUR));
				ti.set_red_cards(ti.red_cards() - 1);
				break;
		}
		state.clear_last_card();
	}

	signal_other_changed.emit();
	return;
}

void GameController::timeout_start(SaveState::Team team) {
	SSL_Referee &ref = *state.mutable_referee();
	SSL_Referee::TeamInfo &ti = TeamMeta::ALL[team].team_info(ref);

	// Only update any of the accounting if there is not already a record of an in-progress timeout.
	// This allows to issue HALT during a timeout, then resume the running timeout, without eating up another of the team’s timeouts and without affecting the Cancel button.
	// If that happens, during HALT there will still be a record of a running timeout.
	if (!(state.has_timeout() && state.timeout().team() == team)) {
		ti.set_timeouts(ti.timeouts() - 1);
		state.mutable_timeout()->set_team(team);
		state.mutable_timeout()->set_left_before(ti.timeout_time());
	}

	set_command(TeamMeta::ALL[team].TIMEOUT_COMMAND);
}

void GameController::prepare_kickoff(SaveState::Team team) {
	set_command(TeamMeta::ALL[team].PREPARE_KICKOFF_COMMAND);
}

void GameController::direct_free_kick(SaveState::Team team) {
	set_command(TeamMeta::ALL[team].DIRECT_FREE_COMMAND);
}

void GameController::indirect_free_kick(SaveState::Team team) {
	set_command(TeamMeta::ALL[team].INDIRECT_FREE_COMMAND);
}

void GameController::prepare_penalty(SaveState::Team team) {
	set_command(TeamMeta::ALL[team].PREPARE_PENALTY_COMMAND);
}

void GameController::yellow_card(SaveState::Team team) {
	SSL_Referee &ref = *state.mutable_referee();
	SSL_Referee::TeamInfo &ti = TeamMeta::ALL[team].team_info(ref);
	logger.write(Glib::ustring::compose(u8"Issuing yellow card to %1.", TeamMeta::ALL[team].COLOUR));

	// Add the yellow card.
	ti.add_yellow_card_times(configuration.yellow_card_seconds * 1000000U);
	ti.set_yellow_cards(ti.yellow_cards() + 1);

	// Record the card as the last card issued.
	state.mutable_last_card()->set_team(team);
	state.mutable_last_card()->set_card(SaveState::CARD_YELLOW);

	signal_other_changed.emit();
}

void GameController::red_card(SaveState::Team team) {
	SSL_Referee &ref = *state.mutable_referee();
	SSL_Referee::TeamInfo &ti = TeamMeta::ALL[team].team_info(ref);
	logger.write(Glib::ustring::compose(u8"Issuing red card to %1.", TeamMeta::ALL[team].COLOUR));

	// Add the red card.
	ti.set_red_cards(ti.red_cards() + 1);

	// Record the card as the last card issued.
	state.mutable_last_card()->set_team(team);
	state.mutable_last_card()->set_card(SaveState::CARD_RED);

	signal_other_changed.emit();
}

bool GameController::tick() {
	SSL_Referee &ref = *state.mutable_referee();

	// Read how many microseconds passed since the last tick.
	uint32_t delta = timer.read_and_reset();

	// Update the time since last state save and save if necessary.
	microseconds_since_last_state_save += delta;
	if (microseconds_since_last_state_save > STATE_SAVE_INTERVAL) {
		microseconds_since_last_state_save = 0;
		save_game(state, configuration.save_filename);
	}

	// Pull out the current command for checking against.
	SSL_Referee::Command command = ref.command();

	// Check if this is a half-time-like stage.
	SSL_Referee::Stage stage = ref.stage();
	bool half_time_like = stage == SSL_Referee::NORMAL_HALF_TIME || stage == SSL_Referee::EXTRA_TIME_BREAK || stage == SSL_Referee::EXTRA_HALF_TIME || stage == SSL_Referee::PENALTY_SHOOTOUT_BREAK;

	// Check if this is a pre-game stage.
	bool pre_game = stage == SSL_Referee::NORMAL_FIRST_HALF_PRE || stage == SSL_Referee::NORMAL_SECOND_HALF_PRE || stage == SSL_Referee::EXTRA_FIRST_HALF_PRE || stage == SSL_Referee::EXTRA_SECOND_HALF_PRE;

	// Run some clocks.
	if (command == SSL_Referee::TIMEOUT_YELLOW || command == SSL_Referee::TIMEOUT_BLUE) {
		// While a team is in a timeout, only its timeout clock runs.
		SaveState::Team team = TeamMeta::command_team(command);
		SSL_Referee::TeamInfo &ti = TeamMeta::ALL[team].team_info(ref);
		uint32_t old_left = ti.timeout_time();
		uint32_t old_tenths = old_left / 100000;
		uint32_t new_left = old_left > delta ? old_left - delta : 0;
		uint32_t new_tenths = new_left / 100000;
		ti.set_timeout_time(new_left);
		if (new_tenths != old_tenths) {
			signal_timeout_time_changed.emit();
		}
	} else if (command != SSL_Referee::HALT || half_time_like) {
		// Otherwise, as long as we are not in halt OR we are in a half-time-like stage, the stage clock runs, if this particular stage *has* a stage clock.
		// There are two game clocks, the stage time left clock and the stage time taken clock.
		// The stage time left clock may or may not exist; the stage time taken clock always exists.
		// Keep both in lockstep.
		{
			bool emit = false;
			if (ref.has_stage_time_left()) {
				int32_t old_left = ref.stage_time_left();
				int32_t old_tenths = old_left / 100000;
				int32_t new_left = old_left - delta;
				int32_t new_tenths = new_left / 100000;
				ref.set_stage_time_left(new_left);
				if (new_tenths != old_tenths) {
					emit = true;
				}
			}
			{
				uint64_t old_taken = state.time_taken();
				uint64_t old_tenths = old_taken / 100000;
				uint64_t new_taken = old_taken + delta;
				uint64_t new_tenths = new_taken / 100000;
				state.set_time_taken(new_taken);
				if (new_tenths != old_tenths) {
					emit = true;
				}
			}
			if (emit) {
				signal_game_clock_changed.emit();
			}
		}

		// Also, in all of these states except a half-time-like or pre-game state, yellow cards count down.
		if (!half_time_like && !pre_game) {
			for (unsigned int teami = 0; teami < 2; ++teami) {
				SaveState::Team team = static_cast<SaveState::Team>(teami);
				SSL_Referee::TeamInfo &ti = TeamMeta::ALL[team].team_info(ref);
				if (ti.yellow_card_times_size()) {
					// Tick down all the counters.
					bool emit = false;
					for (int j = 0; j < ti.yellow_card_times_size(); ++j) {
						uint32_t old_left = ti.yellow_card_times(j);
						uint32_t old_tenths = old_left / 100000;
						uint32_t new_left = old_left > delta ? old_left - delta : 0;
						uint32_t new_tenths = new_left / 100000;
						ti.set_yellow_card_times(j, new_left);
						if (!j && new_tenths != old_tenths) {
							emit = true;
						}
					}

					// Remove any that are at zero.
					if (!ti.yellow_card_times(0)) {
						auto last_valid = std::remove(ti.mutable_yellow_card_times()->begin(), ti.mutable_yellow_card_times()->end(), 0);
						ti.mutable_yellow_card_times()->Truncate(static_cast<int>(last_valid - ti.mutable_yellow_card_times()->begin()));
						emit = true;
					}

					// If we have reached zero yellow cards for this team, we may need to clear the save state’s idea of the last issued card so it doesn’t try to cancel a missing card.
					if (!ti.yellow_card_times_size()) {
						if (state.has_last_card()) {
							if (state.last_card().team() == team && state.last_card().card() == SaveState::CARD_YELLOW) {
								state.clear_last_card();
								signal_other_changed.emit();
							}
						}
					}

					if (emit) {
						signal_yellow_card_time_changed.emit();
					}
				}
			}
		}
	}

	// Publish the current state.
	for (Publisher *pub : publishers) {
		pub->publish(state);
	}

	return true;
}

void GameController::set_command(SSL_Referee::Command command, bool no_signal) {
	SSL_Referee *ref = state.mutable_referee();

	// Record what’s happening.
	logger.write(Glib::ustring::compose(u8"Setting command %1", SSL_Referee::Command_descriptor()->FindValueByNumber(command)->name()));

	// Set the new command.
	ref->set_command(command);

	// Increment the command counter.
	ref->set_command_counter(ref->command_counter() + 1);

	// Record the command timestamp.
	std::chrono::microseconds diff = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - std::chrono::system_clock::from_time_t(0));
	ref->set_command_timestamp(diff.count());

	// We should save the game state now.
	save_game(state, configuration.save_filename);

	// Emit a signal if requested.
	if (!no_signal) {
		signal_other_changed.emit();
	}
}

void GameController::advance_from_pre() {
	switch (state.referee().stage()) {
		case SSL_Referee::NORMAL_FIRST_HALF_PRE:  enter_stage(SSL_Referee::NORMAL_FIRST_HALF); break;
		case SSL_Referee::NORMAL_SECOND_HALF_PRE: enter_stage(SSL_Referee::NORMAL_SECOND_HALF); break;
		case SSL_Referee::EXTRA_FIRST_HALF_PRE:   enter_stage(SSL_Referee::EXTRA_FIRST_HALF); break;
		case SSL_Referee::EXTRA_SECOND_HALF_PRE:  enter_stage(SSL_Referee::EXTRA_SECOND_HALF); break;
		default: break;
	}
}

