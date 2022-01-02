

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "geom.h"
#include "hexgame_savelocation.h"
#include "hexgame.h"
#include "file_utils.h"
#include "lexer.h"
#include "lexer_macros.h"
#include "write.h"
#include "var_utils.h"
#include "hexgame_vars_props.h"


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
        var_props_t nowrite_props_mask = 1 << HEXGAME_VARS_PROP_NOSAVE;
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
    INIT

    prismelrenderer_t *prend = game->prend;

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
    const char *map_filename = NULL;
    const char *stateset_filename = NULL;
    const char *state_name = NULL;

    GET_INT(x)
    GET_INT(y)
    GET_INT(rot)
    GET_YN(turn)
    GET_STR_CACHED(map_filename, &prend->filename_store)
    if(!DONE){
        GET_STR_CACHED(stateset_filename, &prend->filename_store)
        if(!DONE){
            GET_STR_CACHED(state_name, &prend->name_store)
        }
    }

    if(GOT("vars")){
        NEXT
        OPEN
        err = vars_parse(&game->vars, lexer);
        if(err)return err;
        CLOSE
    }

    /* !!! THIS IS A BIT OF A HACK!!!
    ...instead of hexgame_savelocation_t being a self-contained data
    structure, its load method here is loading stuff from game->maps. */
    if(GOT("maps")){
        NEXT
        OPEN
        while(1){
            if(GOT(")"))break;
            const char *name;
            GET_STR_CACHED(name, &prend->name_store)

            hexmap_t *map;
            err = hexgame_get_or_load_map(game, name, &map);
            if(err)return err;

            OPEN
            {
                if(GOT("vars")){
                    NEXT
                    OPEN
                    err = vars_parse(&map->vars, lexer);
                    if(err)return err;
                    CLOSE
                }
            }
            CLOSE
        }
        NEXT
    }

    location->loc.pos[0] = x;
    location->loc.pos[1] = y;
    location->loc.rot = rot;
    location->loc.turn = turn;
    location->map_filename = map_filename;
    location->stateset_filename = stateset_filename;
    location->state_name = state_name;

    fus_lexer_cleanup(lexer);
    free(text);
    return err;
}

