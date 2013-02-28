#ifndef TEAMS_H
#define TEAMS_H

#include "referee.pb.h"
#include "savestate.pb.h"
#include <glibmm/ustring.h>

class TeamMeta {
	public:
		static const TeamMeta ALL[2];

		static SaveState::Team command_team(Referee::Command command);

		const Referee::Command GOAL_COMMAND, TIMEOUT_COMMAND, PREPARE_KICKOFF_COMMAND, DIRECT_FREE_COMMAND, INDIRECT_FREE_COMMAND, PREPARE_PENALTY_COMMAND;
		const Glib::ustring COLOUR;

		TeamMeta();

		SaveState::Team team() const;
		SaveState::Team other() const;

		Referee::TeamInfo &team_info(Referee &ref) const;
		const Referee::TeamInfo &team_info(const Referee &ref) const;

		uint32_t penalty_goals(const SaveState &ss) const;
		void set_penalty_goals(SaveState &ss, uint32_t penalty_goals) const;
};

#endif

