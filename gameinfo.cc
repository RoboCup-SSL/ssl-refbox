#include "gameinfo.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <iterator>
#include <sstream>

Glib::ustring format_time_seconds(std::chrono::high_resolution_clock::duration t) {
	long seconds = std::chrono::duration_cast<std::chrono::duration<long, std::ratio<1>>>(t).count();
	if (seconds <= 0) {
		return u8"0:00";
	} else {
		return Glib::ustring::compose(u8"%1:%2", seconds / 60, Glib::ustring::format(std::setw(2), std::setfill(L'0'), seconds % 60));
	}
}


Glib::ustring format_time_deciseconds(std::chrono::high_resolution_clock::duration t) {
	long seconds = std::chrono::duration_cast<std::chrono::duration<long, std::ratio<1, 10>>>(t).count();
	if (seconds <= 0) {
		return u8"0:00.0";
	} else {
		return Glib::ustring::compose(u8"%1:%2", seconds / 600, Glib::ustring::format(std::setw(4), std::setfill(L'0'), std::fixed, std::setprecision(1), static_cast<unsigned int>(seconds % 600) / 10.0));
	}
}


const char* str_Team[NUM_TEAMS] = {
	"Blue", "Yellow"
};


GameInfo::Data::Data() :
	restart(NEUTRAL),
	state(HALTED),
	stage(PREGAME),
	laststate(HALTED),
	gametime(std::chrono::high_resolution_clock::duration::zero()),
	time_taken(std::chrono::high_resolution_clock::duration::zero()),
	timeoutteam(0),
	timeoutstarttime(std::chrono::high_resolution_clock::duration::zero()),
	yellowcard_time(std::chrono::high_resolution_clock::duration::zero()),
	restarts(0)
{
	std::fill_n(timelimits, NR_GAME_STAGES, std::chrono::high_resolution_clock::duration::zero());
	std::fill_n(timeouts, NUM_TEAMS, std::chrono::high_resolution_clock::duration::zero());
	std::fill_n(nrtimeouts, NUM_TEAMS, 0);
	std::fill_n(goals, NUM_TEAMS, 0);
	std::fill_n(penaltygoals, NUM_TEAMS, 0);
	std::fill_n(yellowcards, NUM_TEAMS, 0);
	std::fill_n(redcards, NUM_TEAMS, 0);
	std::fill_n(penalties, NUM_TEAMS, 0);
	std::fill_n(freekicks, NUM_TEAMS, 0);
}


void GameInfo::Data::save(std::ostream &ofs) const {
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) ofs << teamnames[i].raw() << '\n';
	ofs << restart << ' ';
	ofs << state << ' ';
	ofs << stage << ' ';
	ofs << laststate << ' ';

	ofs << gametime.count() << ' ';

	ofs << time_taken.count() << ' ';

	for (unsigned int i = 0; i < NR_GAME_STAGES; ++i) ofs << timelimits[i].count() << ' ';

	for (unsigned int i = 0; i < NUM_TEAMS; ++i) ofs << timeouts[i].count() << ' ';
	std::copy_n(nrtimeouts, NUM_TEAMS, std::ostream_iterator<int>(ofs, " "));
	ofs << timeoutteam << ' ';
	ofs << timeoutstarttime.count() << ' ';

	std::copy_n(goals, NUM_TEAMS, std::ostream_iterator<int>(ofs, " "));
	std::copy_n(penaltygoals, NUM_TEAMS, std::ostream_iterator<int>(ofs, " "));
	std::copy_n(yellowcards, NUM_TEAMS, std::ostream_iterator<int>(ofs, " "));
	ofs << yellowcard_time.count() << ' ';
	std::copy_n(redcards, NUM_TEAMS, std::ostream_iterator<int>(ofs, " "));
	std::copy_n(freekicks, NUM_TEAMS, std::ostream_iterator<int>(ofs, " "));
	ofs << restarts << '\n';

	for (unsigned int i = 0; i < NUM_TEAMS; ++i) {
		for (auto j = timepenalty[i].begin(), jend = timepenalty[i].end(); j != jend; ++j) {
			ofs << j->count() << ' ';
		}
		ofs << '\n';
	}
}


void GameInfo::Data::load(std::istream &ifs) {
	std::string line;
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) {
		std::getline(ifs, line);
		teamnames[i] = line;
	}

	std::getline(ifs, line);
	std::istringstream iss(line);
	int tmp;
	iss >> tmp;
	restart = static_cast<GameRestart>(tmp);
	iss >> tmp;
	state = static_cast<GameState>(tmp);
	iss >> tmp;
	stage = static_cast<GameStage>(tmp);
	iss >> tmp;
	laststate = static_cast<GameState>(tmp);

	std::chrono::high_resolution_clock::duration::rep tmpt;
	iss >> tmpt;
	gametime = std::chrono::high_resolution_clock::duration(tmpt);

	iss >> tmpt;
	time_taken = std::chrono::high_resolution_clock::duration(tmpt);

	for (unsigned int i = 0; i < NR_GAME_STAGES; ++i) {
		iss >> tmpt;
		timelimits[i] = std::chrono::high_resolution_clock::duration(tmpt);
	}

	for (unsigned int i = 0; i < NUM_TEAMS; ++i) {
		iss >> tmpt;
		timeouts[i] = std::chrono::high_resolution_clock::duration(tmpt);
	}
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) iss >> nrtimeouts[i];
	iss >> timeoutteam;
	iss >> tmpt;
	timeoutstarttime = std::chrono::high_resolution_clock::duration(tmpt);

	for (unsigned int i = 0; i < NUM_TEAMS; ++i) iss >> goals[i];
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) iss >> penaltygoals[i];
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) iss >> yellowcards[i];
	iss >> tmpt;
	yellowcard_time = std::chrono::high_resolution_clock::duration(tmpt);
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) iss >> redcards[i];
	for (unsigned int i = 0; i < NUM_TEAMS; ++i) iss >> freekicks[i];
	iss >> restarts;

	for (unsigned int i = 0; i < NUM_TEAMS; ++i) {
		std::getline(ifs, line);
		iss.str(line);
		timepenalty[i].clear();
		while (iss >> tmpt) {
			timepenalty[i].push_back(std::chrono::high_resolution_clock::duration(tmpt));
		}
	}
}


GameInfo::GameInfo(const std::string& logfname) {
	logfile.exceptions(std::ios_base::eofbit | std::ios_base::failbit | std::ios_base::badbit);
	logfile.open(logfname.c_str());
	std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	logfile << "0.0\tGame log for " << std::ctime(&t);
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
	typedef std::chrono::duration<double, std::ratio<1>> float_seconds;

	logfile << Glib::ustring::compose(u8"%1\t%2\t%3\t%4\tSending %5: %6\n", std::chrono::duration_cast<float_seconds>(data.gametime).count(), getStageString(), getStateString(), std::chrono::duration_cast<float_seconds>(data.time_taken).count(), cmd, msg);
}

