#ifndef _HEXGAME_SAVELOCATION_H_
#define _HEXGAME_SAVELOCATION_H_

#include <stdbool.h>

#include "geom.h"


typedef struct hexgame_location {
    vec_t pos;
    rot_t rot;
    bool turn;
} hexgame_location_t;

typedef struct hexgame_savelocation {
    hexgame_location_t loc;
    char *map_filename;
    char *anim_filename;
    char *state_name;
} hexgame_savelocation_t;

void hexgame_savelocation_init(hexgame_savelocation_t *location);
void hexgame_savelocation_cleanup(hexgame_savelocation_t *location);
void hexgame_savelocation_set(hexgame_savelocation_t *location, vecspace_t *space,
    vec_t pos, rot_t rot, bool turn, char *map_filename,
    char *anim_filename, char *state_name);
int hexgame_savelocation_save(const char *filename, hexgame_savelocation_t *location);
int hexgame_savelocation_load(const char *filename, hexgame_savelocation_t *location);

#endif