#include "legacypublisher.h"
#include "configuration.h"
#include "savestate.pb.h"
#include <cstdint>
#include <stdexcept>

namespace {
	char map_stage(SSL_Referee::Stage stage) {
		switch (stage) {
			case SSL_Referee::NORMAL_FIRST_HALF_PRE: return '1';
			case SSL_Referee::NORMAL_FIRST_HALF: return ' ';
			case SSL_Referee::NORMAL_HALF_TIME: return 'h';
			case SSL_Referee::NORMAL_SECOND_HALF_PRE: return '2';
			case SSL_Referee::NORMAL_SECOND_HALF: return ' ';
			case SSL_Referee::EXTRA_TIME_BREAK: return 'h';
			case SSL_Referee::EXTRA_FIRST_HALF_PRE: return 'o';
			case SSL_Referee::EXTRA_FIRST_HALF: return ' ';
			case SSL_Referee::EXTRA_HALF_TIME: return 'h';
			case SSL_Referee::EXTRA_SECOND_HALF_PRE: return 'O';
			case SSL_Referee::EXTRA_SECOND_HALF: return ' ';
			case SSL_Referee::PENALTY_SHOOTOUT_BREAK: return 'h';
			case SSL_Referee::PENALTY_SHOOTOUT: return 'a';
			case SSL_Referee::POST_GAME: return 'H';
		}

		throw std::logic_error("Impossible state!");
	}

	char map_command(SSL_Referee::Command command) {
		switch (command) {
			case SSL_Referee::HALT: return 'H';
			case SSL_Referee::STOP: return 'S';
			case SSL_Referee::NORMAL_START: return ' ';
			case SSL_Referee::FORCE_START: return 's';
			case SSL_Referee::PREPARE_KICKOFF_YELLOW: return 'k';
			case SSL_Referee::PREPARE_KICKOFF_BLUE: return 'K';
			case SSL_Referee::PREPARE_PENALTY_YELLOW: return 'p';
			case SSL_Referee::PREPARE_PENALTY_BLUE: return 'P';
			case SSL_Referee::DIRECT_FREE_YELLOW: return 'f';
			case SSL_Referee::DIRECT_FREE_BLUE: return 'F';
			case SSL_Referee::INDIRECT_FREE_YELLOW: return 'i';
			case SSL_Referee::INDIRECT_FREE_BLUE: return 'I';
			case SSL_Referee::TIMEOUT_YELLOW: return 't';
			case SSL_Referee::TIMEOUT_BLUE: return 'T';
			case SSL_Referee::GOAL_YELLOW: return 'g';
			case SSL_Referee::GOAL_BLUE: return 'G';
		}

		throw std::logic_error("Impossible state!");
	}
}

LegacyPublisher::LegacyPublisher(const Configuration &configuration, Logger &logger) : bcast(logger, configuration.address, configuration.legacy_port, configuration.interface), cached_command_char('H'), last_stage(SSL_Referee::NORMAL_FIRST_HALF_PRE), last_command(SSL_Referee::HALT), last_yellow_ycards(0), last_blue_ycards(0), last_yellow_rcards(0), last_blue_rcards(0) {
}

void LegacyPublisher::publish(SaveState &state) {
	// Encode the packet.
	uint8_t packet[6];
	packet[0] = compute_command(state.referee());
	packet[1] = static_cast<uint8_t>(state.referee().command_counter());
	packet[2] = static_cast<uint8_t>(state.referee().blue().score());
	packet[3] = static_cast<uint8_t>(state.referee().yellow().score());
	if (state.referee().has_stage_time_left() && state.referee().stage_time_left() >= 0) {
		unsigned int left = state.referee().stage_time_left() / 1000000;
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

char LegacyPublisher::compute_command(const SSL_Referee &state) {
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

