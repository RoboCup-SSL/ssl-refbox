/*
 * TITLE: gamecontrol.h
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

#ifndef __GAME_CONTROL_H__
#define __GAME_CONTROL_H__

#include "gameinfo.h"
#include "commands.h"
#include "logging.h"

#include <string>

class Settings;
class Serial;
class UDP_Broadcast;

// structure for determining what buttons are useable
class EnableState {
public:
  bool enable;
  bool halt;
  bool ready;
  bool stop;
  bool start;
  bool cancel;
  bool switch_colors;

  bool stage_firsthalf;
  bool stage_halftime;
  bool stage_secondhalf;
  bool stage_overtime1;
  bool stage_overtime2;
  bool stage_penaltyshootout;
  bool stage_gameover;
  
  bool timeout[NUM_TEAMS];
  bool endtimeout;

  bool goal[NUM_TEAMS];
  bool subgoal[NUM_TEAMS];
  bool kickoff[NUM_TEAMS];
  bool penalty[NUM_TEAMS];
  bool direct[NUM_TEAMS];
  bool indirect[NUM_TEAMS];
  bool cards;

/*   EnableState() { */
/*     printf("DO NOT CALL THIS!\n"); */
/*     enable = halt = ready = stop = start = cancel = true; */
/*     endtimeout = true; */
/*     cards = true; */
/*     for (int t = 0; t < NUM_TEAMS; t++) { */
/*       timeout[t] = goal[t] = subgoal[t] = true; */
/*       kickoff[t] = penalty[t] = direct[t] = indirect[t] = true; */
/*     } */
/*   } */

  void setEnabledStageControl(const GameState state, const GameStage stage,
                              const bool enabled)
  {
    if(!enabled)
    {
      stage_firsthalf = true;
      stage_halftime = true;
      stage_secondhalf = true;
      stage_overtime1 = true;
      stage_overtime2 = true;
      stage_penaltyshootout = true;
      stage_gameover = true;
      return;
    }

    //normal (playing) case: change not allowed
    stage_firsthalf = false;
    stage_halftime = false;
    stage_secondhalf = false;
    stage_overtime1 = false;
    stage_overtime2 = false;
    stage_penaltyshootout = false;
    stage_gameover = false;

    //allow stage control from halt in half-time.
    if(stage == HALF_TIME)
    {
        stage_secondhalf = true;
    }

    //allow change to the next stage if the game is stopped
    if(state == STOPPED)
    {
      switch(stage)
      {
      case FIRST_HALF:
          stage_halftime=true;
          break;
      case SECOND_HALF:
          stage_overtime1=true;
          stage_gameover=true;
          break;
      case OVER_TIME1:
          stage_gameover=true;
          stage_overtime2=true;
          break;
      case OVER_TIME2:
          stage_penaltyshootout=true;
      case PENALTY_SHOOTOUT:
          stage_gameover=true;
          break;
      default:
          ;
      }
    }
  }
    
  EnableState(GameState state = HALTED, GameStage stage = PREGAME, 
              bool enabled = true)
  {
    if (!enabled) {
      enable = switch_colors = halt = ready = stop = start = cancel = true;
      endtimeout = true;
      cards = true;
      cancel = true; //enable commands to cancel timeout and cards
      for (int t = 0; t < NUM_TEAMS; t++) {
        timeout[t] = true;
        goal[t] = true;
        subgoal[t] = true;
        kickoff[t] = true;
        penalty[t] = true;
        direct[t] = true;
        indirect[t] = true;
      }
    } else {
      enable = enabled;
      bool isstopped = (state == STOPPED);
      bool isprestart = (state == PRESTART);
      bool ishalted = (state == HALTED);
      stop = !isstopped;
      cards = isstopped;
      halt = state != HALTED;
      cancel = false;
      switch (stage) {
      case PREGAME:
      case HALF_TIME:
      case PRESECONDHALF: 
      case PREOVERTIME1: 
      case PREOVERTIME2: 
        //        start = !halt; //isstopped;
        switch_colors = ishalted;      
        stop = true;
        start = (state==STOPPED) || (state==PRESTART);
        ready = isprestart;
        setRestarts(isstopped, true);
        setTimeouts(isstopped);
        setGoals(false);
        break;
      case FIRST_HALF:  
      case SECOND_HALF: 
      case OVER_TIME1:
      case OVER_TIME2:
        start = (state==STOPPED || state==PRESTART);
        //        start = !halt; //isstopped;
        switch_colors = false;
        ready = isprestart;
        setTimeouts(isstopped);
        setGoals(isstopped);
        setRestarts(isstopped);
        break;
      case PENALTY_SHOOTOUT:
        setTimeouts(isstopped);
        setGoals(isstopped);
        setRestarts(isstopped);
        break;
      default:
        setTimeouts(false);
        setGoals(false);
        setRestarts(false);
      }
    }

    setEnabledStageControl(state,stage,enabled);
  }

  void setRestarts(bool en = true, bool kickoffonly = false) {

    for (int t = 0; t < NUM_TEAMS; t++) {
      kickoff[t] = en;
      if (!kickoffonly) {
        penalty[t] = direct[t] = indirect[t] = en;
      } else {
        penalty[t] = direct[t] = indirect[t] = false;
      }
    }
  }

  void setGoals(bool en = true) {
    for (int t = 0; t < NUM_TEAMS; t++) {
      goal[t] = subgoal[t] = en;
    }
  }

  void setTimeouts(bool en = true) {
    timeout[0] = timeout[1] = endtimeout = en;
  }
};


/* structure for encapsulating all the game control data */
class  GameControl {
public:
  struct GameStatePacket{
    char cmd;                      // current referee command
    unsigned char cmd_counter;     // increments each time new command is set
    unsigned char goals_blue;      // current score for blue team
    unsigned char goals_yellow;    // current score for yellow team
    unsigned short time_remaining; // seconds remaining for current game stage
  };

  Logging* log;
  Settings* settings;
  Serial* serial;

  /** Constructor */
  GameControl();

  /** Destructor */
  ~GameControl();


private:
  std::string savename;

  //serial interface
  std::string serdev;


  //ethernet interface
  std::string mc_addr;
  int mc_port;
  UDP_Broadcast *broadcast;

  GameInfo gameinfo;
  double tlast;
  
  // configuration
  // read a config file to fill in parameters
  bool readSettings(const std::string& configfile);
  
  // last Command sent.
  char         lastCommand;

  // incremented if a new command was sent.
  unsigned int lastCommandCounter;    

  // log commands, send them over serial and change game state    
  void sendCommand(const char cmd, const char *msg);
  void ethernetSendCommand(const char cmd, const unsigned int counter);
  char *concatTeam(char *msg, const char *msgpart, Team team);

  bool enabled;
  bool has_serial;


public:
  // initializes sets up everything
  bool init(const std::string& confname,
            const char *logfname,
            bool restart = false);

  void close();

  // debugging printout
  void print(FILE *f = stdout);

  // get the info to display
  GameInfo getGameInfo() {
      return (gameinfo);
  }

  bool isEnabled() {
    return (enabled);
  }

  void toggleEnable() {
    enabled = !enabled;
    log->add("enabled %i\n", enabled);
  }
  void setEnable(bool en = true) {
    enabled = en;
    log->add("setting enabled %i\n", enabled);
  }

  EnableState getEnableState() {
    EnableState es(gameinfo.data.state, gameinfo.data.stage, enabled);
    return (es);
  }

  bool switchColors();

	/////////////////////////////
	// send commands
	bool beginFirstHalf();
	bool beginHalfTime();
	bool beginSecondHalf();
	bool beginOvertime1();
	bool beginOvertime2();
	bool beginPenaltyShootout();

	bool beginTimeout(Team team);
	bool stopTimeout();

	bool goalScored(Team team);
	bool removeGoal(Team team);

	bool awardYellowCard(Team team);
	bool awardRedCard(Team team);

	bool setKickoff(Team team);
	bool setPenalty(Team team);
	bool setDirect(Team team);
	bool setIndirect(Team team);

        void setTeamName(Team team, const std::string& name)
        {
            //max. 63 chars + null
            strncpy(gameinfo.data.teamnames[team], name.c_str(), 63);
            gameinfo.data.teamnames[team][63] = '\0';
        }  

	bool setHalt();
	bool setReady();
	bool setStart();
	bool setStop();

	// maybe deprecate
	bool setCancel();

	// update times etc
	void stepTime();

};



#endif
