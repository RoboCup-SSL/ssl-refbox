#include "gameinfo.h"
#include <algorithm>
#include <ctime>
#include <iterator>

const char* str_Team[NUM_TEAMS] = {
	"Blue", "Yellow"
};


GameInfo::Data::Data() {
	restart = NEUTRAL;
	state = HALTED;
	stage = PREGAME;
	laststate = HALTED;

	std::time(&gamestart);
	gametime = 0;

	time_taken = 0;

	std::fill_n(timelimits, NR_GAME_STAGES, 0);

	std::fill_n(timeouts, NUM_TEAMS, 0);
	std::fill_n(nrtimeouts, NUM_TEAMS, 0);
	timeoutteam = 0;
	timeoutstarttime = 0;

	std::fill_n(goals, NUM_TEAMS, 0);
	std::fill_n(penaltygoals, NUM_TEAMS, 0);
	std::fill_n(yellowcards, NUM_TEAMS, 0);
	std::fill_n(timepenalty, NUM_TEAMS, 0);
	yellowcard_time = 0;
	std::fill_n(redcards, NUM_TEAMS, 0);
	std::fill_n(penalties, NUM_TEAMS, 0);
	std::fill_n(freekicks, NUM_TEAMS, 0);
	restarts = 0;
}


void GameInfo::Data::save(std::ostream &ofs) const {
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) ofs << teamnames[i].raw() << '\n';
	ofs << restart << ' ';
	ofs << state << ' ';
	ofs << stage << ' ';
	ofs << laststate << ' ';

	ofs << gamestart << ' ';
	ofs << gametime << ' ';

	ofs << time_taken << ' ';

	std::copy_n(timelimits, NR_GAME_STAGES, std::ostream_iterator<double>(ofs, " "));

	std::copy_n(timeouts, NUM_TEAMS, std::ostream_iterator<double>(ofs, " "));
	std::copy_n(nrtimeouts, NUM_TEAMS, std::ostream_iterator<int>(ofs, " "));
	ofs << timeoutteam << ' ';
	ofs << timeoutstarttime << ' ';

	std::copy_n(goals, NUM_TEAMS, std::ostream_iterator<int>(ofs, " "));
	std::copy_n(penaltygoals, NUM_TEAMS, std::ostream_iterator<int>(ofs, " "));
	std::copy_n(yellowcards, NUM_TEAMS, std::ostream_iterator<int>(ofs, " "));
	std::copy_n(timepenalty, NUM_TEAMS, std::ostream_iterator<double>(ofs, " "));
	ofs << yellowcard_time << ' ';
	std::copy_n(redcards, NUM_TEAMS, std::ostream_iterator<int>(ofs, " "));
	std::copy_n(freekicks, NUM_TEAMS, std::ostream_iterator<int>(ofs, " "));
	ofs << restarts << ' ';
}


void GameInfo::Data::load(std::istream &ifs) {
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) {
		std::string line;
		std::getline(ifs, line);
		teamnames[i] = line;
	}
	int tmp;
	ifs >> tmp;
	restart = static_cast<GameRestart>(tmp);
	ifs >> tmp;
	state = static_cast<GameState>(tmp);
	ifs >> tmp;
	stage = static_cast<GameStage>(tmp);
	ifs >> tmp;
	laststate = static_cast<GameState>(tmp);

	ifs >> gamestart;
	ifs >> gametime;

	ifs >> time_taken;

	for (unsigned int i = 0; i < NR_GAME_STAGES; ++i) ifs >> timelimits[i];

	for (unsigned int i = 0; i < NUM_TEAMS; ++i) ifs >> timeouts[i];
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) ifs >> nrtimeouts[i];
	ifs >> timeoutteam;
	ifs >> timeoutstarttime;

	for (unsigned int i = 0; i < NUM_TEAMS; ++i) ifs >> goals[i];
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) ifs >> penaltygoals[i];
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) ifs >> yellowcards[i];
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) ifs >> timepenalty[i];
	ifs >> yellowcard_time;
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) ifs >> redcards[i];
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) ifs >> freekicks[i];
	ifs >> restarts;
}


GameInfo::GameInfo(const std::string& logfname) {
	logfile.exceptions(std::ios_base::eofbit | std::ios_base::failbit | std::ios_base::badbit);
	logfile.open(logfname.c_str());
	logfile << "0.0\tGame log for " << std::ctime(&data.gamestart);
}


void GameInfo::save(const std::string& fname) const {
	std::ofstream ofs;
	ofs.exceptions(std::ios_base::eofbit | std::ios_base::failbit | std::ios_base::badbit);
	ofs.open(fname.c_str());
	data.save(ofs);
}


void GameInfo::load(const std::string& fname) {
	std::ifstream ifs;
	ifs.exceptions(std::ios_base::eofbit | std::ios_base::failbit | std::ios_base::badbit);
	ifs.open(fname.c_str());
	data.load(ifs);
}


void GameInfo::logCommand(char cmd, const Glib::ustring& msg) {
	logfile << Glib::ustring::compose(u8"%1\t%2\t%3\t%4\tSending %5: %6\n", data.gametime, getStageString(), getStateString(), data.time_taken, cmd, msg);
}

