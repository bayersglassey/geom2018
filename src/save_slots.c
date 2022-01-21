

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

#include "save_slots.h"
#include "hexgame.h"
#include "hexmap.h"
#include "write.h"
#include "file_utils.h"
#include "lexer.h"
#include "lexer_macros.h"
#include "var_utils.h"
#include "hexgame_vars_props.h"


const char *save_slot_filenames[SAVE_SLOTS] = {
    "saves/0.txt",
    "saves/1.txt",
    "saves/2.txt"
};


#ifdef __EMSCRIPTEN__
void emccdemo_syncfs();
#endif


const char *get_save_slot_filename(int i){
    if(i < 0 || i >= SAVE_SLOTS)return NULL;
    return save_slot_filenames[i];
}

bool get_save_slot_file_exists(int i){
    const char *filename = get_save_slot_filename(i);
    FILE *file = fopen(filename, "r");
    if(!file)return false;
    fclose(file);
    return true;
}

int delete_save_slot(int i){
    const char *filename = get_save_slot_filename(i);
    fprintf(stderr, "Removing save file: %s\n", filename);
    if(remove(filename)){
        perror("remove");
        return 1;
    }
    return 0;
}



static void _write_vars(vars_t *vars, FILE *file, int depth){
    /* Don't write the vars which say they shouldn't be saved. */
    var_props_t nowrite_props_mask = 1 << HEXGAME_VARS_PROP_NOSAVE;
    vars_write_with_mask(vars, file, depth * 4, nowrite_props_mask);
}
int hexgame_save(hexgame_t *game, const char *filename){
    int err;

    FILE *file = fopen(filename, "w");
    if(file == NULL){
        fprintf(stderr, "Couldn't save game to %s: ", filename);
        perror(NULL);
        return 2;
    }

    /* Save format version */
    fprintf(file, "version: %i\n", SAVE_FORMAT_VERSION);

    /* Game vars */
    fprintf(file, "game:\n");
    fprintf(file, "    vars:\n");
    _write_vars(&game->vars, file, 2);

    /* Players */
    fprintf(file, "players:\n");
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];

        fprintf(file, "    :\n");
        fprintf(file, "        keymap: %i\n", player->keymap);

        fputs("        spawn: ", file);
        hexgame_savelocation_write(&player->respawn_location, file);
        fputc('\n', file);

        body_t *body = player->body;
        if(!body)continue;

        fprintf(file, "        body:\n");
        fputs("            vars:\n", file);
        _write_vars(&body->vars, file, 4);
    }

    /* Hexmaps */
    fprintf(file, "maps:\n");
    for(int i = 0; i < game->maps_len; i++){
        hexmap_t *map = game->maps[i];
        fprintf(file, "    ");
        fus_write_str(file, map->filename);
        fprintf(file, ":\n");
        fprintf(file, "        vars:\n");
        _write_vars(&map->vars, file, 3);
        fprintf(file, "        submaps:\n");
        for(int j = 0; j < map->submap_groups_len; j++){
            hexmap_submap_group_t *group = map->submap_groups[j];

            /* FOR NOW, since group->visited is the only thing we save,
            just skip unvisited groups */
            if(!group->visited)continue;

            fprintf(file, "            ");
            fus_write_str(file, group->name);
            fprintf(file, ":\n");
            fprintf(file, "                visited: %c\n",
                group->visited? 't': 'n');
        }
    }

    fclose(file);

#   ifdef __EMSCRIPTEN__
    /* Needed to actually save the IDBFS data to browser's indexedDb. */
    emccdemo_syncfs();
#   endif

    return 0;
}


int hexgame_load(hexgame_t *game, const char *filename,
    bool *bad_version_ptr
){
    int err;

    char *text = load_file(filename);
    if(text == NULL){
        fprintf(stderr, "Couldn't load game from %s\n", filename);
        return 2;
    }

    fus_lexer_t _lexer, *lexer=&_lexer;
    err = fus_lexer_init(lexer, text, filename);
    if(err)return err;

    prismelrenderer_t *prend = game->prend;

    int version = -1;
    if(GOT("version")){
        NEXT
        OPEN
        GET_INT(version)
        CLOSE
    }
    if(version != SAVE_FORMAT_VERSION){
        fprintf(stderr, "Old save format version: %i (expected: %i)\n",
            version, SAVE_FORMAT_VERSION);
        /* Indicate to caller that the reason for returning a failure code
        is a bad save format version. */
        *bad_version_ptr = true;
        return 2;
    }

    GET("game")
    OPEN
    {
        GET("vars")
        OPEN
        err = vars_parse(&game->vars, lexer);
        if(err)return err;
        CLOSE
    }
    CLOSE

    GET("players")
    OPEN
    while(!GOT(")")){
        OPEN

        GET("keymap")
        OPEN
        int keymap;
        GET_INT(keymap)
        CLOSE

        GET("spawn")
        OPEN
        hexgame_savelocation_t spawn_location;
        err = hexgame_savelocation_parse(&spawn_location, lexer,
            &prend->filename_store, &prend->name_store);
        if(err)return err;
        CLOSE

        vars_t body_vars;
        vars_init_with_props(&body_vars, hexgame_vars_prop_names);

        bool got_body = false;
        if(GOT("body")){
            got_body = true;
            NEXT
            OPEN
            {
                GET("vars")
                OPEN
                err = vars_parse(&body_vars, lexer);
                if(err)return err;
                CLOSE
            }
            CLOSE
        }

        ARRAY_PUSH_NEW(player_t*, game->players, player)
        err = player_init(player, game, keymap, &spawn_location);
        if(err)return err;

        if(got_body){
            err = player_spawn_body(player);
            if(err)return err;

            err = vars_copy(&player->body->vars, &body_vars);
            if(err)return err;
        }

        vars_cleanup(&body_vars);
        CLOSE
    }
    CLOSE

    GET("maps")
    OPEN
    while(!GOT(")")){
        const char *name;
        GET_STR_CACHED(name, &prend->name_store)

        hexmap_t *map;
        err = hexgame_get_or_load_map(game, name, &map);
        if(err)return err;

        OPEN
        {
            GET("vars")
            OPEN
            err = vars_parse(&map->vars, lexer);
            if(err)return err;
            CLOSE

            GET("submaps")
            OPEN
            while(!GOT(")")){
                const char *name;
                GET_STR_CACHED(name, &prend->name_store)

                hexmap_submap_group_t *group = hexmap_get_submap_group(
                    map, name);
                if(!group){
                    fprintf(stderr, "WARNING: "
                        "couldn't find submap group \"%s\" in map \"%s\"\n",
                        name, map->filename);
                    fprintf(stderr,
                        "...maybe this save file is from an old version?\n");
                    PARSE_SILENT
                    continue;
                }

                OPEN
                {
                    GET("visited")
                    OPEN
                    GET_BOOL(group->visited)
                    CLOSE
                }
                CLOSE
            }
            CLOSE
        }
        CLOSE
    }
    CLOSE

    fus_lexer_cleanup(lexer);
    free(text);
    return 0;
}
