

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "geom.h"
#include "hexgame_savelocation.h"
#include "stringstore.h"
#include "lexer.h"
#include "lexer_macros.h"
#include "write.h"


#ifdef __EMSCRIPTEN__
void emccdemo_syncfs();
#endif


void hexgame_savelocation_init(hexgame_savelocation_t *location){
    vec_zero(location->loc.pos);
    location->loc.rot = 0;
    location->loc.turn = false;
    location->map_filename = NULL;
    location->stateset_filename = NULL;
    location->state_name = NULL;
}

void hexgame_savelocation_cleanup(hexgame_savelocation_t *location){
    /* Nuthin */
}

void hexgame_savelocation_set(hexgame_savelocation_t *location, vecspace_t *space,
    vec_t pos, rot_t rot, bool turn, const char *map_filename,
    const char *stateset_filename, const char *state_name
){
    vec_cpy(space->dims, location->loc.pos, pos);
    location->loc.rot = rot;
    location->loc.turn = turn;
    location->map_filename = map_filename;
    location->stateset_filename = stateset_filename;
    location->state_name = state_name;
}

void hexgame_savelocation_write(hexgame_savelocation_t *location, FILE *file){
    fprintf(file, "%i %i %i %c ", location->loc.pos[0], location->loc.pos[1],
        location->loc.rot, location->loc.turn? 'y': 'n');
    fus_write_str(file, location->map_filename);
    if(location->stateset_filename){
        putc(' ', file);
        fus_write_str(file, location->stateset_filename);
        if(location->state_name){
            putc(' ', file);
            fus_write_str(file, location->state_name);
        }
    }
}

int hexgame_savelocation_parse(hexgame_savelocation_t *location,
    fus_lexer_t *lexer, stringstore_t *filename_store, stringstore_t *name_store
){
    INIT

    int x, y;
    rot_t rot;
    bool turn;
    const char *map_filename = NULL;
    const char *stateset_filename = NULL;
    const char *state_name = NULL;

    GET_INT(x)
    GET_INT(y)
    GET_INT(rot)
    GET_YN(turn)
    GET_STR_CACHED(map_filename, filename_store)
    if(!DONE){
        GET_STR_CACHED(stateset_filename, filename_store)
        if(!DONE){
            GET_STR_CACHED(state_name, name_store)
        }
    }

    location->loc.pos[0] = x;
    location->loc.pos[1] = y;
    location->loc.rot = rot;
    location->loc.turn = turn;
    location->map_filename = map_filename;
    location->stateset_filename = stateset_filename;
    location->state_name = state_name;
    return 0;
}

