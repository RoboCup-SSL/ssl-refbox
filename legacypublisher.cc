#include "legacypublisher.h"
#include "configuration.h"
#include "savestate.pb.h"
#include <cstdint>
#include <stdexcept>

namespace {
	char map_stage(Referee::Stage stage) {
		switch (stage) {
			case Referee::NORMAL_FIRST_HALF_PRE: return '1';
			case Referee::NORMAL_FIRST_HALF: return '1';
			case Referee::NORMAL_HALF_TIME: return 'h';
			case Referee::NORMAL_SECOND_HALF_PRE: return '2';
			case Referee::NORMAL_SECOND_HALF: return '2';
			case Referee::EXTRA_TIME_BREAK: return 'h';
			case Referee::EXTRA_FIRST_HALF_PRE: return 'o';
			case Referee::EXTRA_FIRST_HALF: return 'o';
			case Referee::EXTRA_HALF_TIME: return 'h';
			case Referee::EXTRA_SECOND_HALF_PRE: return 'O';
			case Referee::EXTRA_SECOND_HALF: return 'O';
			case Referee::PENALTY_SHOOTOUT_BREAK: return 'h';
			case Referee::PENALTY_SHOOTOUT: return 'a';
			case Referee::POST_GAME: return 'H';
		}

		throw std::logic_error("Impossible state!");
	}

	char map_command(Referee::Command command) {
		switch (command) {
			case Referee::HALT: return 'H';
			case Referee::STOP: return 'S';
			case Referee::NORMAL_START: return ' ';
			case Referee::FORCE_START: return 's';
			case Referee::PREPARE_KICKOFF_YELLOW: return 'k';
			case Referee::PREPARE_KICKOFF_BLUE: return 'K';
			case Referee::PREPARE_PENALTY_YELLOW: return 'p';
			case Referee::PREPARE_PENALTY_BLUE: return 'P';
			case Referee::DIRECT_FREE_YELLOW: return 'f';
			case Referee::DIRECT_FREE_BLUE: return 'F';
			case Referee::INDIRECT_FREE_YELLOW: return 'i';
			case Referee::INDIRECT_FREE_BLUE: return 'I';
			case Referee::TIMEOUT_YELLOW: return 't';
			case Referee::TIMEOUT_BLUE: return 'T';
			case Referee::GOAL_YELLOW: return 'g';
			case Referee::GOAL_BLUE: return 'G';
		}

		throw std::logic_error("Impossible state!");
	}
}

LegacyPublisher::LegacyPublisher(const Configuration &configuration, Logger &logger) : bcast(logger, configuration.address, configuration.legacy_port, configuration.interface), counter(0), cached_command_char('H'), last_stage(Referee::NORMAL_FIRST_HALF_PRE), last_command(Referee::HALT), last_yellow_ycards(0), last_blue_ycards(0), last_yellow_rcards(0), last_blue_rcards(0) {
}

void LegacyPublisher::publish(SaveState &state) {
	// Encode the packet.
	uint8_t packet[6];
	packet[0] = compute_command(state.referee());
	packet[1] = counter;
	packet[2] = static_cast<uint8_t>(state.referee().blue().score());
	packet[3] = static_cast<uint8_t>(state.referee().yellow().score());
	if (state.referee().has_stage_time_left() && state.referee().stage_time_left() >= 0) {
		unsigned int left = state.referee().stage_time_left();
		if (left <= 65535) {
			packet[4] = static_cast<uint8_t>(left / 256);
			packet[5] = static_cast<uint8_t>(left);
		} else {
			packet[4] = 0xFF;
			packet[5] = 0xFF;
		}
	} else {
		packet[4] = 0;
		packet[5] = 0;
	}

	// Send the packet.
	bcast.send(packet, sizeof(packet));
}

char LegacyPublisher::compute_command(const Referee &state) {
	enum class Disposition {
		STAGE,
		COMMAND,
		YELLOW_YCARD,
		BLUE_YCARD,
		YELLOW_RCARD,
		BLUE_RCARD,
		CACHE,
	} disposition;

	if (state.stage() != last_stage) {
		// We have just changed from one stage to another.
		// We should announce the new stage.
		disposition = Disposition::STAGE;
	} else if (state.command() != last_command) {
		// We have just changed from one command to another.
		// We should announce the new command.
		disposition = Disposition::COMMAND;
	} else if (state.yellow().yellow_card_times_size() > last_yellow_ycards) {
		// Yellow has more yellow cards than before.
		// Announce a yellow card issued.
		disposition = Disposition::YELLOW_YCARD;
	} else if (state.blue().yellow_card_times_size() > last_blue_ycards) {
		// Blue has more yellow cards than before.
		// Announce a yellow card issued.
		disposition = Disposition::BLUE_YCARD;
	} else if (state.yellow().red_cards() > last_yellow_rcards) {
		// Yellow has more red cards than before.
		// Announce a red card issued.
		disposition = Disposition::YELLOW_RCARD;
	} else if (state.blue().red_cards() > last_blue_rcards) {
		// Blue has more red cards than before.
		// Announce a red card issued.
		disposition = Disposition::BLUE_RCARD;
	} else {
		// If nothing has changed, just return the cached value from last time.
		disposition = Disposition::CACHE;
	}

	// Update all saved state to match the current state.
	last_stage = state.stage();
	last_command = state.command();
	last_yellow_ycards = state.yellow().yellow_card_times_size();
	last_blue_ycards = state.blue().yellow_card_times_size();
	last_yellow_rcards = state.yellow().red_cards();
	last_blue_rcards = state.blue().red_cards();

	// Choose a command to announce based on the decision made above.
	switch (disposition) {
		case Disposition::STAGE: return cached_command_char = map_stage(state.stage());
		case Disposition::COMMAND: return cached_command_char = map_command(state.command());
		case Disposition::YELLOW_YCARD: return cached_command_char = 'y';
		case Disposition::BLUE_YCARD: return cached_command_char = 'Y';
		case Disposition::YELLOW_RCARD: return cached_command_char = 'r';
		case Disposition::BLUE_RCARD: return cached_command_char = 'R';
		case Disposition::CACHE: return cached_command_char;
	}

	throw std::logic_error("Impossible state!");
}

