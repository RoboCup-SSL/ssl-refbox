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

GameController::GameController(Logger &logger, const Configuration &configuration, const std::vector<Publisher *> &publishers, const std::string &resume_filename) :
		configuration(configuration),
		logger(logger),
		publishers(publishers),
		tick_connection(Glib::signal_timeout().connect(sigc::mem_fun(this, &GameController::tick), 25)),
		microseconds_since_last_state_save(0) {
	if (!resume_filename.empty()) {
		std::ifstream ifs;
		ifs.exceptions(std::ios_base::badbit);
		ifs.open(resume_filename, std::ios_base::in | std::ios_base::binary);
		if (!state.ParseFromIstream(&ifs)) {
			throw std::runtime_error(Glib::locale_from_utf8(Glib::ustring::compose(u8"Protobuf error loading saved game state from file \"%1\"!", Glib::filename_to_utf8(resume_filename))));
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
		ref.set_blueteamonpositivehalf(false);

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

bool GameController::can_enter_stage(SSL_Referee::Stage stage) const {
	const SSL_Referee &ref = state.referee();
	bool is_stopped = ref.command() == SSL_Referee::STOP || ref.command() == SSL_Referee::GOAL_YELLOW || ref.command() == SSL_Referee::GOAL_BLUE;
	bool is_normal_half = ref.stage() == SSL_Referee::NORMAL_FIRST_HALF || ref.stage() == SSL_Referee::NORMAL_SECOND_HALF || ref.stage() == SSL_Referee::EXTRA_FIRST_HALF || ref.stage() == SSL_Referee::EXTRA_SECOND_HALF;

	switch (stage) {
		case SSL_Referee::NORMAL_FIRST_HALF_PRE:
		case SSL_Referee::NORMAL_FIRST_HALF:
		case SSL_Referee::NORMAL_SECOND_HALF:
		case SSL_Referee::EXTRA_FIRST_HALF:
		case SSL_Referee::EXTRA_SECOND_HALF:
			// You can only get to first half pre-game or any running game half when you are ignoring rules; otherwise, you start in first half pre-game and enter other halves via the Normal Start command.
			return false;

		case SSL_Referee::NORMAL_SECOND_HALF_PRE:
			// You can get to second half when you are in normal half time.
			return ref.stage() == SSL_Referee::NORMAL_HALF_TIME;

		case SSL_Referee::EXTRA_FIRST_HALF_PRE:
			// You can get to overtime 1 when you are in extra time break.
			return ref.stage() == SSL_Referee::EXTRA_TIME_BREAK;

		case SSL_Referee::EXTRA_SECOND_HALF_PRE:
			// You can get to overtime 2 when you are in extra time half time.
			return ref.stage() == SSL_Referee::EXTRA_HALF_TIME;

		case SSL_Referee::PENALTY_SHOOTOUT:
			// You can get to penalty shootout when you are in penalty shootout break.
			return ref.stage() == SSL_Referee::PENALTY_SHOOTOUT_BREAK;

		case SSL_Referee::POST_GAME:
			// You can get to post-game whenever you are stopped or halted except in post-game.
			// This might be needed at any time throughout the game, due to the stop-at-ten-points rule.
			return ref.stage() != SSL_Referee::POST_GAME && (is_stopped || ref.command() == SSL_Referee::HALT);

		case SSL_Referee::NORMAL_HALF_TIME:
		case SSL_Referee::EXTRA_TIME_BREAK:
		case SSL_Referee::EXTRA_HALF_TIME:
		case SSL_Referee::PENALTY_SHOOTOUT_BREAK:
			// You can get to half time whenever you are stopped in the proper normal half.
			return is_normal_half && is_stopped && stage == next_half_time();
	}

	return false;
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
		// set_command will save the game state and emit a change signal.
		set_command(SSL_Referee::HALT);
	}
	// In case of starting a game half, the function that called
	// advance_from_pre will also call set_command.
}

SSL_Referee::Stage GameController::next_half_time() const {
	const SSL_Referee &ref = state.referee();

	// Which stage to go into depends on which stage we are already in.
	switch (ref.stage()) {
		case SSL_Referee::NORMAL_FIRST_HALF_PRE:
		case SSL_Referee::NORMAL_FIRST_HALF:
			return SSL_Referee::NORMAL_HALF_TIME;
		case SSL_Referee::NORMAL_HALF_TIME:
		case SSL_Referee::NORMAL_SECOND_HALF_PRE:
		case SSL_Referee::NORMAL_SECOND_HALF:
			return SSL_Referee::EXTRA_TIME_BREAK;
		case SSL_Referee::EXTRA_TIME_BREAK:
		case SSL_Referee::EXTRA_FIRST_HALF_PRE:
		case SSL_Referee::EXTRA_FIRST_HALF:
			return SSL_Referee::EXTRA_HALF_TIME;
		case SSL_Referee::EXTRA_HALF_TIME:
		case SSL_Referee::EXTRA_SECOND_HALF_PRE:
		case SSL_Referee::EXTRA_SECOND_HALF:
		case SSL_Referee::PENALTY_SHOOTOUT_BREAK:
		case SSL_Referee::PENALTY_SHOOTOUT:
			return SSL_Referee::PENALTY_SHOOTOUT_BREAK;
		case SSL_Referee::POST_GAME:
			return SSL_Referee::POST_GAME;
	}

	return SSL_Referee::POST_GAME;
}

bool GameController::can_set_command(SSL_Referee::Command command) const {
	const SSL_Referee &ref = state.referee();
	bool is_normal_half = ref.stage() == SSL_Referee::NORMAL_FIRST_HALF || ref.stage() == SSL_Referee::NORMAL_SECOND_HALF || ref.stage() == SSL_Referee::EXTRA_FIRST_HALF || ref.stage() == SSL_Referee::EXTRA_SECOND_HALF;
	bool is_break = ref.stage() == SSL_Referee::NORMAL_HALF_TIME || ref.stage() == SSL_Referee::EXTRA_TIME_BREAK || ref.stage() == SSL_Referee::EXTRA_HALF_TIME || ref.stage() == SSL_Referee::PENALTY_SHOOTOUT_BREAK;
	bool is_stopped = ref.command() == SSL_Referee::STOP || ref.command() == SSL_Referee::GOAL_YELLOW || ref.command() == SSL_Referee::GOAL_BLUE;
	bool is_prepare_kickoff = ref.command() == SSL_Referee::PREPARE_KICKOFF_YELLOW || ref.command() == SSL_Referee::PREPARE_KICKOFF_BLUE;
	bool is_prepare_penalty = ref.command() == SSL_Referee::PREPARE_PENALTY_YELLOW || ref.command() == SSL_Referee::PREPARE_PENALTY_BLUE;
	bool is_pshootout = ref.stage() == SSL_Referee::PENALTY_SHOOTOUT;
	bool is_ball_placement = ref.command() == SSL_Referee::BALL_PLACEMENT_YELLOW || ref.command() == SSL_Referee::BALL_PLACEMENT_BLUE;
	bool is_team_name_empty = false;
	for (const TeamMeta &team : TeamMeta::ALL) {
		is_team_name_empty |= team.team_info(ref).name().empty();
	}
	switch (command) {
		case SSL_Referee::HALT:
			// You can HALT any time you are not already halted.
			return ref.command() != SSL_Referee::HALT;

		case SSL_Referee::STOP:
			// You can STOP any time you are not already stopped except in post-game.
			return !is_stopped && ref.stage() != SSL_Referee::POST_GAME;

		case SSL_Referee::FORCE_START:
			// You can FORCE START any time you are stopped in a normal half or a break (for robot testing).
			return (is_normal_half || is_break) && is_stopped;

		case SSL_Referee::NORMAL_START:
			// You can NORMAL START when you are preparing a prepared play (kickoff or penalty kick), except if a required team name is missing.
			return (is_prepare_kickoff || is_prepare_penalty) && !(configuration.team_names_required && is_team_name_empty);

		case SSL_Referee::PREPARE_KICKOFF_YELLOW:
		case SSL_Referee::PREPARE_KICKOFF_BLUE:
			// A team can take a kickoff whenever the game is stopped or in ball placement and not in a break or penalty shootout.
			return !is_break && !is_pshootout && (is_stopped || is_ball_placement);

		case SSL_Referee::DIRECT_FREE_YELLOW:
		case SSL_Referee::DIRECT_FREE_BLUE:
		case SSL_Referee::INDIRECT_FREE_YELLOW:
		case SSL_Referee::INDIRECT_FREE_BLUE:
			// A team can take a free kick whenever the game is in a normal half and stopped or after a successfull autonomous ball placement.
			return is_normal_half && (is_stopped || is_ball_placement);

		case SSL_Referee::PREPARE_PENALTY_YELLOW:
		case SSL_Referee::PREPARE_PENALTY_BLUE:
			// A team can take a penalty kick whenever the game is stopped or in ball placement in a normal half or during penalty shootout.
			return (is_normal_half || is_pshootout) && (is_stopped || is_ball_placement);

		// A team can start a timeout whenever the game is stopped and not in a break or penalty shootout.
		// A team can *resume* a timeout whenever the game is halted and that team already had a timeout in progress before the halt.
		case SSL_Referee::TIMEOUT_YELLOW:
			return (!is_break && !is_pshootout && is_stopped) || (ref.command() == SSL_Referee::HALT && state.has_timeout() && state.timeout().team() == SaveState::TEAM_YELLOW);
		case SSL_Referee::TIMEOUT_BLUE:
			return (!is_break && !is_pshootout && is_stopped) || (ref.command() == SSL_Referee::HALT && state.has_timeout() && state.timeout().team() == SaveState::TEAM_BLUE);

		case SSL_Referee::GOAL_YELLOW:
		case SSL_Referee::GOAL_BLUE:
			// You can award goals whenever you are stopped.
			return is_stopped;

		// You can ask for ball placement whenever you are stopped or the other team failed to place the ball
		case SSL_Referee::BALL_PLACEMENT_YELLOW:
			return is_stopped || ref.command() == SSL_Referee::BALL_PLACEMENT_BLUE;
		case SSL_Referee::BALL_PLACEMENT_BLUE:
			return is_stopped || ref.command() == SSL_Referee::BALL_PLACEMENT_YELLOW;
	}

	return false;
}

bool GameController::command_needs_designated_position(SSL_Referee::Command command) {
	switch (command) {
		case SSL_Referee::BALL_PLACEMENT_YELLOW:
		case SSL_Referee::BALL_PLACEMENT_BLUE:
			return true;

		case SSL_Referee::HALT:
		case SSL_Referee::STOP:
		case SSL_Referee::FORCE_START:
		case SSL_Referee::NORMAL_START:
		case SSL_Referee::PREPARE_KICKOFF_YELLOW:
		case SSL_Referee::PREPARE_KICKOFF_BLUE:
		case SSL_Referee::DIRECT_FREE_YELLOW:
		case SSL_Referee::DIRECT_FREE_BLUE:
		case SSL_Referee::INDIRECT_FREE_YELLOW:
		case SSL_Referee::INDIRECT_FREE_BLUE:
		case SSL_Referee::PREPARE_PENALTY_YELLOW:
		case SSL_Referee::PREPARE_PENALTY_BLUE:
		case SSL_Referee::TIMEOUT_YELLOW:
		case SSL_Referee::TIMEOUT_BLUE:
		case SSL_Referee::GOAL_YELLOW:
		case SSL_Referee::GOAL_BLUE:
			return false;
	}

	return false;
}

void GameController::set_game_event(const SSL_Referee_Game_Event *game_event) {
    SSL_Referee *ref = state.mutable_referee();

    // copy game event from request
    if(game_event != NULL) {
        ref->mutable_gameevent()->CopyFrom(*game_event);
    }
}

void GameController::set_command(SSL_Referee::Command command, float designated_x, float designated_y, bool cancelling_timeout_end) {
	SSL_Referee *ref = state.mutable_referee();

	// Record what’s happening.
	logger.write(Glib::ustring::compose(u8"Setting command %1", SSL_Referee::Command_descriptor()->FindValueByNumber(command)->name()));

	// Clear any prior designated position.
	ref->clear_designated_position();

	// Clear any last timeout information, unless we need it.
	// It is easier to do this here, before a command might set it, than later.
	if (!cancelling_timeout_end) {
		state.clear_last_timeout();
	}

	// Implement any special side effects.
	switch (command) {
		case SSL_Referee::STOP:
			// Remember which team had the timeout, if any, so exiting the timeout can be cancelled.
			if (state.has_timeout()) {
				*state.mutable_last_timeout() = state.timeout();
			}
			state.clear_timeout();
			break;

		case SSL_Referee::FORCE_START:
		case SSL_Referee::NORMAL_START:
			advance_from_pre();
			break;

		case SSL_Referee::TIMEOUT_YELLOW:
		case SSL_Referee::TIMEOUT_BLUE:
			// Only update any of the accounting if there is not already a record of an in-progress timeout.
			// This allows to issue HALT during a timeout, then resume the running timeout, without eating up another of the team’s timeouts and without affecting the Cancel button.
			// If that happens, during HALT there will still be a record of a running timeout.
			{
				SaveState::Team team = (command == SSL_Referee::TIMEOUT_YELLOW) ? SaveState::TEAM_YELLOW : SaveState::TEAM_BLUE;
				SSL_Referee::TeamInfo &ti = TeamMeta::ALL[team].team_info(*ref);
				if (cancelling_timeout_end) {
					// We have been asked to cancel the previously ended timeout.
					// Do this by just moving the timeout information back from last_timeout.
					// We need to keep the old left_before value as well, so that Timeout, Stop, Cancel End, Cancel Timeout refunds the full time, not just the time taken in the second part.
					*state.mutable_timeout() = state.last_timeout();
					state.clear_last_timeout();
				} else if (!(state.has_timeout() && state.timeout().team() == team)) {
					// Do not debit a timeout if the team has no timeouts left, as we would wrap the counter.
					// Assume the referee is granting an extra timeout at their discretion.
					if (ti.timeouts()) {
						ti.set_timeouts(ti.timeouts() - 1);
					}
					state.mutable_timeout()->set_team(team);
					state.mutable_timeout()->set_left_before(ti.timeout_time());
				}
			}
			break;

		case SSL_Referee::GOAL_YELLOW:
		case SSL_Referee::GOAL_BLUE:
			{
				SaveState::Team team = (command == SSL_Referee::GOAL_YELLOW) ? SaveState::TEAM_YELLOW : SaveState::TEAM_BLUE;
				SSL_Referee::TeamInfo &ti = TeamMeta::ALL[team].team_info(*ref);

				// Increase the team’s score.
				ti.set_score(ti.score() + 1);

				// Increase the team’s number of penalty goals if in a penalty shootout.
				if (ref->stage() == SSL_Referee::PENALTY_SHOOTOUT) {
					TeamMeta::ALL[team].set_penalty_goals(state, TeamMeta::ALL[team].penalty_goals(state) + 1);
				}
			}
			break;

		case SSL_Referee::BALL_PLACEMENT_YELLOW:
		case SSL_Referee::BALL_PLACEMENT_BLUE:
			// Communicate the designated position.
			ref->mutable_designated_position()->set_x(designated_x);
			ref->mutable_designated_position()->set_y(designated_y);
			break;

		case SSL_Referee::HALT:
		case SSL_Referee::PREPARE_KICKOFF_YELLOW:
		case SSL_Referee::PREPARE_KICKOFF_BLUE:
		case SSL_Referee::DIRECT_FREE_YELLOW:
		case SSL_Referee::DIRECT_FREE_BLUE:
		case SSL_Referee::INDIRECT_FREE_YELLOW:
		case SSL_Referee::INDIRECT_FREE_BLUE:
		case SSL_Referee::PREPARE_PENALTY_YELLOW:
		case SSL_Referee::PREPARE_PENALTY_BLUE:
			// No side effects.
			break;
	}

	// Set the new command.
	ref->set_command(command);

	// Increment the command counter.
	ref->set_command_counter(ref->command_counter() + 1);

	// Record the command timestamp.
	std::chrono::microseconds diff = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - std::chrono::system_clock::from_time_t(0));
	ref->set_command_timestamp(static_cast<uint64_t>(diff.count()));

	// We should save the game state now.
	save_game(state, configuration.save_filename);

	// Notify listeners of the state change.
	signal_other_changed.emit();
}

void GameController::set_teamname(SaveState::Team team, const Glib::ustring &name) {
	SSL_Referee &ref = *state.mutable_referee();
	TeamMeta::ALL[team].team_info(ref).set_name(name.raw());
	signal_teamname_changed.emit();
}

bool GameController::can_set_goalie() const {
	// You can change goalies whenever the game is stopped or halted except in post-game.
	const SSL_Referee &ref = state.referee();
	bool is_stopped = ref.command() == SSL_Referee::STOP || ref.command() == SSL_Referee::GOAL_YELLOW || ref.command() == SSL_Referee::GOAL_BLUE;
	return ref.stage() != SSL_Referee::POST_GAME && (ref.command() == SSL_Referee::HALT || is_stopped);
}

void GameController::set_goalie(SaveState::Team team, unsigned int goalie) {
	SSL_Referee &ref = *state.mutable_referee();
	TeamMeta::ALL[team].team_info(ref).set_goalie(goalie);
}

bool GameController::can_switch_colours() const {
	// You can switch colours when you are halted in a break or pre-half.
	const SSL_Referee &ref = state.referee();
	bool is_break = ref.stage() == SSL_Referee::NORMAL_HALF_TIME || ref.stage() == SSL_Referee::EXTRA_TIME_BREAK || ref.stage() == SSL_Referee::EXTRA_HALF_TIME || ref.stage() == SSL_Referee::PENALTY_SHOOTOUT_BREAK;
	bool is_pre = ref.stage() == SSL_Referee::NORMAL_FIRST_HALF_PRE || ref.stage() == SSL_Referee::NORMAL_SECOND_HALF_PRE || ref.stage() == SSL_Referee::EXTRA_FIRST_HALF_PRE || ref.stage() == SSL_Referee::EXTRA_SECOND_HALF_PRE;
	return ref.command() == SSL_Referee::HALT && (is_break || is_pre);
}

bool GameController::can_switch_sides() const {
	return can_switch_colours();
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

void GameController::switch_sides(bool blueTeamOnPositiveHalf) {
	logger.write(Glib::ustring::compose(u8"Switching sides: %1 Team on positive half", blueTeamOnPositiveHalf ? "Blue" : "Yellow"));

	SSL_Referee &ref = *state.mutable_referee();

	ref.set_blueteamonpositivehalf(blueTeamOnPositiveHalf);

	signal_other_changed.emit();
}

bool GameController::can_subtract_goal(SaveState::Team team) const {
	// You can subtract goals whenever you are stopped and the team has points.
	const SSL_Referee &ref = state.referee();
	return (ref.command() == SSL_Referee::STOP || ref.command() == SSL_Referee::GOAL_YELLOW || ref.command() == SSL_Referee::GOAL_BLUE) && TeamMeta::ALL[team].team_info(ref).score();
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

GameController::CancelType GameController::cancel_type() const {
	const SSL_Referee &ref = state.referee();
	if (ref.stage() == SSL_Referee::POST_GAME) {
		// You can never cancel anything in post-game.
		return CancelType::NONE;
	} else if (ref.command() == SSL_Referee::TIMEOUT_YELLOW || ref.command() == SSL_Referee::TIMEOUT_BLUE) {
		// You can cancel a timeout only when the timeout is running, not if it is in a nested HALT.
		return CancelType::TIMEOUT;
	} else if (state.has_last_timeout()) {
		// You can cancel ending a timeout when the last team who took a timeout is remembered.
		return CancelType::TIMEOUT_END;
	} else if (state.has_last_card()) {
		// You can cancel a card whenever there is one outstanding.
		return CancelType::CARD;
	} else {
		// There is nothing available to cancel right now.
		return CancelType::NONE;
	}
}

void GameController::cancel() {
	SSL_Referee &ref = *state.mutable_referee();

	switch (cancel_type()) {
		case CancelType::NONE:
			break;

		case CancelType::CARD:
			// A card is active; cancel it.
			{
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
			break;

		case CancelType::TIMEOUT:
			// A timeout is active; cancel it.
			{
				SaveState::Team team = TeamMeta::command_team(ref.command());
				logger.write(Glib::ustring::compose(u8"Cancelling %1 timeout.", TeamMeta::ALL[team].COLOUR));
				SSL_Referee::TeamInfo &ti = TeamMeta::ALL[team].team_info(ref);
				ti.set_timeouts(ti.timeouts() + 1);
				ti.set_timeout_time(state.timeout().left_before());
				set_command(SSL_Referee::STOP);
				// Do not allow cancelling the end of the cancelled timeout, because that way lies madness!
				state.clear_last_timeout();
			}
			break;

		case CancelType::TIMEOUT_END:
			// Resume the timeout that was most recently running without debiting another timeout from the team’s budget.
			{
				set_command((state.last_timeout().team() == SaveState::TEAM_YELLOW) ? SSL_Referee::TIMEOUT_YELLOW : SSL_Referee::TIMEOUT_BLUE, 0.0f, 0.0f, true);
			}
			break;
	}

	signal_other_changed.emit();
}

bool GameController::can_issue_card() const {
	const SSL_Referee &ref = state.referee();
	return ref.command() == SSL_Referee::STOP || ref.command() == SSL_Referee::GOAL_YELLOW || ref.command() == SSL_Referee::GOAL_BLUE;
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
	bool half_time_like = stage == SSL_Referee::NORMAL_HALF_TIME
                      || stage == SSL_Referee::EXTRA_TIME_BREAK
                      || stage == SSL_Referee::EXTRA_HALF_TIME
                      || stage == SSL_Referee::PENALTY_SHOOTOUT_BREAK;

	bool stopped_game = command == SSL_Referee::HALT
                    || command == SSL_Referee::STOP
                    || command == SSL_Referee::BALL_PLACEMENT_BLUE
                    || command == SSL_Referee::BALL_PLACEMENT_YELLOW
                    || command == SSL_Referee::GOAL_BLUE
                    || command == SSL_Referee::GOAL_YELLOW;

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
	} else if (!stopped_game || half_time_like) {
		// Otherwise, as long as we are not in halt OR we are in a half-time-like stage, the stage clock runs, if this particular stage *has* a stage clock.
		// There are two game clocks, the stage time left clock and the stage time taken clock.
		// The stage time left clock may or may not exist; the stage time taken clock always exists.
		// Keep both in lockstep.
		{
			bool emit = false;
			if (ref.has_stage_time_left()) {
				int32_t old_left = ref.stage_time_left();
				int32_t old_tenths = old_left / 100000;
				int32_t new_left = old_left - static_cast<int32_t>(delta);
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

void GameController::advance_from_pre() {
	switch (state.referee().stage()) {
		case SSL_Referee::NORMAL_FIRST_HALF_PRE:  enter_stage(SSL_Referee::NORMAL_FIRST_HALF); break;
		case SSL_Referee::NORMAL_SECOND_HALF_PRE: enter_stage(SSL_Referee::NORMAL_SECOND_HALF); break;
		case SSL_Referee::EXTRA_FIRST_HALF_PRE:   enter_stage(SSL_Referee::EXTRA_FIRST_HALF); break;
		case SSL_Referee::EXTRA_SECOND_HALF_PRE:  enter_stage(SSL_Referee::EXTRA_SECOND_HALF); break;
		default: break;
	}
}
