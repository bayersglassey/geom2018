

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "geom.h"
#include "hexgame_savelocation.h"
#include "hexgame.h"
#include "file_utils.h"
#include "lexer.h"
#include "write.h"
#include "var_utils.h"


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

int hexgame_savelocation_save(const char *filename,
    hexgame_savelocation_t *location, hexgame_t *game
){
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
    fputc('\n', f);

    fprintf(f, "vars:\n");
    vars_write(&game->vars, f, 4*1);

    fprintf(f, "maps:\n");
    /* !!! THIS IS A BIT OF A HACK!!!
    ...instead of hexgame_savelocation_t being a self-contained data
    structure, its save method here is dumping stuff from game->vars
    and game->maps. */
    for(int i = 0; i < game->maps_len; i++){
        hexmap_t *map = game->maps[i];
        fprintf(f, "    ");
        fus_write_str(f, map->name);
        fprintf(f, ":\n");
        fprintf(f, "        vars:\n");

        /* Don't write the vars which say they shouldn't be saved. */
        var_props_t nowrite_props_mask = 1 << HEXMAP_VARS_PROP_NOSAVE;
        vars_write_with_mask(&map->vars, f, 4*3, nowrite_props_mask);
    }

    fclose(f);

#   ifdef __EMSCRIPTEN__
    /* Needed to actually save the IDBFS data to browser's indexedDb. */
    emccdemo_syncfs();
#   endif

    return 0;
}

int hexgame_savelocation_load(const char *filename,
    hexgame_savelocation_t *location, hexgame_t *game,
    bool *file_found_ptr
){
    int err = 0;

    char *text = load_file(filename);
    if(file_found_ptr)*file_found_ptr = true;
    if(text == NULL){
        fprintf(stderr, "Couldn't load location from %s: ", filename);

        /* Indicate to caller that reason for returning a failure code
        of 2 is missing file.
        So caller can choose to treat that situation as something other
        than an outright error. */
        if(file_found_ptr)*file_found_ptr = false;

        return 2;
    }

    fus_lexer_t _lexer, *lexer=&_lexer;
    err = fus_lexer_init(lexer, text, filename);
    if(err)return err;

    int x, y;
    rot_t rot;
    bool turn;
    char *map_filename = NULL;
    char *stateset_filename = NULL;
    char *state_name = NULL;

    err = fus_lexer_get_int(lexer, &x);
    if(err)goto err;
    err = fus_lexer_get_int(lexer, &y);
    if(err)goto err;
    err = fus_lexer_get_int(lexer, &rot);
    if(err)goto err;
    err = fus_lexer_get_yn(lexer, &turn);
    if(err)goto err;
    err = fus_lexer_get_str(lexer, &map_filename);
    if(err)goto err;
    if(!fus_lexer_done(lexer)){
        err = fus_lexer_get_str(lexer, &stateset_filename);
        if(err)goto err;
        if(!fus_lexer_done(lexer)){
            err = fus_lexer_get_str(lexer, &state_name);
            if(err)goto err;
        }
    }

    if(fus_lexer_got(lexer, "vars")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = vars_parse(&game->vars, lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    /* !!! THIS IS A BIT OF A HACK!!!
    ...instead of hexgame_savelocation_t being a self-contained data
    structure, its load method here is loading stuff from game->maps. */
    if(fus_lexer_got(lexer, "maps")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        while(1){
            if(fus_lexer_got(lexer, ")"))break;
            char *name;
            err = fus_lexer_get_str(lexer, &name);
            if(err)return err;

            hexmap_t *map;
            err = hexgame_get_or_load_map(game, name, &map);
            if(err)return err;
            free(name);

            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            {
                if(fus_lexer_got(lexer, "vars")){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    err = fus_lexer_get(lexer, "(");
                    if(err)return err;
                    err = vars_parse(&map->vars, lexer);
                    if(err)return err;
                    err = fus_lexer_get(lexer, ")");
                    if(err)return err;
                }
            }
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }
        err = fus_lexer_next(lexer);
        if(err)return err;
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
    fus_lexer_cleanup(lexer);
    free(text);
    return err;
}

