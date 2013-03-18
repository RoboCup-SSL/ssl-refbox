#ifndef TEAMS_H
#define TEAMS_H

#include "referee.pb.h"
#include "savestate.pb.h"
#include <glibmm/ustring.h>

class TeamMeta {
	public:
		static const TeamMeta ALL[2];

		static SaveState::Team command_team(SSL_Referee::Command command);

		const SSL_Referee::Command GOAL_COMMAND, TIMEOUT_COMMAND, PREPARE_KICKOFF_COMMAND, DIRECT_FREE_COMMAND, INDIRECT_FREE_COMMAND, PREPARE_PENALTY_COMMAND;
		const Glib::ustring COLOUR;

		TeamMeta();

		SaveState::Team team() const;
		SaveState::Team other() const;

		SSL_Referee::TeamInfo &team_info(SSL_Referee &ref) const;
		const SSL_Referee::TeamInfo &team_info(const SSL_Referee &ref) const;

		uint32_t penalty_goals(const SaveState &ss) const;
		void set_penalty_goals(SaveState &ss, uint32_t penalty_goals) const;
};

#endif

