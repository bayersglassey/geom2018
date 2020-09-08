#ifndef _HEXGAME_LOCATION_H_
#define _HEXGAME_LOCATION_H_

#include <stdbool.h>

#include "geom.h"


/************
 * LOCATION *
 ************/

typedef struct hexgame_location {
    /* Applied in the following order: pos, rot, turn */
    vec_t pos;
    rot_t rot;
    bool turn;
} hexgame_location_t;

rot_t hexgame_location_get_rot(hexgame_location_t *loc);
void hexgame_location_init_trf(hexgame_location_t *loc, trf_t *trf);


/****************
 * SAVELOCATION *
 ****************/

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