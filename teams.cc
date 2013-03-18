#include "teams.h"
#include <stdexcept>

const TeamMeta TeamMeta::ALL[2];

SaveState::Team TeamMeta::command_team(SSL_Referee::Command command) {
	switch (command) {
		case SSL_Referee::PREPARE_KICKOFF_YELLOW: return SaveState::TEAM_YELLOW;
		case SSL_Referee::PREPARE_KICKOFF_BLUE: return SaveState::TEAM_BLUE;
		case SSL_Referee::PREPARE_PENALTY_YELLOW: return SaveState::TEAM_YELLOW;
		case SSL_Referee::PREPARE_PENALTY_BLUE: return SaveState::TEAM_BLUE;
		case SSL_Referee::DIRECT_FREE_YELLOW: return SaveState::TEAM_YELLOW;
		case SSL_Referee::DIRECT_FREE_BLUE: return SaveState::TEAM_BLUE;
		case SSL_Referee::INDIRECT_FREE_YELLOW: return SaveState::TEAM_YELLOW;
		case SSL_Referee::INDIRECT_FREE_BLUE: return SaveState::TEAM_BLUE;
		case SSL_Referee::TIMEOUT_YELLOW: return SaveState::TEAM_YELLOW;
		case SSL_Referee::TIMEOUT_BLUE: return SaveState::TEAM_BLUE;
		case SSL_Referee::GOAL_YELLOW: return SaveState::TEAM_YELLOW;
		case SSL_Referee::GOAL_BLUE: return SaveState::TEAM_BLUE;
		default: throw std::logic_error("Command is not team-specific!");
	}
}

TeamMeta::TeamMeta() :
		GOAL_COMMAND(team() == SaveState::TEAM_YELLOW ? SSL_Referee::GOAL_YELLOW : SSL_Referee::GOAL_BLUE),
		TIMEOUT_COMMAND(team() == SaveState::TEAM_YELLOW ? SSL_Referee::TIMEOUT_YELLOW : SSL_Referee::TIMEOUT_BLUE),
		PREPARE_KICKOFF_COMMAND(team() == SaveState::TEAM_YELLOW ? SSL_Referee::PREPARE_KICKOFF_YELLOW : SSL_Referee::PREPARE_KICKOFF_BLUE),
		DIRECT_FREE_COMMAND(team() == SaveState::TEAM_YELLOW ? SSL_Referee::DIRECT_FREE_YELLOW : SSL_Referee::DIRECT_FREE_BLUE),
		INDIRECT_FREE_COMMAND(team() == SaveState::TEAM_YELLOW ? SSL_Referee::INDIRECT_FREE_YELLOW : SSL_Referee::INDIRECT_FREE_BLUE),
		PREPARE_PENALTY_COMMAND(team() == SaveState::TEAM_YELLOW ? SSL_Referee::PREPARE_PENALTY_YELLOW : SSL_Referee::PREPARE_PENALTY_BLUE),
		COLOUR(team() == SaveState::TEAM_YELLOW ? u8"yellow" : u8"blue") {
}

SaveState::Team TeamMeta::team() const {
	return static_cast<SaveState::Team>(this - ALL);
}

SaveState::Team TeamMeta::other() const {
	return static_cast<SaveState::Team>((team() + 1U) % 2U);
}

SSL_Referee::TeamInfo &TeamMeta::team_info(SSL_Referee &ref) const {
	return *(team() == SaveState::TEAM_YELLOW ? ref.mutable_yellow() : ref.mutable_blue());
}

const SSL_Referee::TeamInfo &TeamMeta::team_info(const SSL_Referee &ref) const {
	return team() == SaveState::TEAM_YELLOW ? ref.yellow() : ref.blue();
}

uint32_t TeamMeta::penalty_goals(const SaveState &ss) const {
	return team() == SaveState::TEAM_YELLOW ? ss.yellow_penalty_goals() : ss.blue_penalty_goals();
}

void TeamMeta::set_penalty_goals(SaveState &ss, uint32_t penalty_goals) const {
	if (team() == SaveState::TEAM_YELLOW) {
		ss.set_yellow_penalty_goals(penalty_goals);
	} else {
		ss.set_blue_penalty_goals(penalty_goals);
	}
}

