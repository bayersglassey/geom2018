#ifndef _SAVE_SLOTS_H_
#define _SAVE_SLOTS_H_

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

#include "hexgame.h"


#define SAVE_SLOTS 3

#define SAVE_FORMAT_VERSION 0


const char *get_save_slot_filename(int i);
bool get_save_slot_file_exists(int i);
int delete_save_slot(int i);

int hexgame_save(hexgame_t *game, const char *filename);
int hexgame_load(hexgame_t *game, const char *filename,
    bool *bad_version_ptr);

#endif