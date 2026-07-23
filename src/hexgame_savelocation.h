#ifndef _HEXGAME_SAVELOCATION_H_
#define _HEXGAME_SAVELOCATION_H_

#include <stdbool.h>

#include "hexgame_location.h"
#include "lexer.h"
#include "stringstore.h"

struct hexgame;


typedef struct hexgame_savelocation {
    hexgame_location_t loc;
    const char *map_filename;
    const char *stateset_filename;
    const char *state_name;
} hexgame_savelocation_t;

void hexgame_savelocation_init(hexgame_savelocation_t *location);
void hexgame_savelocation_cleanup(hexgame_savelocation_t *location);
void hexgame_savelocation_set(hexgame_savelocation_t *location, vecspace_t *space,
    vec_t pos, rot_t rot, bool turn, const char *map_filename,
    const char *stateset_filename, const char *state_name);
void hexgame_savelocation_set_from_location(hexgame_savelocation_t *location,
    hexgame_location_t *loc, const char *map_filename,
    const char *stateset_filename, const char *state_name);
void hexgame_savelocation_write(hexgame_savelocation_t *location, FILE *file);
int hexgame_savelocation_parse(hexgame_savelocation_t *location,
    fus_lexer_t *lexer);

#endif
