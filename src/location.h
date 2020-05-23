#ifndef _LOCATION_H_
#define _LOCATION_H_

#include <stdbool.h>

#include "geom.h"


typedef struct location {
    vec_t pos;
    rot_t rot;
    bool turn;
    char *map_filename;
    char *anim_filename;
    char *state_name;
} location_t;

void location_init(location_t *location);
void location_cleanup(location_t *location);
void location_set(location_t *location, vecspace_t *space,
    vec_t pos, rot_t rot, bool turn, char *map_filename,
    char *anim_filename, char *state_name);
int location_save(const char *filename, location_t *location);
int location_load(const char *filename, location_t *location);

#endif