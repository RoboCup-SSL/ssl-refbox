#ifndef SAVEGAME_H
#define SAVEGAME_H

#include <string>

class SaveState;

void save_game(const SaveState &ss, const std::string &save_filename);

#endif

