

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "geom.h"
#include "hexgame_location.h"
#include "util.h"
#include "lexer.h"
#include "write.h"
#include "hexspace.h"



/************
 * LOCATION *
 ************/

rot_t hexgame_location_get_rot(hexgame_location_t *loc){
    /* Returns trf.rot, where trf is loc converted to a trf_t.

    * If !loc->turn, returns loc->rot.
    * If loc->turn, maps loc->rot to return value as follows:

        0 => 3
        1 => 2
        2 => 1
        3 => 0
        4 => 5
        5 => 4
    */

    rot_t rot = loc->rot;
    if(loc->turn){
        rot = rot_contain(HEXSPACE_ROT_MAX,
            HEXSPACE_ROT_MAX/2 - rot);
    }
    return rot;
}

void hexgame_location_init_trf(hexgame_location_t *loc, trf_t *trf){
    /* Converts loc to a trf_t. */
    vec_cpy(HEXSPACE_DIMS, trf->add, loc->pos);
    trf->rot = hexgame_location_get_rot(loc);
    trf->flip = loc->turn;
}

void hexgame_location_from_trf(hexgame_location_t *loc, trf_t *trf){
    /* Converts a trf_t to loc. */
    vec_cpy(HEXSPACE_DIMS, loc->pos, trf->add);
    loc->rot = trf->flip? (HEXSPACE_ROT_MAX/2 - trf->rot): trf->rot;
    loc->turn = trf->flip;
}

void hexgame_location_apply(hexgame_location_t *loc, trf_t *trf){
    /* Applies a trf_t to loc. */
    trf_t loctrf;
    hexgame_location_init_trf(loc, &loctrf);
    trf_apply(&hexspace, &loctrf, trf);
    hexgame_location_from_trf(loc, &loctrf);
}


/****************
 * SAVELOCATION *
 ****************/

void hexgame_savelocation_init(hexgame_savelocation_t *location){
    vec_zero(location->loc.pos);
    location->loc.rot = 0;
    location->loc.turn = false;
    location->map_filename = NULL;
    location->stateset_filename = NULL;
    location->state_name = NULL;
}

void hexgame_savelocation_cleanup(hexgame_savelocation_t *location){
    free(location->map_filename);
    free(location->stateset_filename);
    free(location->state_name);
}

void hexgame_savelocation_set(hexgame_savelocation_t *location, vecspace_t *space,
    vec_t pos, rot_t rot, bool turn, char *map_filename,
    char *stateset_filename, char *state_name
){
    vec_cpy(space->dims, location->loc.pos, pos);
    location->loc.rot = rot;
    location->loc.turn = turn;

    #define SET_A_THING(THING) if(location->THING != THING){ \
        free(location->THING); \
        location->THING = THING; \
    }
    SET_A_THING(map_filename)
    SET_A_THING(stateset_filename)
    SET_A_THING(state_name)
    #undef SET_A_THING
}

int hexgame_savelocation_save(const char *filename, hexgame_savelocation_t *location){
    FILE *f = fopen(filename, "w");
    if(f == NULL){
        fprintf(stderr, "Couldn't save player to %s: ", filename);
        perror(NULL);
        return 2;
    }
    fprintf(f, "%i %i %i %c ", location->loc.pos[0], location->loc.pos[1],
        location->loc.rot, location->loc.turn? 'y': 'n');
    fus_write_str(f, location->map_filename);
    if(location->stateset_filename){
        putc(' ', f);
        fus_write_str(f, location->stateset_filename);
        if(location->state_name){
            putc(' ', f);
            fus_write_str(f, location->state_name);
        }
    }
    fclose(f);
    return 0;
}

int hexgame_savelocation_load(const char *filename, hexgame_savelocation_t *location){
    int err = 0;

    char *text = load_file(filename);
    if(text == NULL){
        fprintf(stderr, "Couldn't load location from %s: ", filename);
        return 2;
    }

    fus_lexer_t lexer;
    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    int x, y;
    rot_t rot;
    bool turn;
    char *map_filename = NULL;
    char *stateset_filename = NULL;
    char *state_name = NULL;

    err = fus_lexer_get_int(&lexer, &x);
    if(err)goto err;
    err = fus_lexer_get_int(&lexer, &y);
    if(err)goto err;
    err = fus_lexer_get_int(&lexer, &rot);
    if(err)goto err;
    err = fus_lexer_get_yn(&lexer, &turn);
    if(err)goto err;
    err = fus_lexer_get_str(&lexer, &map_filename);
    if(err)goto err;
    if(!fus_lexer_done(&lexer)){
        err = fus_lexer_get_str(&lexer, &stateset_filename);
        if(err)goto err;
        if(!fus_lexer_done(&lexer)){
            err = fus_lexer_get_str(&lexer, &state_name);
            if(err)goto err;
        }
    }

    location->loc.pos[0] = x;
    location->loc.pos[1] = y;
    location->loc.rot = rot;
    location->loc.turn = turn;
    location->map_filename = map_filename;
    location->stateset_filename = stateset_filename;
    location->state_name = state_name;
    goto done;

err:
    free(map_filename);
    free(stateset_filename);
    free(state_name);
done:
    fus_lexer_cleanup(&lexer);
    free(text);
    return err;
}

