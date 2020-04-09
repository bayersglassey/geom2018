

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "geom.h"
#include "location.h"
#include "util.h"
#include "lexer.h"
#include "write.h"



/************
 * LOCATION *
 ************/

void location_init(location_t *location){
    vec_zero(location->pos);
    location->rot = 0;
    location->turn = false;
    location->map_filename = NULL;
    location->anim_filename = NULL;
    location->state_name = NULL;
}

void location_cleanup(location_t *location){
    free(location->map_filename);
    free(location->anim_filename);
    free(location->state_name);
}

void location_set(location_t *location, vecspace_t *space,
    vec_t pos, rot_t rot, bool turn, char *map_filename,
    char *anim_filename, char *state_name
){
    vec_cpy(space->dims, location->pos, pos);
    location->rot = rot;
    location->turn = turn;

    #define SET_A_THING(THING) if(location->THING != THING){ \
        free(location->THING); \
        location->THING = THING; \
    }
    SET_A_THING(map_filename)
    SET_A_THING(anim_filename)
    SET_A_THING(state_name)
    #undef SET_A_THING
}

int location_save(const char *filename, location_t *location){
    FILE *f = fopen(filename, "w");
    if(f == NULL){
        fprintf(stderr, "Couldn't save player to %s: ", filename);
        perror(NULL);
        return 2;
    }
    fprintf(f, "%i %i %i %c ", location->pos[0], location->pos[1],
        location->rot, location->turn? 'y': 'n');
    fus_write_str(f, location->map_filename);
    if(location->anim_filename){
        putc(' ', f);
        fus_write_str(f, location->anim_filename);
        if(location->state_name){
            putc(' ', f);
            fus_write_str(f, location->state_name);
        }
    }
    fclose(f);
    return 0;
}

int location_load(const char *filename, location_t *location){
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
    char *anim_filename = NULL;
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
        err = fus_lexer_get_str(&lexer, &anim_filename);
        if(err)goto err;
        if(!fus_lexer_done(&lexer)){
            err = fus_lexer_get_str(&lexer, &state_name);
            if(err)goto err;
        }
    }

    location->pos[0] = x;
    location->pos[1] = y;
    location->rot = rot;
    location->turn = turn;
    location->map_filename = map_filename;
    location->anim_filename = anim_filename;
    location->state_name = state_name;
    goto done;

err:
    free(map_filename);
    free(anim_filename);
    free(state_name);
done:
    fus_lexer_cleanup(&lexer);
    free(text);
    return err;
}

