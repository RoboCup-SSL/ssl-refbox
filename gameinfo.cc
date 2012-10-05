#include <ostream>
#include "gameinfo.h"


const char* str_GameState[] = {
    "HALTED", "STOPPED", "TIMEOUT", "PRESTART", "RUNNING"
};

const char* str_GameStage[] = {
    "PREGAME",
    "FIRST_HALF", "HALF_TIME", "PRESECONDHALF", "SECOND_HALF",
    "PREOVERTIME1", "OVER_TIME1", "PREOVERTIME2", "OVER_TIME2",
    "PENALTY_SHOOTOUT"
};  

const char* str_GameRestart[] = {
    "NEUTRAL", "DIRECT", "INDIRECT", "PENALTY", "KICKOFF"
};

