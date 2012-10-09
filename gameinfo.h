/*
 * TITLE: gameinfo.h
 *
 * PURPOSE: Stores the status of the game
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

#ifndef GAMEINFO_H
#define GAMEINFO_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <glibmm.h>
#include <string>

Glib::ustring format_time_seconds(std::chrono::high_resolution_clock::duration t);
Glib::ustring format_time_deciseconds(std::chrono::high_resolution_clock::duration t);

#define NUM_TEAMS 2
enum Team {Blue, Yellow};
extern const char* str_Team[NUM_TEAMS];

/* game stage and status enums */
enum GameState {HALTED, STOPPED, TIMEOUT, PRESTART, RUNNING};
enum GameStage {PREGAME, FIRST_HALF, HALF_TIME, PRESECONDHALF, SECOND_HALF, 
	PREOVERTIME1, OVER_TIME1, PREOVERTIME2, OVER_TIME2, PENALTY_SHOOTOUT};
#define NR_GAME_STAGES (static_cast<unsigned int>(PENALTY_SHOOTOUT) + 1)
enum GameRestart {NEUTRAL, DIRECT, INDIRECT, PENALTY, KICKOFF};

class GameInfo {
	public:
		struct Data {
			//keep switchColors() in gamecontrol.cc in sync with team data variables!
			Glib::ustring teamnames[NUM_TEAMS];
			GameRestart restart;
			GameState state;
			GameStage stage;
			GameState laststate;

			std::chrono::high_resolution_clock::duration gametime; // time of the game

			std::chrono::high_resolution_clock::duration time_taken;

			std::chrono::high_resolution_clock::duration timelimits[NR_GAME_STAGES];

			std::chrono::high_resolution_clock::duration timeouts[NUM_TEAMS];
			int nrtimeouts[NUM_TEAMS];
			int timeoutteam;
			std::chrono::high_resolution_clock::duration timeoutstarttime;

			int goals[NUM_TEAMS];
			int penaltygoals[NUM_TEAMS];
			int yellowcards[NUM_TEAMS];
			std::chrono::high_resolution_clock::duration timepenalty[NUM_TEAMS];
			std::chrono::high_resolution_clock::duration yellowcard_time;
			int redcards[NUM_TEAMS];
			int penalties[NUM_TEAMS];
			int freekicks[NUM_TEAMS];
			int restarts;

			Data();
			void save(std::ostream &ofs) const;
			void load(std::istream &ifs);
		};        

		Data data;
		std::ofstream logfile;

		GameInfo(const std::string& logfname);

		void save(const std::string& fname) const;

		void load(const std::string& fname);

		void logCommand(char cmd, const Glib::ustring& msg);


		std::string getStateString() const {
			switch (data.state) {
				case HALTED:  return ("Halted"); break;
				case STOPPED: return ("Stopped"); break;
				case TIMEOUT: return ("Timeout"); break;
				case PRESTART:   return ("Prestart"); break;
				case RUNNING: return ("Running"); break;
				default: return ("Unknown state!!!!\n"); break;
			}
		}

		std::string getStageString() const {
			switch (data.stage) {
				case PREGAME:  return ("Pre-game"); break;
				case PRESECONDHALF:  return ("Pre-second half"); break;
				case PREOVERTIME1:  return ("Pre-overtime first half"); break;
				case PREOVERTIME2:  return ("Pre-overtime second half"); break;
				case FIRST_HALF:  return ("First Half"); break;
				case HALF_TIME:   return ("Half Time"); break;
				case SECOND_HALF: return ("Second Half"); break;
				case OVER_TIME1:   return ("Overtime first half"); break;
				case OVER_TIME2:   return ("Overtime second half"); break;
				case PENALTY_SHOOTOUT: return ("Penalty Shootout"); break;
				default: return ("Unknown stage!!!!\n"); break;
			}
		}

		std::string getRestartString() const {
			switch (data.stage) {
				case NEUTRAL:  return ("Neutral"); break;
				case DIRECT:   return ("Direct"); break;
				case INDIRECT: return ("Indirect"); break;
				case PENALTY:  return ("Penalty"); break;
				case KICKOFF:  return ("Kickoff"); break;
				default: return ("Unknown restart!!!!\n"); break;
			}
		}

		bool isTimeComplete() const {
			return (data.time_taken >= data.timelimits[static_cast<unsigned int>(data.stage)]);
		}

		std::chrono::high_resolution_clock::duration timeRemaining() const {
			return (std::max(std::chrono::high_resolution_clock::duration::zero(), data.timelimits[static_cast<unsigned int>(data.stage)] - data.time_taken));
		}

		std::chrono::high_resolution_clock::duration timeTaken() const {
			return (data.time_taken);
		}

		bool isTimeout() const {
			return (data.state == TIMEOUT);
		}

		bool isHalted() const {
			return (data.state == HALTED);
		}

		bool isStopped() const {
			return (data.state == STOPPED);
		}

		bool isPrestart() const {
			return (data.state == PRESTART);
		}

		bool isRunning() const {
			return (data.state == RUNNING);
		}

		bool isGeneralPlay() const {
			return (data.stage == FIRST_HALF || data.stage == SECOND_HALF 
					|| data.stage == OVER_TIME1 || data.stage == OVER_TIME2);
		}

		bool isGameTied() const {
			return (data.goals[Blue] == data.goals[Yellow]);
		}

		std::chrono::high_resolution_clock::duration timeoutRemaining(int team = -1) const {
			return (data.timeouts[((team < 0) ? data.timeoutteam : team)]);
		}

		int nrTimeouts(int team = -1) const {
			return (data.nrtimeouts[((team < 0) ? data.timeoutteam : team)]);
		}

		bool isTimeoutComplete() const {
			return (data.timeouts[data.timeoutteam] <= std::chrono::high_resolution_clock::duration::zero());
		}

		void resetTimer() {
			data.time_taken = std::chrono::high_resolution_clock::duration::zero();
		}

		bool canRestart() const {
			return (data.state == STOPPED);
		}

		void setRunning() {
			data.state = RUNNING;
		}

		void setPrestart() {
			data.state = PRESTART;
		}

		void setStopped() {
			data.state = STOPPED;
		}

		void setTimelimits(const std::chrono::high_resolution_clock::duration (&tlim)[NR_GAME_STAGES], const std::chrono::high_resolution_clock::duration (&touts)[NUM_TEAMS], int ntouts) {
			std::copy_n(tlim, NR_GAME_STAGES, data.timelimits);
			std::copy_n(touts, NUM_TEAMS, data.timeouts);
			std::fill_n(data.nrtimeouts, NUM_TEAMS, ntouts);
		}

		std::chrono::high_resolution_clock::duration penaltyTimeRemaining(int team = 0) const {
			return (data.timepenalty[team]);
		}
};

#endif

