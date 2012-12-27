/*
 * TITLE: gamecontrol.cc
 *
 * PURPOSE: Wraps the game control functionality for keeping track of time, and other 
 * game info.
 *
 * WRITTEN BY: Brett Browning 
 */
/* LICENSE:  =========================================================================
   RoboCup F180 Referee Box Source Code Release
   -------------------------------------------------------------------------
   Copyright (C) 2003 RoboCup Federation
   -------------------------------------------------------------------------
   This software is distributed under the GNU General Public License,
   version 2.  If you do not have a copy of this licence, visit
   www.gnu.org, or write: Free Software Foundation, 59 Temple Place,
   Suite 330 Boston, MA 02111-1307 USA.  This program is distributed
   in the hope that it will be useful, but WITHOUT ANY WARRANTY,
   including MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   ------------------------------------------------------------------------- 

*/

#include "commands.h"
#include "gamecontrol.h"
#include "gameinfo.h"
#include "logging.h"
#include "settings.h"
#include "udp_broadcast.h"
#include <glibmm.h>
#include <iostream>
#include <iomanip>
#include <utility>

#ifndef WIN32
#include <arpa/inet.h> //for htonl()
#else
#include <winsock.h>
#endif



#define CHOOSETEAM(t, blue, yel) (((t) == Blue) ? (blue) : (yel))


GameControl::GameControl(const std::string& configfile, const std::string& logfname, bool restart) :
	broadcast(log),
	gameinfo(logfname)
{
	lastCommand = 'H'; //HALT
	lastCommandCounter = 0;

	enabled = true;

	// default multicast address
	mc_addr = "224.5.29.1";
	mc_port = 10001;

	log.add(u8"reading config file...");
	readSettings(configfile);
	log.add(u8"config file read.");

	try {
		broadcast.set_destination(mc_addr, mc_port);
	}
	catch (const UDP_Broadcast::IOError& e)
	{
		log.add(Glib::ustring::compose(u8"Ethernet failed: %1", e.what()));
	}

	// a little user output
	std::cout << "Game Settings\n";
	std::cout << "\ttimelimits : First Half " << format_time_seconds(gameinfo.data.timelimits[FIRST_HALF]) << '\n';
	std::cout << "\t\tHalf time " << format_time_seconds(gameinfo.data.timelimits[HALF_TIME]) << '\n';
	std::cout << "\t\tSecond half " << format_time_seconds(gameinfo.data.timelimits[SECOND_HALF]) << '\n';
	std::cout << "\t\tOvertime " << format_time_seconds(gameinfo.data.timelimits[OVER_TIME1]) << '\n';
	std::cout << "\t\tOvertime " << format_time_seconds(gameinfo.data.timelimits[OVER_TIME2]) << '\n';
	std::cout << "\ttimeouts : number " << gameinfo.data.nrtimeouts[0] << ", total time " << format_time_seconds(gameinfo.data.timeouts[0]) << '\n';

	// intialize the timer
	gameinfo.resetTimer();
	tlast = std::chrono::high_resolution_clock::now();

	// restart from saved state
	if (restart) {
		gameinfo.load(savename);
	}
}

/////////////////////////////
// send commands
// log commands, send them over UDP and change game state
// increment command counter
void GameControl::sendCommand(char cmd, const Glib::ustring& msg) 
{
	lastCommand = cmd;
	lastCommandCounter++;

	log.add(Glib::ustring::compose(u8"Sending '%1': %2", cmd, msg));
	gameinfo.logCommand(cmd, msg);

	ethernetSendPacket();
}


/////////////////////////////
// send command to ethernet clients.
void GameControl::ethernetSendPacket()
{
	unsigned int rem = std::chrono::duration_cast<std::chrono::duration<unsigned int, std::ratio<1>>>(gameinfo.timeRemaining()).count();

	uint8_t p[6];
	p[0] = lastCommand;
	p[1] = static_cast<uint8_t>(lastCommandCounter);
	p[2] = static_cast<uint8_t>(gameinfo.data.goals[Blue]);
	p[3] = static_cast<uint8_t>(gameinfo.data.goals[Yellow]);
	p[4] = static_cast<uint8_t>(rem >> 8);
	p[5] = static_cast<uint8_t>(rem);

	try
	{
		broadcast.send(p,sizeof(p));
	}
	catch (UDP_Broadcast::IOError& e)
	{
		log.add(Glib::ustring::compose(u8"!! UDP_Broadcast: %1", e.what()));
	}
}

void GameControl::stepTime() 
{
	std::chrono::high_resolution_clock::time_point tnew = std::chrono::high_resolution_clock::now();
	std::chrono::high_resolution_clock::duration dt = tnew - tlast;
	tlast = tnew;

	// save restore file
	gameinfo.save(savename);

	// update game time
	gameinfo.data.gametime += dt;

	if (gameinfo.isTimeout()) {
		gameinfo.data.timeouts[gameinfo.data.timeoutteam] -= dt;
		if (gameinfo.isTimeoutComplete()) {
			stopTimeout();
		}
	} else {
		if ((gameinfo.data.stage == HALF_TIME) || !gameinfo.isHalted()) {
			gameinfo.data.time_taken += dt;
			for (int x = 0; x < NUM_TEAMS; ++x) {
				if (gameinfo.data.timepenalty[x] > std::chrono::high_resolution_clock::duration::zero()) {
					gameinfo.data.timepenalty[x] -= dt;
				} else {
					gameinfo.data.timepenalty[x] = std::chrono::high_resolution_clock::duration::zero();
				}
			}
		}
	}

	// repeat last command (if someone missed it)
	ethernetSendPacket();
}


namespace {
	std::chrono::high_resolution_clock::duration int_seconds_to_clock(int seconds) {
		return std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::duration<int, std::ratio<1>>(seconds));
	}
}


////////////////////////////////////////
// configuration
// read a config file to fill in parameters
void GameControl::readSettings(const std::string& configfile) 
{
	Settings settings(log);

	settings.readFile(configfile);

	settings.get("SAVENAME", savename);
	settings.get("MULTICASTADDRESS", mc_addr);
	int temp_port;
	settings.get("MULTICASTPORT", temp_port);
	mc_port = static_cast<uint16_t>(temp_port);

	int timeout_seconds;
	settings.get("TIMEOUT_LIMIT", timeout_seconds);
	gameinfo.data.timeouts[Blue] = int_seconds_to_clock(timeout_seconds);
	gameinfo.data.timeouts[Yellow] = int_seconds_to_clock(timeout_seconds);

	int timeout_number;
	settings.get("NR_TIMEOUTS", timeout_number);
	gameinfo.data.nrtimeouts[Blue] = timeout_number;
	gameinfo.data.nrtimeouts[Yellow] = timeout_number;

	int timelimit_firsthalf;
	settings.get("FIRST_HALF", timelimit_firsthalf);
	gameinfo.data.timelimits[FIRST_HALF] = int_seconds_to_clock(timelimit_firsthalf);

	int timelimit_halftime;
	settings.get("HALF_TIME", timelimit_halftime);
	gameinfo.data.timelimits[HALF_TIME] = int_seconds_to_clock(timelimit_halftime);

	int timelimit_secondhalf;
	settings.get("SECOND_HALF", timelimit_secondhalf);
	gameinfo.data.timelimits[SECOND_HALF] = int_seconds_to_clock(timelimit_secondhalf);

	int timelimit_overtime;
	settings.get("OVER_TIME", timelimit_overtime);
	gameinfo.data.timelimits[OVER_TIME1] = int_seconds_to_clock(timelimit_overtime);
	gameinfo.data.timelimits[OVER_TIME2] = int_seconds_to_clock(timelimit_overtime);

	int timelimit_yellowcard;
	settings.get("YELLOWCARD_TIME", timelimit_yellowcard);
	gameinfo.data.yellowcard_time = int_seconds_to_clock(timelimit_yellowcard);
}




/////////////////////////////
// game stage commands
bool GameControl::beginFirstHalf()
{
	if (enabled) {
		if (gameinfo.data.stage != PREGAME) 
			return (false);

		// send the first half signal but we do not officially begin
		// until start signal is sent
		gameinfo.data.stage = PREGAME;
		setHalt();
	}
	sendCommand(COMM_FIRST_HALF, "Begin first half");
	return (true);
}

bool GameControl::beginHalfTime() 
{ 
	if (enabled) {
		if (gameinfo.data.stage != FIRST_HALF) 
			return (false);

		gameinfo.data.stage = HALF_TIME;
		setHalt();
		gameinfo.resetTimer();
	}
	sendCommand(COMM_HALF_TIME, "Begin half time");
	return (true);
}

bool GameControl::beginSecondHalf()
{ 
	if (enabled) {
		if (gameinfo.data.stage != HALF_TIME) 
			return (false);
		setHalt();
		gameinfo.data.stage = PRESECONDHALF;
	}
	sendCommand(COMM_SECOND_HALF, "Begin second half");
	return (true);
}

bool GameControl::beginOvertime1()
{ 
	if (enabled) {
		if (gameinfo.data.stage != SECOND_HALF) 
			return (false);

		gameinfo.data.stage = PREOVERTIME1;
		setHalt();
		gameinfo.resetTimer();
	}
	sendCommand(COMM_OVER_TIME1, "Begin overtime");
	return (true);
}

bool GameControl::beginOvertime2()
{ 
	if (enabled) {
		if (gameinfo.data.stage != OVER_TIME1) 
			return (false);

		gameinfo.data.stage = PREOVERTIME2;
		setHalt();
		gameinfo.resetTimer();
	}
	sendCommand(COMM_OVER_TIME2, "Begin overtime second half");
	return (true);
}

bool GameControl::beginPenaltyShootout()
{ 
	if (enabled) {
		if ((gameinfo.data.stage != OVER_TIME2) && (gameinfo.data.stage != SECOND_HALF)) {
			return (false);
		}

		gameinfo.data.stage = PENALTY_SHOOTOUT;
		setHalt();

		gameinfo.resetTimer();
	}
	sendCommand(COMM_PENALTY_SHOOTOUT, "Begin Penalty shootout");
	return (true);
}

/////////////////////////////////
// game control commands
bool GameControl::setHalt()
{ 
	if (enabled) {
		gameinfo.data.state = HALTED;
	}
	sendCommand(COMM_HALT, "Halting robots");
	return (true);
}

bool GameControl::setReady()
{ 
	if (!enabled) {
		sendCommand(COMM_READY, "STarting robots");
	} else {
		if (!gameinfo.isTimeout() && gameinfo.isPrestart()) {
			sendCommand(COMM_READY, "STarting robots");
			gameinfo.setRunning();

			// progress into the first half upon the start signal
			switch (gameinfo.data.stage) {
				case PREGAME:
					gameinfo.data.stage = FIRST_HALF;
					gameinfo.resetTimer();
					break;
				case PRESECONDHALF:
					gameinfo.data.stage = SECOND_HALF;
					gameinfo.resetTimer();
					break;
				case PREOVERTIME1:
					gameinfo.data.stage = OVER_TIME1;
					gameinfo.resetTimer();
					break;
				case PREOVERTIME2:
					gameinfo.data.stage = OVER_TIME2;
					gameinfo.resetTimer();
					break;
				default:
					break;
			}
		}
	}
	return (true);
}

bool GameControl::setStart()
{ 
	if (!enabled) {
		sendCommand(COMM_START, "Starting robots");
	} else {
		if (!gameinfo.isTimeout()) {
			sendCommand(COMM_START, "Starting robots");
			gameinfo.setRunning();

			// progress into the first half upon the start signal
			switch (gameinfo.data.stage) {
				case PREGAME:
					gameinfo.data.stage = FIRST_HALF;
					gameinfo.resetTimer();
					break;
				case PRESECONDHALF:
					gameinfo.data.stage = SECOND_HALF;
					gameinfo.resetTimer();
					break;
				case PREOVERTIME1:
					gameinfo.data.stage = OVER_TIME1;
					gameinfo.resetTimer();
					break;
				case PREOVERTIME2:
					gameinfo.data.stage = OVER_TIME2;
					gameinfo.resetTimer();
					break;
				default:
					break;
			}
		}
	}
	return true;
}

bool GameControl::setStop()
{ 
	sendCommand(COMM_STOP, "Stopping robots");

	if (enabled) {
		// progress out of half time if we hit stop
		if (gameinfo.data.stage == HALF_TIME) {
			//            beginSecondHalf();
			gameinfo.setStopped();
		} else {
			gameinfo.setStopped();
		}
	}
	return (true);
}

// maybe deprecate
bool GameControl::setCancel()
{ 
	sendCommand(COMM_CANCEL, "Sending cancel");

	// reset timeout if it is canceled
	if (gameinfo.data.state == TIMEOUT) {
		gameinfo.data.nrtimeouts[gameinfo.data.timeoutteam]++;
		gameinfo.data.timeouts[gameinfo.data.timeoutteam] = gameinfo.data.timeoutstarttime;
		gameinfo.data.state = gameinfo.data.laststate;
	}
	else 
	{
		// reset yellow card if it is canceled
		for (unsigned int x = 1; x < NUM_TEAMS; ++x) {
			if (gameinfo.data.timepenalty[x] > std::chrono::high_resolution_clock::duration::zero() && gameinfo.data.timepenalty[x] > gameinfo.data.timepenalty[x-1])
				gameinfo.data.timepenalty[x] = std::chrono::high_resolution_clock::duration::zero();
			else if (gameinfo.data.timepenalty[x-1] > std::chrono::high_resolution_clock::duration::zero())
				gameinfo.data.timepenalty[x-1] = std::chrono::high_resolution_clock::duration::zero();
		}
	}
	return (true);
}

///////////////////
// timeout control
bool GameControl::beginTimeout(Team team)
{
	sendCommand(CHOOSETEAM(team, COMM_TIMEOUT_BLUE, COMM_TIMEOUT_YELLOW), 
			Glib::ustring::compose(u8"Timeout %1", str_Team[team]));

	if (enabled) {
		if ((gameinfo.nrTimeouts(team) <= 0) || (gameinfo.timeoutRemaining(team) <= std::chrono::high_resolution_clock::duration::zero()))
			return (false);
		if (!gameinfo.isStopped() && !gameinfo.isHalted())
			return (false);

		gameinfo.data.laststate = gameinfo.data.state;
		gameinfo.data.state = TIMEOUT;
		gameinfo.data.nrtimeouts[team]--;
		gameinfo.data.timeoutteam = team;
		gameinfo.data.timeoutstarttime = gameinfo.timeoutRemaining(team);
	}

	return (true);
}

bool GameControl::stopTimeout()
{ 
	sendCommand(COMM_TIMEOUT_END, "End Timeout");

	if (enabled) {
		if (gameinfo.data.state != TIMEOUT)
			return (false);

		// necessary since we ignore halts for timeouts
		gameinfo.data.state = HALTED;
		setHalt();
	}
	return (true);
}


// status commands
bool GameControl::goalScored(Team team)
{ 
	sendCommand(CHOOSETEAM(team, COMM_GOAL_BLUE, COMM_GOAL_YELLOW), 
			Glib::ustring::compose(u8"Goal scored %1", str_Team[team]));

	if (enabled) {
		if ( (!gameinfo.isStopped() && !gameinfo.isHalted()) || (gameinfo.data.stage == PREGAME))
			return (false);

		if (gameinfo.data.stage == PENALTY_SHOOTOUT) {
			gameinfo.data.penaltygoals[team]++;
		} else {
			gameinfo.data.goals[team]++;
		}
	}

	return (true);
}

bool GameControl::removeGoal(Team team)
{ 
	sendCommand(CHOOSETEAM(team, COMM_SUBGOAL_BLUE, COMM_SUBGOAL_YELLOW), 
			Glib::ustring::compose(u8"Goal removed %1", str_Team[team]));

	if (enabled) {
		if ( (!gameinfo.isStopped() && !gameinfo.isHalted()) || (gameinfo.data.stage == PREGAME))
			return (false);

		if (gameinfo.data.stage == PENALTY_SHOOTOUT) {
			if (gameinfo.data.penaltygoals[team] > 0)
				gameinfo.data.penaltygoals[team]--;
		} else if (gameinfo.data.goals[team] > 0) {
			gameinfo.data.goals[team]--;
		}
	}

	return (true);
}

bool GameControl::awardYellowCard(Team team)
{ 
	sendCommand(CHOOSETEAM(team, COMM_YELLOWCARD_BLUE, COMM_YELLOWCARD_YELLOW), 
			Glib::ustring::compose(u8"Yellow card awarded %1", str_Team[team]));

	if (enabled) {
		if (!gameinfo.isStopped())
			return (false);
	}

	gameinfo.data.timepenalty[team] = gameinfo.data.yellowcard_time;

	return (true);
}


void GameControl::switchColors()
{
	static_assert(NUM_TEAMS == 2, "This function needs updating for NUM_TEAMS != 2.");
	std::swap(gameinfo.data.teamnames[0], gameinfo.data.teamnames[1]);
	std::swap(gameinfo.data.timeouts[0], gameinfo.data.timeouts[1]);
	std::swap(gameinfo.data.nrtimeouts[0], gameinfo.data.nrtimeouts[1]);
	std::swap(gameinfo.data.goals[0], gameinfo.data.goals[1]);
	std::swap(gameinfo.data.penaltygoals[0], gameinfo.data.penaltygoals[1]);
	std::swap(gameinfo.data.yellowcards[0], gameinfo.data.yellowcards[1]);
	std::swap(gameinfo.data.timepenalty[0], gameinfo.data.timepenalty[1]);
	std::swap(gameinfo.data.redcards[0], gameinfo.data.redcards[1]);
	std::swap(gameinfo.data.penalties[0], gameinfo.data.penalties[1]);
	std::swap(gameinfo.data.freekicks[0], gameinfo.data.freekicks[1]);
}


bool GameControl::awardRedCard(Team team)
{
	if (enabled) {
		if (!gameinfo.isStopped())
			return (false);
	}

	++gameinfo.data.redcards[team];

	sendCommand(CHOOSETEAM(team, COMM_REDCARD_BLUE, COMM_REDCARD_YELLOW), 
			Glib::ustring::compose(u8"Red card awarded %1", str_Team[team]));
	return (true);
}


// game restart commands
bool GameControl::setKickoff(Team team)
{
	sendCommand(CHOOSETEAM(team, COMM_KICKOFF_BLUE, COMM_KICKOFF_YELLOW), 
			Glib::ustring::compose(u8"Kickoff %1", str_Team[team]));

	if (enabled) {
		if (!gameinfo.isStopped() || !gameinfo.canRestart())
			return (false);
		gameinfo.setPrestart();
	}

	return (true);
}

bool GameControl::setPenalty(Team team)
{ 
	sendCommand(CHOOSETEAM(team, COMM_PENALTY_BLUE, COMM_PENALTY_YELLOW), 
			Glib::ustring::compose(u8"Penalty kick %1", str_Team[team]));

	if (enabled) {
		if (!gameinfo.isStopped() || !gameinfo.canRestart())
			return (false);

		gameinfo.setPrestart();
	}
	return (true);
}

bool GameControl::setDirect(Team team)
{ 
	sendCommand(CHOOSETEAM(team, COMM_DIRECT_BLUE, COMM_DIRECT_YELLOW), 
			Glib::ustring::compose(u8"Direct freekick %1", str_Team[team]));

	if (enabled) {
		if (!gameinfo.isStopped() || !gameinfo.canRestart())
			return (false);

		gameinfo.setRunning();
	}
	return (true);
}

bool GameControl::setIndirect(Team team)
{ 
	sendCommand(CHOOSETEAM(team, COMM_INDIRECT_BLUE, COMM_INDIRECT_YELLOW), 
			Glib::ustring::compose(u8"Indirect freekick %1", str_Team[team]));

	if (enabled) {
		if (!gameinfo.isStopped() || !gameinfo.canRestart())
			return (false);

		gameinfo.setRunning();
	}
	return (true);
}

