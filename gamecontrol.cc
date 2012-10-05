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

#include <cstdio>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sstream>
#include <iostream>

#ifndef WIN32
#  include <arpa/inet.h> //for htonl()
#else
#  include <winsock.h>
#endif

#include "commands.h"
#include "gamecontrol.h"
#include "serial.h"
#include "udp_broadcast.h"
#include "settings.h"
#include "logging.h"


#define MAX_LINE 256

#define CHOOSETEAM(t, blue, yel) (((t) == Blue) ? (blue) : (yel))


GameControl::GameControl()
{
    log = new Logging();
    settings = new Settings(log);
    serial = new Serial(log);
    broadcast = new UDP_Broadcast(log);
}


GameControl::~GameControl()
{
    delete settings;
    delete serial;
    delete broadcast;
    delete log;
}


// initializes sets up everything
bool GameControl::init(const std::string& configfile, const char *logfname, bool restart) 
{
    lastCommand = 'H'; //HALT
    lastCommandCounter = 0;

    enabled = true;
    has_serial = false; //will be set to true if found.
    // default serial device
#ifdef WIN32
    serdev = "COM1:";
#else
    serdev = "/dev/ttyS0";
#endif

    // default multicast address
    mc_addr = "224.5.29.1";
    mc_port = 10001;
  
    log->add("reading config file...");
    readSettings(configfile);
    log->add("config file read.\n");

    try {
        broadcast->set_destination(mc_addr, mc_port);
    }
    catch (UDP_Broadcast::IOError& e)
    {
        log->add("Ethernet failed: %s\n", e.what().c_str());
    }

    // a little user output
    print();

    /* open the serial port */
    log->add("Serial: Opening Serial Connection on device %s ...",
             serdev.c_str());
    if (!serial->Open(serdev, COMM_BAUD_RATE)) {
        log->add("ERROR: Cannot open serial connection.");
        //return (false);
    } else {
        log->add("Serial: using device \"%s\".", serdev.c_str());
        has_serial = true;
    }
    // intialize the timer
    gameinfo.resetTimer();
    tlast = getTime();

    if (!gameinfo.openLog(logfname)) {
        log->add("ERROR: Cannot open log file \"%s\"..", logfname);
        return (false);
    }

    // restart from saved state
    if (restart) {
        gameinfo.load(savename);
    }

    return (true);
}
		

void GameControl::close() 
{
    gameinfo.closeLog();
    serial->Close();
}

void GameControl::print(FILE *f) 
{
    fprintf(f, "Game Settings\n");
    fprintf(f, "\ttimelimits : First Half %i:%02i\n",
            DISP_MIN(gameinfo.data.timelimits[FIRST_HALF]), 
            (int)DISP_SEC(gameinfo.data.timelimits[FIRST_HALF]));
    fprintf(f, "\t\tHalf time %i:%02i\n",
            DISP_MIN(gameinfo.data.timelimits[HALF_TIME]), 
            (int)DISP_SEC(gameinfo.data.timelimits[HALF_TIME]));
    fprintf(f, "\t\tSecond half %i:%02i\n",
            DISP_MIN(gameinfo.data.timelimits[SECOND_HALF]), 
            (int)DISP_SEC(gameinfo.data.timelimits[SECOND_HALF]));
    fprintf(f, "\t\tOvertime %i:%02i\n",
            DISP_MIN(gameinfo.data.timelimits[OVER_TIME1]),
            (int)DISP_SEC(gameinfo.data.timelimits[OVER_TIME1]));
    fprintf(f, "\t\tOvertime %i:%02i\n",
            DISP_MIN(gameinfo.data.timelimits[OVER_TIME2]),
            (int)DISP_SEC(gameinfo.data.timelimits[OVER_TIME2]));

    fprintf(f, "\ttimeouts : number %i, total time %f\n",
            gameinfo.data.nrtimeouts[0], SEC2MIN(gameinfo.data.timeouts[0]));
}

/////////////////////////////
// send commands
// log commands, send them over serial and change game state
// increment command counter
void GameControl::sendCommand(char cmd, const char *msg) 
{
    lastCommand = cmd;
    lastCommandCounter++;

    log->add("Sending '%c': %s", cmd, msg);
    gameinfo.writeLog("Sending %c: %s", cmd, msg);

    ethernetSendCommand(cmd, lastCommandCounter);
    
    if (has_serial)
    {
        serial->WriteByte(cmd);
    }
}


/////////////////////////////
// send command to ethernet clients.
void GameControl::ethernetSendCommand(const char cmd, const unsigned int counter)
{
    GameStatePacket p;
    p.cmd          = cmd;
    p.cmd_counter  = lastCommandCounter & 0xFF;
    p.goals_blue   = gameinfo.data.goals[Blue  ] & 0xFF;
    p.goals_yellow = gameinfo.data.goals[Yellow] & 0xFF;
    p.time_remaining = htons((int)floor(gameinfo.timeRemaining()));

    try
    {
        broadcast->send(&p,sizeof(p));
    }
    catch (UDP_Broadcast::IOError& e)
    {
        std::cerr << "!! UDP_Broadcast: " << e.what() << std::endl;
    }
    
}
    
void GameControl::stepTime() 
{
    double tnew = getTime();
    double dt = tnew - tlast;
    tlast = tnew;

    //  printf("game state %i\n", gameinfo.data.state);

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
        if ((gameinfo.data.stage == HALF_TIME) ||
            !gameinfo.isHalted()) {
            gameinfo.data.time_taken += dt;
            for (int x = 0; x < NUM_TEAMS; ++x) {
                if (gameinfo.data.timepenalty[x] > 0) {
                    gameinfo.data.timepenalty[x] -= dt;
                } else {
                    gameinfo.data.timepenalty[x] = 0;
                }
            }
      
        }
    }

    // repeat last command (if someone missed it)
    ethernetSendCommand(lastCommand, lastCommandCounter);
}


////////////////////////////////////////
// configuration
// read a config file to fill in parameters
bool GameControl::readSettings(const std::string& configfile) 
{
    settings->readFile(configfile);

    settings->get("SAVENAME", savename);
    settings->get("SERIALDEVICE", serdev);
    settings->get("MULTICASTADDRESS", mc_addr);
    settings->get("MULTICASTPORT", mc_port);

    int timeout_seconds;
    settings->get("TIMEOUT_LIMIT", timeout_seconds);
    gameinfo.data.timeouts[(int)Blue] = timeout_seconds;
    gameinfo.data.timeouts[(int)Yellow] = timeout_seconds;

    int timeout_number;
    settings->get("NR_TIMEOUTS", timeout_number);
    gameinfo.data.nrtimeouts[(int)Blue] = timeout_number;
    gameinfo.data.nrtimeouts[(int)Yellow] = timeout_number;

    int timelimit_firsthalf;
    settings->get("FIRST_HALF", timelimit_firsthalf);
    gameinfo.data.timelimits[FIRST_HALF] = timelimit_firsthalf;

    int timelimit_halftime;
    settings->get("HALF_TIME", timelimit_halftime);
    gameinfo.data.timelimits[HALF_TIME] = timelimit_halftime;

    int timelimit_secondhalf;
    settings->get("SECOND_HALF", timelimit_secondhalf);
    gameinfo.data.timelimits[SECOND_HALF] = timelimit_secondhalf;

    int timelimit_overtime;
    settings->get("OVER_TIME", timelimit_overtime);
    gameinfo.data.timelimits[OVER_TIME1] = timelimit_overtime;
    gameinfo.data.timelimits[OVER_TIME2] = timelimit_overtime;

    int timelimit_yellowcard;
    settings->get("YELLOWCARD_TIME", timelimit_yellowcard);
    gameinfo.data.yellowcard_time = timelimit_yellowcard;

    return (true);
}




/////////////////////////////
// game stage commands
bool GameControl::beginFirstHalf()
{
    if (enabled) {
        if (gameinfo.data.stage != PREGAME) 
            return (false);

        // send the first half signal but we do not oficially begin
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
        if ((gameinfo.data.stage != OVER_TIME2) && 
            (gameinfo.data.stage != SECOND_HALF)) {
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
      
            // progress into teh first half upon the start signal
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
        //    if (!gameinfo.isTimeout() && gameinfo.isStopped()) {
        if (!gameinfo.isTimeout()) {
            sendCommand(COMM_START, "Starting robots");
            gameinfo.setRunning();
      
            // progress into teh first half upon the start signal
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
        for (int x = 1; x < NUM_TEAMS; ++x)
            if (gameinfo.data.timepenalty[x] > 0 && gameinfo.data.timepenalty[x] > gameinfo.data.timepenalty[x-1])
                gameinfo.data.timepenalty[x] = 0.0;
            else if (gameinfo.data.timepenalty[x-1] > 0)
                gameinfo.data.timepenalty[x-1] = 0.0;
    }
    return (true);
}

///////////////////
// timeout control
bool GameControl::beginTimeout(Team team)
{
    char msg[256];
    sendCommand(CHOOSETEAM(team, COMM_TIMEOUT_BLUE, COMM_TIMEOUT_YELLOW), 
                concatTeam(msg, "Timeout", team));


    if (enabled) {
        if ((gameinfo.nrTimeouts(team) <= 0) || (gameinfo.timeoutRemaining(team) <= 0)) {
            return (false);
        }
        if (!gameinfo.isStopped() && !gameinfo.isHalted())
            return (false);
    
        gameinfo.data.laststate = gameinfo.data.state;
        gameinfo.data.state = TIMEOUT;
        gameinfo.data.nrtimeouts[(int)team]--;
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
    char msg[256];
    sendCommand(CHOOSETEAM(team, COMM_GOAL_BLUE, COMM_GOAL_YELLOW), 
                concatTeam(msg, "Goal scored", team));


    if (enabled) {
        if ( (!gameinfo.isStopped() && !gameinfo.isHalted()) ||
            (gameinfo.data.stage == PREGAME))
            return (false);
    
        if (gameinfo.data.stage == PENALTY_SHOOTOUT) {
            gameinfo.data.penaltygoals[(int) team]++;
        } else {
            gameinfo.data.goals[(int) team]++;
        }
    }
    
    return (true);
}

bool GameControl::removeGoal(Team team)
{ 
    char msg[256];
    sendCommand(CHOOSETEAM(team, COMM_SUBGOAL_BLUE, COMM_SUBGOAL_YELLOW), 
                concatTeam(msg, "Goal removed", team));


    if (enabled) {
        if ( (!gameinfo.isStopped() && !gameinfo.isHalted()) ||
            (gameinfo.data.stage == PREGAME))
            return (false);
    
        if (gameinfo.data.stage == PENALTY_SHOOTOUT) {
            if (gameinfo.data.penaltygoals[(int) team] > 0)
                gameinfo.data.penaltygoals[(int) team]--;
        } else if (gameinfo.data.goals[team] > 0) {
            gameinfo.data.goals[(int) team]--;
        }
    }
	
    return (true);
}

bool GameControl::awardYellowCard(Team team)
{ 
    char msg[256];
    sendCommand(CHOOSETEAM(team, COMM_YELLOWCARD_BLUE, COMM_YELLOWCARD_YELLOW), 
                concatTeam(msg, "Yellow card awarded", team));


    if (enabled) {
        if (!gameinfo.isStopped())
            return (false);
    }
  
    gameinfo.data.timepenalty[team] = gameinfo.data.yellowcard_time;
  
    return (true);
}


bool GameControl::switchColors()
{
    int tmp_i;
    double tmp_d;
    char tmp_teamname[64];

    strncpy(tmp_teamname, gameinfo.data.teamnames[0], 64);
    strncpy(gameinfo.data.teamnames[0], gameinfo.data.teamnames[1], 64);
    strncpy(gameinfo.data.teamnames[1], tmp_teamname, 64);

    tmp_d = gameinfo.data.timeouts[0];
    gameinfo.data.timeouts[0] = gameinfo.data.timeouts[1];
    gameinfo.data.timeouts[1] = tmp_d;

    tmp_d = gameinfo.data.timepenalty[0];
    gameinfo.data.timepenalty[0] = gameinfo.data.timepenalty[1];
    gameinfo.data.timepenalty[1] = tmp_d;

    tmp_i = gameinfo.data.nrtimeouts[0];
    gameinfo.data.nrtimeouts[0] = gameinfo.data.nrtimeouts[1];
    gameinfo.data.nrtimeouts[1] = tmp_i;

    tmp_i = gameinfo.data.goals[0];
    gameinfo.data.goals[0] = gameinfo.data.goals[1];
    gameinfo.data.goals[1] = tmp_i;

    tmp_i = gameinfo.data.penaltygoals[0];
    gameinfo.data.penaltygoals[0] = gameinfo.data.penaltygoals[1];
    gameinfo.data.penaltygoals[1] = tmp_i;

    tmp_i = gameinfo.data.yellowcards[0];
    gameinfo.data.yellowcards[0] = gameinfo.data.yellowcards[1];
    gameinfo.data.yellowcards[1] = tmp_i;

    tmp_i = gameinfo.data.redcards[0];
    gameinfo.data.redcards[0] = gameinfo.data.redcards[1];
    gameinfo.data.redcards[1] = tmp_i;

    tmp_i = gameinfo.data.penalties[0];
    gameinfo.data.penalties[0] = gameinfo.data.penalties[1];
    gameinfo.data.penalties[1] = tmp_i;

    tmp_i = gameinfo.data.freekicks[0];
    gameinfo.data.freekicks[0] = gameinfo.data.freekicks[1];
    gameinfo.data.freekicks[1] = tmp_i;
    return true;
}


bool GameControl::awardRedCard(Team team)
{
    if (enabled) {
        if (!gameinfo.isStopped())
            return (false);
    }
	
    gameinfo.data.timepenalty[team] = 0.0;
  
    ++gameinfo.data.redcards[team];
  
    char msg[256];
    sendCommand(CHOOSETEAM(team, COMM_REDCARD_BLUE, COMM_REDCARD_YELLOW), 
                concatTeam(msg, "Yellow card awarded", team));
    return (true);
}


// game restart commands
bool GameControl::setKickoff(Team team)
{
    char msg[256];
    sendCommand(CHOOSETEAM(team, COMM_KICKOFF_BLUE, COMM_KICKOFF_YELLOW), 
                concatTeam(msg, "Kickoff", team));

    if (enabled) {
        if (!gameinfo.isStopped() || !gameinfo.canRestart())
            return (false);
        gameinfo.setPrestart();
    }
	
    return (true);
}

bool GameControl::setPenalty(Team team)
{ 
    char msg[256];
    sendCommand(CHOOSETEAM(team, COMM_PENALTY_BLUE, COMM_PENALTY_YELLOW), 
                concatTeam(msg, "Penalty kick", team));


    if (enabled) {
        if (!gameinfo.isStopped() || !gameinfo.canRestart())
            return (false);
	
        gameinfo.setPrestart();
    }
    return (true);
}

bool GameControl::setDirect(Team team)
{ 
    char msg[256];
    sendCommand(CHOOSETEAM(team, COMM_DIRECT_BLUE, COMM_DIRECT_YELLOW), 
                concatTeam( msg, "Direct freekick", team));


    if (enabled) {
        if (!gameinfo.isStopped() || !gameinfo.canRestart())
            return (false);
	
        gameinfo.setRunning();
    }
    return (true);
}

bool GameControl::setIndirect(Team team)
{ 
    char msg[256];
    sendCommand(CHOOSETEAM(team, COMM_INDIRECT_BLUE, COMM_INDIRECT_YELLOW), 
                concatTeam(msg, "Indirect freekick", team));


    if (enabled) {
        if (!gameinfo.isStopped() || !gameinfo.canRestart())
            return (false);
	
        gameinfo.setRunning();
    }
    return (true);
}


char *GameControl::concatTeam(char *msg, const char *msgpart, Team team)
{
    strcpy(msg, msgpart);
    if (team == Blue)
        strcat(msg, " Blue");
    else
        strcat(msg, " Yellow");
    return (msg);
}
 
