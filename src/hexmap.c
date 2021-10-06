

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "hexmap.h"
#include "hexgame.h"
#include "lexer.h"
#include "vars.h"
#include "var_utils.h"
#include "valexpr.h"
#include "util.h"
#include "mathutil.h"
#include "geom.h"
#include "vec4.h"
#include "hexspace.h"
#include "hexgame_location.h"
#include "prismelrenderer.h"


const char *hexmap_door_type_msg(int door_type){
    switch(door_type){
        case HEXMAP_DOOR_TYPE_DUD: return "Dud";
        case HEXMAP_DOOR_TYPE_RESPAWN: return "Respawn";
        case HEXMAP_DOOR_TYPE_NEW_GAME: return "New Game";
        case HEXMAP_DOOR_TYPE_CONTINUE: return "Continue";
        case HEXMAP_DOOR_TYPE_PLAYERS: return "Players";
        case HEXMAP_DOOR_TYPE_EXIT: return "Exit";
        case HEXMAP_DOOR_TYPE_CAMERA_MAPPER: return "Camera Mapper";
        default: return "Unknown";
    }
}


/******************
 * HEXMAP TILESET *
 ******************/

void hexmap_tileset_cleanup(hexmap_tileset_t *tileset){
    free(tileset->name);
    ARRAY_FREE_PTR(hexmap_tileset_entry_t*, tileset->vert_entries, (void))
    ARRAY_FREE_PTR(hexmap_tileset_entry_t*, tileset->edge_entries, (void))
    ARRAY_FREE_PTR(hexmap_tileset_entry_t*, tileset->face_entries, (void))
}

int hexmap_tileset_init(hexmap_tileset_t *tileset, char *name){
    tileset->name = name;
    ARRAY_INIT(tileset->vert_entries)
    ARRAY_INIT(tileset->edge_entries)
    ARRAY_INIT(tileset->face_entries)
    return 0;
}

static const char VERTS[] = "verts";
static const char EDGES[] = "edges";
static const char FACES[] = "faces";
static const char *WHEN_FACES_SOLID_RGRAPH_KEYS_VERT[] = {
    "none", "some", "all", NULL};
static const char *WHEN_FACES_SOLID_RGRAPH_KEYS_EDGE[] = {
    "neither", "bottom", "top", "both", NULL};
static int _hexmap_tileset_parse_part(
    const char *part_name /* one of VERTS, EDGES, or FACES */,
    hexmap_tileset_entry_t ***entries_ptr,
    int *entries_len_ptr, int *entries_size_ptr,
    prismelrenderer_t *prend, fus_lexer_t *lexer
){
    /* WELCOME TO THE FUNCTION FROM HELL
    AREN'T WE GLAD WE ROLLED OUR OWN DATA FORMAT? */
    int err;

    /* The following hack is an indication that our array.h system
    of doing things was itself a hack.
    That is, we will use ARRAY_PUSH_NEW below, passing it the token
    "entries", and it expects the tokens "entries_len" and "entries_size"
    to also exist.
    So we create them here and populate them from pointers, then assign
    their values back through the pointers at end of function.
    It's weird, man... but it works! */
    hexmap_tileset_entry_t **entries = *entries_ptr;
    int entries_len = *entries_len_ptr;
    int entries_size = *entries_size_ptr;

    err = fus_lexer_get(lexer, part_name);
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    while(1){
        if(fus_lexer_got(lexer, ")"))break;

        char tile_c;
        err = fus_lexer_get_chr(lexer, &tile_c);
        if(err)return err;

        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        ARRAY_PUSH_NEW(hexmap_tileset_entry_t*, entries, entry)
        entry->type = HEXMAP_TILESET_ENTRY_TYPE_ROTS;
        entry->n_rgraphs = 0;
        entry->tile_c = tile_c;
        entry->frame_offset = 0;

        if(fus_lexer_got(lexer, "frame_offset")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_int(lexer, &entry->frame_offset);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }

        const char **rgraph_keys = NULL;
        if(fus_lexer_got(lexer, "when_faces_solid")){
            err = fus_lexer_next(lexer);
            if(err)return err;

            if(part_name == VERTS){
                rgraph_keys = WHEN_FACES_SOLID_RGRAPH_KEYS_VERT;
            }else if(part_name == EDGES){
                rgraph_keys = WHEN_FACES_SOLID_RGRAPH_KEYS_EDGE;
            }else{
                fus_lexer_err_info(lexer);
                fprintf(stderr, "when_faces_solid only makes sense for "
                    "verts and edges!\n");
                return 2;
            }

            entry->type = HEXMAP_TILESET_ENTRY_TYPE_WHEN_FACES_SOLID;

            err = fus_lexer_get(lexer, "(");
            if(err)return err;
        }

        while(1){
            if(rgraph_keys){
                if(!fus_lexer_got_name(lexer))break;
                const char *rgraph_key = rgraph_keys[entry->n_rgraphs];
                if(rgraph_key == NULL){
                    fus_lexer_err_info(lexer);
                    fprintf(stderr,
                        "Too many entries for when_faces_solid.\n");
                    return 2;
                }
                err = fus_lexer_get(lexer, rgraph_key);
                if(err)return err;
                err = fus_lexer_get(lexer, "(");
                if(err)return err;
            }else{
                if(!fus_lexer_got_str(lexer))break;
            }
            if(entry->n_rgraphs == HEXMAP_TILESET_ENTRY_RGRAPHS) {
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Already parsed max rgraphs (%i)!\n",
                    HEXMAP_TILESET_ENTRY_RGRAPHS);
                return 2;
            }
            char *rgraph_name;
            err = fus_lexer_get_str(lexer, &rgraph_name);
            if(err)return err;
            rendergraph_t *rgraph =
                prismelrenderer_get_rendergraph(prend, rgraph_name);
            if(rgraph == NULL){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Couldn't find shape: %s\n",
                    rgraph_name);
                free(rgraph_name); return 2;}
            free(rgraph_name);
            entry->rgraphs[entry->n_rgraphs] = rgraph;
            entry->n_rgraphs++;
            if(rgraph_keys){
                err = fus_lexer_get(lexer, ")");
                if(err)return err;
            }
        }

        if(entry->n_rgraphs == 0){
            return fus_lexer_unexpected(lexer, "str");
        }
        if(rgraph_keys){
            const char *rgraph_key = rgraph_keys[entry->n_rgraphs];
            if(rgraph_key != NULL){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Missing an entry for when_faces_solid. "
                    "Expected entries:");
                for(int i = 0; rgraph_keys[i]; i++){
                    fprintf(stderr, " %s", rgraph_keys[i]);
                }
                fputc('\n', stderr);
                return 2;
            }
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }
    err = fus_lexer_next(lexer);
    if(err)return err;

    *entries_ptr = entries;
    *entries_len_ptr = entries_len;
    *entries_size_ptr = entries_size;
    return 0;
}

static int hexmap_tileset_parse(hexmap_tileset_t *tileset,
    prismelrenderer_t *prend, char *name,
    fus_lexer_t *lexer
){
    int err;

    err = hexmap_tileset_init(tileset, name);
    if(err)return err;

    err = _hexmap_tileset_parse_part(VERTS,
        &tileset->vert_entries,
        &tileset->vert_entries_len,
        &tileset->vert_entries_size,
        prend, lexer);
    if(err)return err;
    err = _hexmap_tileset_parse_part(EDGES,
        &tileset->edge_entries,
        &tileset->edge_entries_len,
        &tileset->edge_entries_size,
        prend, lexer);
    if(err)return err;
    err = _hexmap_tileset_parse_part(FACES,
        &tileset->face_entries,
        &tileset->face_entries_len,
        &tileset->face_entries_size,
        prend, lexer);
    if(err)return err;

    return 0;
}

int hexmap_tileset_load(hexmap_tileset_t *tileset,
    prismelrenderer_t *prend, const char *filename, vars_t *vars
){
    int err;
    fus_lexer_t lexer;

    fprintf(stderr, "Loading tileset: %s\n", filename);

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init_with_vars(&lexer, text, filename, vars);
    if(err)return err;

    err = hexmap_tileset_parse(tileset, prend, strdup(filename),
        &lexer);
    if(err)return err;

    fus_lexer_cleanup(&lexer);

    free(text);
    return 0;
}


/**********
 * HEXMAP *
 **********/

void body_cleanup(struct body *body);
void hexmap_cleanup(hexmap_t *map){
    free(map->name);

    ARRAY_FREE_PTR(body_t*, map->bodies, body_cleanup)

    ARRAY_FREE_PTR(hexmap_submap_t*, map->submaps, hexmap_submap_cleanup)
    ARRAY_FREE_PTR(hexmap_recording_t*, map->recordings,
        hexmap_recording_cleanup)
    ARRAY_FREE_PTR(palette_t*, map->palettes, palette_cleanup)
    ARRAY_FREE_PTR(hexmap_tileset_t*, map->tilesets,
        hexmap_tileset_cleanup)

    vars_cleanup(&map->vars);
}

int hexmap_init(hexmap_t *map, hexgame_t *game, char *name,
    vec_t unit
){
    int err;

    prismelrenderer_t *prend = game->prend;
    vecspace_t *space = &hexspace;

    map->name = name;
    map->game = game;
    map->space = space;
    hexgame_location_zero(&map->spawn);

    map->prend = prend;
    vec_cpy(prend->space->dims, map->unit, unit);

    static const char *prop_names[HEXMAP_VARS_PROPS + 1] =
        {HEXMAP_VARS_PROP_NAMES, NULL};
    vars_init_with_props(&map->vars, prop_names);

    ARRAY_INIT(map->bodies)
    ARRAY_INIT(map->submaps)
    ARRAY_INIT(map->recordings)
    ARRAY_INIT(map->palettes)
    ARRAY_INIT(map->tilesets)
    return 0;
}

static int hexmap_parse_recording(fus_lexer_t *lexer,
    char **filename_ptr, char **palmapper_name_ptr, int *frame_offset_ptr,
    trf_t *trf, vars_t *vars, vars_t *bodyvars, bool *relative_ptr,
    valexpr_t *visible_expr, bool *visible_not_ptr
){
    int err;

    valexpr_set_literal_bool(visible_expr, true);
    bool visible_not = false;

    err = fus_lexer_get(lexer, "(");
    if(err)return err;

    bool relative = false;
    if(fus_lexer_got(lexer, "relative")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        relative = true;
    }
    *relative_ptr = relative;

    /* NOTE: we are initializing a trf_t, but the fus format is that of a
    hexgame_location_t, so that we can e.g. dump player's current location
    and use it directly in maps. */
    hexgame_location_t loc = {0};
    if(fus_lexer_got(lexer, "(")){
        err = hexgame_location_parse(&loc, lexer);
        if(err)return err;
    }
    hexgame_location_init_trf(&loc, trf);

    err = fus_lexer_get_str(lexer, filename_ptr);
    if(err)return err;
    if(
        !fus_lexer_got(lexer, ")") &&
        !fus_lexer_got(lexer, "visible") &&
        !fus_lexer_got(lexer, "vars") &&
        !fus_lexer_got(lexer, "bodyvars")
    ){
        if(fus_lexer_got(lexer, "empty")){
            err = fus_lexer_next(lexer);
            if(err)return err;
        }else{
            err = fus_lexer_get_str(lexer, palmapper_name_ptr);
            if(err)return err;
        }
        if(fus_lexer_got_int(lexer)){
            err = fus_lexer_get_int(lexer, frame_offset_ptr);
            if(err)return err;
        }
    }

    if(fus_lexer_got(lexer, "visible")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        if(fus_lexer_got(lexer, "not")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            visible_not = true;
        }
        err = valexpr_parse(visible_expr, lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "vars")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = vars_parse(vars, lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "bodyvars")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = vars_parse(bodyvars, lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    *visible_not_ptr = visible_not;
    return 0;
}

static int hexmap_load_hexmap_recording(
    hexmap_t *map, hexmap_submap_t *submap, hexgame_t *game,
    hexmap_recording_t *recording
){
    /* NOTE: submap may be NULL! */
    int err;
    vecspace_t *space = map->space;

    /* Get palmapper */
    palettemapper_t *palmapper = NULL;
    if(recording->palmapper_name != NULL){
        palmapper = prismelrenderer_get_palmapper(
            game->prend, recording->palmapper_name);
        if(palmapper == NULL){
            fprintf(stderr, "Couldn't find palmapper: %s\n",
                recording->palmapper_name);
            return 2;
        }
    }

    trf_t trf = {0};
    trf_cpy(space, &trf, &recording->trf);
    if(submap)vec_add(space->dims, trf.add, submap->pos);

    if(recording->type == HEXMAP_RECORDING_TYPE_RECORDING){
        body_t *body;
        err = hexmap_load_recording(map, recording->filename,
            palmapper, true, recording->frame_offset, &trf, &body);
        if(err)return err;

        err = valexpr_copy(&body->visible_expr, &recording->visible_expr);
        if(err)return err;
        body->visible_not = recording->visible_not;

        /* We uhhhh, don't do anything with recording->bodyvars,
        interestingly enough. */
        err = vars_copy(&body->vars, &recording->vars);
        if(err)return err;
    }else if(recording->type == HEXMAP_RECORDING_TYPE_ACTOR){
        /* We create a body with NULL stateset, state.
        Is that even allowed?..
        Currently I think all actors immediately "play", so it's never an
        issue, but... */
        ARRAY_PUSH_NEW(body_t*, map->bodies, body)
        err = body_init(body, game, map, NULL, NULL, palmapper);
        if(err)return err;

        ARRAY_PUSH_NEW(actor_t*, game->actors, actor)
        err = actor_init(actor, map, body, recording->filename, NULL);
        if(err)return err;

        actor->trf = trf;

        err = valexpr_copy(&body->visible_expr, &recording->visible_expr);
        if(err)return err;
        body->visible_not = recording->visible_not;

        err = vars_copy(&body->vars, &recording->bodyvars);
        if(err)return err;
        err = vars_copy(&actor->vars, &recording->vars);
        if(err)return err;
    }else{
        fprintf(stderr, "%s: Unrecognized hexmap recording type: %i\n",
            __func__, recording->type);
        return 2;
    }

    return 0;
}

int hexmap_load(hexmap_t *map, hexgame_t *game, const char *filename,
    vars_t *vars
){
    int err;
    fus_lexer_t lexer;

    fprintf(stderr, "Loading map: %s\n", filename);

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init_with_vars(&lexer, text, filename, vars);
    if(err)return err;

    err = hexmap_parse(map, game, strdup(filename), &lexer);
    if(err)return err;

    /* Load recordings & actors */
    for(int i = 0; i < map->recordings_len; i++){
        hexmap_recording_t *recording = map->recordings[i];
        err = hexmap_load_hexmap_recording(map, NULL, game, recording);
        if(err)return err;
    }
    for(int j = 0; j < map->submaps_len; j++){
        hexmap_submap_t *submap = map->submaps[j];
        for(int i = 0; i < submap->collmap.recordings_len; i++){
            hexmap_recording_t *recording = submap->collmap.recordings[i];
            err = hexmap_load_hexmap_recording(map, submap, game, recording);
            if(err)return err;
        }
    }

    fus_lexer_cleanup(&lexer);

    free(text);
    return 0;
}

int hexmap_parse(hexmap_t *map, hexgame_t *game, char *name,
    fus_lexer_t *lexer
){
    int err;

    prismelrenderer_t *prend = game->prend;

    /* parse unit */
    vec_t unit;
    err = fus_lexer_get(lexer, "unit");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    for(int i = 0; i < prend->space->dims; i++){
        err = fus_lexer_get_int(lexer, &unit[i]);
        if(err)return err;
    }
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    /* init the map */
    err = hexmap_init(map, game, name, unit);
    if(err)return err;

    /* parse spawn point */
    char *spawn_filename = NULL;
    err = fus_lexer_get(lexer, "spawn");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    if(fus_lexer_got_str(lexer)){
        err = fus_lexer_get_str(lexer, &spawn_filename);
        if(err)return err;
    }else{
        err = fus_lexer_get_vec(lexer, map->space, map->spawn.pos);
        if(err)return err;
    }
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    /* default palette */
    palette_t *default_palette;
    {
        char *palette_filename;
        err = fus_lexer_get(lexer, "default_palette");
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_str(lexer, &palette_filename);
        if(err)return err;
        err = hexmap_get_or_create_palette(map, palette_filename,
            &default_palette);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    /* default tileset */
    hexmap_tileset_t *default_tileset;
    {
        char *tileset_filename;
        err = fus_lexer_get(lexer, "default_tileset");
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_str(lexer, &tileset_filename);
        if(err)return err;
        err = hexmap_get_or_create_tileset(map, tileset_filename,
            &default_tileset);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    /* parse actors */
    if(fus_lexer_got(lexer, "actors")){
    err = fus_lexer_next(lexer);
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    while(1){
        if(fus_lexer_got(lexer, ")"))break;

        char *filename;
        char *palmapper_name = NULL;
        int frame_offset = 0;
        trf_t trf;
        vars_t vars;
        vars_init(&vars);
        vars_t bodyvars;
        vars_init(&bodyvars);
        bool relative; /* unused here -- only used when parsing submaps */
        valexpr_t visible_expr;
        bool visible_not;
        err = hexmap_parse_recording(lexer,
            &filename, &palmapper_name, &frame_offset, &trf,
            &vars, &bodyvars, &relative, &visible_expr, &visible_not);
        if(err)return err;

        ARRAY_PUSH_NEW(hexmap_recording_t*, map->recordings,
            recording)
        err = hexmap_recording_init(recording,
            HEXMAP_RECORDING_TYPE_ACTOR,
            filename, palmapper_name, frame_offset);
        if(err)return err;

        recording->trf = trf;
        recording->vars = vars;
        recording->bodyvars = bodyvars;
        recording->visible_expr = visible_expr;
        recording->visible_not = visible_not;
    }
    err = fus_lexer_next(lexer);
    if(err)return err;
    }


    /* parse vars */
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


    /* parse submaps */
    err = fus_lexer_get(lexer, "submaps");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    {
        hexmap_submap_parser_context_t context;
        err = hexmap_submap_parser_context_init(&context, NULL);
        if(err)return err;

        context.palette = default_palette;
        context.tileset = default_tileset;
        while(1){
            if(fus_lexer_got(lexer, ")"))break;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = hexmap_parse_submap(map, lexer, &context);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }
        hexmap_submap_parser_context_cleanup(&context);
    }
    err = fus_lexer_next(lexer);
    if(err)return err;


    /* maybe get spawn point from a submap */
    if(spawn_filename != NULL){
        hexmap_submap_t *spawn_submap = NULL;
        for(int i = 0; i < map->submaps_len; i++){
            hexmap_submap_t *submap = map->submaps[i];
            if(strcmp(submap->filename, spawn_filename) == 0){
                spawn_submap = submap; break;}
        }
        if(spawn_submap == NULL){
            fprintf(stderr, "Couldn't find submap with filename: %s\n",
                spawn_filename);
            return 2;}
        map->spawn = spawn_submap->collmap.spawn;
        vec_add(map->space->dims, map->spawn.pos, spawn_submap->pos);
        free(spawn_filename);
    }


    /* pheeew */
    return 0;
}

static int hexmap_parse_door(hexmap_t *map, hexmap_submap_t *submap,
    hexmap_door_t *door, fus_lexer_t *lexer
){
    /* We assume door's memory starts off zero'd, e.g. it was calloc'd
    by ARRAY_PUSH_NEW */
    int err;

    vecspace_t *space = map->space;

    if(fus_lexer_got(lexer, "dud")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        door->type = HEXMAP_DOOR_TYPE_DUD;
    }else if(fus_lexer_got(lexer, "new_game")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        door->type = HEXMAP_DOOR_TYPE_NEW_GAME;
        err = fus_lexer_get_str(lexer, &door->u.location.map_filename);
        if(err)return err;
    }else if(fus_lexer_got(lexer, "continue")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        door->type = HEXMAP_DOOR_TYPE_CONTINUE;
    }else if(fus_lexer_got(lexer, "players")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        door->type = HEXMAP_DOOR_TYPE_PLAYERS;
        err = fus_lexer_get_int(lexer, &door->u.n_players);
        if(err)return err;
    }else if(fus_lexer_got(lexer, "exit")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        door->type = HEXMAP_DOOR_TYPE_EXIT;
    }else if(fus_lexer_got(lexer, "camera_mapper")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        char *s;
        {
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_str(lexer, &s);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }
        door->type = HEXMAP_DOOR_TYPE_CAMERA_MAPPER;
        door->u.s = s;
    }else{
        door->type = HEXMAP_DOOR_TYPE_RESPAWN;

        if(fus_lexer_got(lexer, "map")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_str(lexer, &door->u.location.map_filename);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }else{
            /* Non-null door->u.location.map_filename indicates player should
            "teleport" to door->u.location */
            door->u.location.map_filename = strdup(map->name);
        }

        if(fus_lexer_got(lexer, "anim")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_str(lexer, &door->u.location.stateset_filename);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }

        err = fus_lexer_get(lexer, "pos");
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_vec(lexer, space, door->u.location.loc.pos);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;

        err = fus_lexer_get(lexer, "rot");
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_int(lexer, &door->u.location.loc.rot);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;

        err = fus_lexer_get(lexer, "turn");
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_yn(lexer, &door->u.location.loc.turn);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    return 0;
}

static int hexmap_populate_submap_doors(hexmap_t *map,
    hexmap_submap_t *submap
){
    /* Scan submap's collmap for door tiles, and link the corresponding
    doors to them.
    (The "corresponding" door is simply the next one found, as we iterate
    through the tiles from top-left to bottom-right.) */
    int err;

    int n_doors = 0;
    hexcollmap_t *collmap = &submap->collmap;
    int w = collmap->w;
    int h = collmap->h;
    for(int y = 0; y < h; y++){
        for(int x = 0; x < w; x++){
            hexcollmap_tile_t *tile = &collmap->tiles[y * w + x];
            for(int i = 0; i < 2; i++){
                hexcollmap_elem_t *elem = &tile->face[i];
                if(elem->tile_c != 'D')continue;
                if(n_doors < submap->doors_len){
                    hexmap_door_t *door = submap->doors[n_doors];
                    door->elem = elem;
                }
                n_doors++;
            }
        }
    }

    if(n_doors != submap->doors_len){
        fprintf(stderr,
            "Map (%s) and collmap (%s) disagree on number of doors: "
            "%i != %i\n",
            map->name, submap->filename,
            submap->doors_len, n_doors);
        return 2;
    }
    return 0;
}

int hexmap_parse_submap(hexmap_t *map, fus_lexer_t *lexer,
    hexmap_submap_parser_context_t *parent_context
){
    int err;
    vecspace_t *space = map->space;
    prismelrenderer_t *prend = map->prend;

    if(fus_lexer_got(lexer, "skip")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_parse_silent(lexer);
        if(err)return err;
        return 0;
    }

    hexmap_submap_parser_context_t _context, *context=&_context;
    err = hexmap_submap_parser_context_init(context, parent_context);
    if(err)return err;

    if(fus_lexer_got(lexer, "bg")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        context->solid = false;
    }

    /* We own this string, and free it before returning.
    If we pass it to other functions, they either copy it themselves, or we
    pass them a copy. */
    char *submap_filename = NULL;
    if(fus_lexer_got(lexer, "file")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_str(lexer, &submap_filename);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    /* You can inherit parent's texts, and/or define your own. */
    if(fus_lexer_got(lexer, "inherit_text")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        for(int i = 0; i < parent_context->text_exprs_len; i++){
            valexpr_t *text_expr = parent_context->text_exprs[i];
            ARRAY_PUSH_NEW(valexpr_t*, context->text_exprs, new_text_expr)
            err = valexpr_copy(new_text_expr, text_expr);
            if(err)return err;
        }
    }
    while(fus_lexer_got(lexer, "text")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;

        ARRAY_PUSH_NEW(valexpr_t*, context->text_exprs, text_expr)
        err = valexpr_parse(text_expr, lexer);
        if(err)return err;

        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    /* You can *either* inherit parent context's visibility, or define your
    own.
    (NOTE: context->visible_expr is initialized to a literal bool, which doesn't
    need to be cleaned up, so we can freely overwrite it, *once*.) */
    if(fus_lexer_got(lexer, "inherit_visible")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = valexpr_copy(&context->visible_expr,
            &parent_context->visible_expr);
        if(err)return err;
        context->visible_not = parent_context->visible_not;
    }else if(fus_lexer_got(lexer, "visible")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        if(fus_lexer_got(lexer, "not")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            context->visible_not = true;
        }

        err = valexpr_parse(&context->visible_expr, lexer);
        if(err)return err;

        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "pos")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;

        vec_t pos;
        err = fus_lexer_get_vec(lexer, space, pos);
        if(err)return err;
        vec_add(space->dims, context->pos, pos);

        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "rot")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;

        rot_t rot = 0;
        err = fus_lexer_get_int(lexer, &rot);
        if(err)return err;
        context->rot = rot_contain(space->rot_max, context->rot + rot);

        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "camera")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        if(fus_lexer_got(lexer, "follow")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            context->camera_type = CAMERA_TYPE_FOLLOW;
        }else{
            vec_t camera_pos;
            err = fus_lexer_get_vec(lexer, space, camera_pos);
            if(err)return err;

            /* Camera pos becomes submap's pos, plus whatever we parsed */
            vec_cpy(space->dims, context->camera_pos, context->pos);
            vec_add(space->dims, context->camera_pos, camera_pos);
        }
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "mapper")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_mapper(lexer, map->prend, NULL, &context->mapper);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "palette")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;

        char *palette_filename;
        err = fus_lexer_get_str(lexer, &palette_filename);
        if(err)return err;
        err = hexmap_get_or_create_palette(map, palette_filename,
            &context->palette);
        if(err)return err;
        free(palette_filename);

        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "tileset")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;

        char *tileset_filename;
        err = fus_lexer_get_str(lexer, &tileset_filename);
        if(err)return err;
        err = hexmap_get_or_create_tileset(map, tileset_filename,
            &context->tileset);
        if(err)return err;
        free(tileset_filename);

        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(submap_filename != NULL){
        fprintf(stderr, "Loading submap: %s\n", submap_filename);

        ARRAY_PUSH_NEW(hexmap_submap_t*, map->submaps, submap)

        char *submap_filename_copy = strdup(submap_filename);
        if(!submap_filename_copy)return 1;

        /* WARNING: submap's init does *NOT* init its collmap, so need
        to load submap->collmap immediately after this call. */
        err = hexmap_submap_init_from_parser_context(map, submap,
            submap_filename_copy, context);
        if(err)return err;

        /* Load submap->collmap */
        if(context->rot){
            /* load & transform collmap */

            hexcollmap_t *collmap = calloc(1, sizeof(*collmap));
            if(!collmap)return 1;
            err = hexcollmap_load(collmap, map->space,
                submap_filename, lexer->vars);
            if(err)return err;

            char *collmap_filename = strdup(submap_filename);
            if(!collmap_filename)return 1;

            hexcollmap_init_clone(&submap->collmap, collmap, collmap_filename);
            int err = hexcollmap_clone(&submap->collmap, collmap, context->rot);
            if(err)return err;

            hexcollmap_cleanup(collmap);
            free(collmap);
        }else{
            /* load collmap */
            err = hexcollmap_load(&submap->collmap, map->space,
                submap_filename, lexer->vars);
            if(err)return err;
        }

        /* render submap->rgraph_map, submap->rgraph_minimap */
        err = hexmap_submap_create_rgraph_map(submap);
        if(err)return err;
        err = hexmap_submap_create_rgraph_minimap(submap);
        if(err)return err;

        /* parse doors */
        if(fus_lexer_got(lexer, "doors")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;

            while(1){
                if(fus_lexer_got(lexer, ")"))break;

                err = fus_lexer_get(lexer, "(");
                if(err)return err;

                ARRAY_PUSH_NEW(hexmap_door_t*, submap->doors, door)
                err = hexmap_parse_door(map, submap, door, lexer);
                if(err)return err;

                err = fus_lexer_get(lexer, ")");
                if(err)return err;
            }
            err = fus_lexer_next(lexer);
            if(err)return err;

            err = hexmap_populate_submap_doors(map, submap);
            if(err)return err;
        }
    }

    if(fus_lexer_got(lexer, "recs")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        while(1){
            if(fus_lexer_got(lexer, ")"))break;

            char *filename;
            char *palmapper_name = NULL;
            int frame_offset = 0;
            trf_t trf;
            vars_t vars;
            vars_init(&vars);
            vars_t bodyvars;
            vars_init(&bodyvars);
            bool relative;
            valexpr_t visible_expr;
            bool visible_not;
            err = hexmap_parse_recording(lexer,
                &filename, &palmapper_name, &frame_offset, &trf,
                &vars, &bodyvars, &relative, &visible_expr, &visible_not);
            if(err)return err;

            if(relative){
                trf_rot(space, &trf, context->rot);
                vec_add(space->dims, trf.add, context->pos);
            }

            ARRAY_PUSH_NEW(hexmap_recording_t*, map->recordings,
                recording)
            err = hexmap_recording_init(recording,
                HEXMAP_RECORDING_TYPE_RECORDING,
                filename, palmapper_name, frame_offset);
            if(err)return err;

            recording->trf = trf;
            recording->vars = vars;
            recording->bodyvars = bodyvars;
            recording->visible_expr = visible_expr;
            recording->visible_not = visible_not;
        }
        err = fus_lexer_next(lexer);
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "submaps")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        while(1){
            if(fus_lexer_got(lexer, ")"))break;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = hexmap_parse_submap(map, lexer, context);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }
        err = fus_lexer_next(lexer);
        if(err)return err;
    }

    free(submap_filename);
    hexmap_submap_parser_context_cleanup(context);
    return 0;
}

int hexmap_get_submap_index(hexmap_t *map, hexmap_submap_t *submap){
    for(int i = 0; i < map->submaps_len; i++){
        if(map->submaps[i] == submap)return i;
    }
    return -1;
}

static palette_t *hexmap_get_palette(hexmap_t *map, const char *name){
    for(int i = 0; i < map->palettes_len; i++){
        palette_t *palette = map->palettes[i];
        if(!strcmp(palette->name, name))return palette;
    }
    return NULL;
}

int hexmap_get_or_create_palette(hexmap_t *map, const char *name,
    palette_t **palette_ptr
){
    int err;
    palette_t *palette = hexmap_get_palette(map, name);
    if(palette == NULL){
        ARRAY_PUSH_NEW(palette_t*, map->palettes, _palette);
        palette = _palette;

        err = palette_load(palette, name, NULL);
        if(err)return err;
    }
    *palette_ptr = palette;
    return 0;
}

static hexmap_tileset_t *hexmap_get_tileset(hexmap_t *map, const char *name){
    for(int i = 0; i < map->tilesets_len; i++){
        hexmap_tileset_t *tileset = map->tilesets[i];
        if(!strcmp(tileset->name, name))return tileset;
    }
    return NULL;
}

int hexmap_get_or_create_tileset(hexmap_t *map, const char *name,
    hexmap_tileset_t **tileset_ptr
){
    int err;
    hexmap_tileset_t *tileset = hexmap_get_tileset(map, name);
    if(tileset == NULL){
        ARRAY_PUSH_NEW(hexmap_tileset_t*, map->tilesets, _tileset);
        tileset = _tileset;

        err = hexmap_tileset_load(tileset, map->prend, name, NULL);
        if(err)return err;
    }
    *tileset_ptr = tileset;
    return 0;
}

int hexmap_load_recording(hexmap_t *map, const char *filename,
    palettemapper_t *palmapper, bool loop, int offset, trf_t *trf,
    body_t **body_ptr
){
    int err;

    hexgame_t *game = map->game;

    ARRAY_PUSH_NEW(body_t*, map->bodies, body)
    err = body_init(body, game, map, NULL, NULL, palmapper);
    if(err)return err;

    err = recording_load(&body->recording, filename, NULL, body, loop);
    if(err)return err;

    body->recording.offset += offset;

    if(trf){
        hexgame_location_apply(&body->recording.loc0, trf);
    }

    err = body_play_recording(body);
    if(err)return err;

    if(body_ptr)*body_ptr = body;
    return 0;
}

static int hexmap_collide_elem(hexmap_t *map, int all_type,
    int x, int y, trf_t *trf,
    hexcollmap_elem_t *elems2, int rot,
    void (*normalize_elem)(trf_t *index),
    hexcollmap_elem_t *(get_elem)(
        hexcollmap_t *collmap, trf_t *index),
    hexmap_collision_t *collision,

    /* *collide_ptr: true (1) or false (0), or 2 if caller should continue
    checking for a collision. */
    int *collide_ptr
){
    int err;

    vecspace_t *space = map->space;

    for(int r2 = 0; r2 < rot; r2++){
        if(!hexcollmap_elem_is_solid(&elems2[r2]))continue;

        trf_t index;
        hexspace_set(index.add, x, y);
        index.rot = r2;
        index.flip = false;

        /* And now, because we were fools and defined */
        /* the tile coords such that their Y is flipped */
        /* compared to vecspaces, we need to flip that Y */
        /* before calling trf_apply and then flip it back */
        /* again: */
        index.add[1] = -index.add[1];
        trf_apply(space, &index, trf);
        index.add[1] = -index.add[1];
        normalize_elem(&index);

        bool collide = false;
        for(int i = 0; i < map->submaps_len; i++){
            hexmap_submap_t *submap = map->submaps[i];

            bool solid;
            err = hexmap_submap_is_solid(submap, &solid);
            if(err)return err;
            if(!solid)continue;

            hexcollmap_t *collmap1 = &submap->collmap;

            trf_t subindex;
            hexspace_set(subindex.add,
                index.add[0] - submap->pos[0],
                index.add[1] + submap->pos[1]);
            subindex.rot = index.rot;
            subindex.flip = index.flip;

            hexcollmap_elem_t *elem = get_elem(collmap1, &subindex);
            if(elem != NULL){
                hexmap_collision_elem_t *collision_elem = NULL;

                if(elem->tile_c == 'S')collision_elem = &collision->savepoint;
                else if(elem->tile_c == 'D')collision_elem = &collision->door;
                else if(elem->tile_c == 'w')collision_elem = &collision->water;

                if(collision_elem){
                    collision_elem->submap = submap;
                    collision_elem->elem = elem;
                }
            }
            if(hexcollmap_elem_is_solid(elem)){
                collide = true; break;}
        }
        if(all_type != 2){
            bool all = all_type;
            if((all && !collide) || (!all && collide)){
                *collide_ptr = collide;
                return 0;
            }
        }else{
            /* Just looking for savepoints & doors... */
        }
    }

    *collide_ptr = 2; /* Caller should keep looking for a collision */
    return 0;
}

static void hexmap_collision_elem_init(hexmap_collision_elem_t *elem){
    elem->submap = NULL;
    elem->elem = NULL;
}

static void hexmap_collision_init(hexmap_collision_t *collision){
    hexmap_collision_elem_init(&collision->savepoint);
    hexmap_collision_elem_init(&collision->water);
    hexmap_collision_elem_init(&collision->door);
}

static int _hexmap_collide(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, int all_type, hexmap_collision_t *collision, bool *collide_ptr
){
#   define _RETURN(_collide) {*collide_ptr = _collide; return 0;}
    int err;

    hexmap_collision_init(collision);

    int ox2 = collmap2->ox;
    int oy2 = collmap2->oy;
    int w2 = collmap2->w;
    int h2 = collmap2->h;

    vecspace_t *space = map->space;

    /* NOTE: for tile coords (ox, oy, x, y, w, h),
    Y is reversed (down is positive, up is negative) */

    for(int y2 = 0; y2 < h2; y2++){
        for(int x2 = 0; x2 < w2; x2++){
            hexcollmap_tile_t *tile2 = &collmap2->tiles[y2 * w2 + x2];

            /* collide: true (1) or false (0), or 2 if we should continue
            checking for a collision. */
            int collide;

            int x = x2 - ox2;
            int y = y2 - oy2;
            err = hexmap_collide_elem(map, all_type,
                x, y, trf,
                tile2->vert, 1,
                hexcollmap_normalize_vert,
                hexcollmap_get_vert,
                collision, &collide);
            if(err)return err;
            if(collide != 2)_RETURN(collide)
            err = hexmap_collide_elem(map, all_type,
                x, y, trf,
                tile2->edge, 3,
                hexcollmap_normalize_edge,
                hexcollmap_get_edge,
                collision, &collide);
            if(err)return err;
            if(collide != 2)_RETURN(collide)
            err = hexmap_collide_elem(map, all_type,
                x, y, trf,
                tile2->face, 2,
                hexcollmap_normalize_face,
                hexcollmap_get_face,
                collision, &collide);
            if(err)return err;
            if(collide != 2)_RETURN(collide)
        }
    }

    {
        bool collide;
        if(all_type == 2){
            /* Return value doesn't matter, we were just looking for
            savepoints & doors */
            collide = false;
        }else{
            bool all = all_type;
            if(all)collide = true;
            else collide = false;
        }

        *collide_ptr = collide;
        return 0;
    }
#   undef _RETURN
}

int hexmap_collide(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, bool all, bool *collide_ptr
){
    hexmap_collision_t collision;
    return _hexmap_collide(map, collmap2, trf, all, &collision, collide_ptr);
}

int hexmap_collide_special(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, hexmap_collision_t *collision
){
    int all_type = 2;
    bool collide; /* unused */
    return _hexmap_collide(map, collmap2, trf, all_type, collision, &collide);
}


int hexmap_step(hexmap_t *map){
    int err;

    hexgame_t *game = map->game;
    vecspace_t *space = game->space;

    /* Collide bodies with each other */
    for(int i = 0; i < map->bodies_len; i++){
        body_t *body = map->bodies[i];
        if(body->state == NULL)continue;
        hexcollmap_t *hitbox = body->state->hitbox;
        if(hitbox == NULL)continue;

        bool visible;
        err = body_is_visible(body, &visible);
        if(err)return err;
        if(!visible)continue;

        trf_t hitbox_trf;
        hexgame_location_init_trf(&body->loc, &hitbox_trf);

        /* This body has a hitbox! So collide it against all other bodies'
        hitboxes. */
        for(int j = i + 1; j < map->bodies_len; j++){
            body_t *body_other = map->bodies[j];
            if(body_other->state == NULL)continue;
            hexcollmap_t *hitbox_other = body_other->state->hitbox;
            if(hitbox_other == NULL)continue;

            bool visible_other;
            err = body_is_visible(body_other, &visible_other);
            if(err)return err;
            if(!visible_other)continue;

            trf_t hitbox_other_trf;
            hexgame_location_init_trf(&body_other->loc, &hitbox_other_trf);

            /* The other body has a hitbox! Do the collision... */
            bool collide = hexcollmap_collide(hitbox, &hitbox_trf,
                hitbox_other, &hitbox_other_trf, space, false);
            if(collide){
                /* There was a collision!
                Now we find out who was right... and who was dead. */
                err = body_collide_against_body(body, body_other);
                if(err)return err;
                err = body_collide_against_body(body_other, body);
                if(err)return err;
            }
        }
    }

    /* Do 1 gameplay step for each body */
    for(int i = 0; i < map->bodies_len; i++){
        body_t *body = map->bodies[i];

        bool visible;
        err = body_is_visible(body, &visible);
        if(err)return err;
        if(!visible)continue;

        err = body_step(body, game);
        if(err)return err;
    }

    return 0;
}

int hexmap_refresh_vars(hexmap_t *map){
    int err;
    vars_t *vars = &map->vars;

    /* At the moment, we don't set any special vars... */

    return 0;
}


/*****************
 * HEXMAP SUBMAP *
 *****************/

const char *submap_camera_type_msg(int camera_type){
    switch(camera_type){
        case CAMERA_TYPE_STATIC: return "Use camera_pos";
        case CAMERA_TYPE_FOLLOW: return "Follow player";
        default: return "Unkown";
    }
}

void hexmap_door_cleanup(hexmap_door_t *door){
    switch(door->type){
        case HEXMAP_DOOR_TYPE_RESPAWN:
        case HEXMAP_DOOR_TYPE_NEW_GAME:
            hexgame_savelocation_cleanup(&door->u.location);
            break;
        case HEXMAP_DOOR_TYPE_CAMERA_MAPPER:
            free(door->u.s);
            break;
        default:
            break;
    }
}

void hexmap_submap_cleanup(hexmap_submap_t *submap){
    free(submap->filename);
    ARRAY_FREE_PTR(valexpr_t*, submap->text_exprs, valexpr_cleanup)
    valexpr_cleanup(&submap->visible_expr);
    hexcollmap_cleanup(&submap->collmap);
    ARRAY_FREE_PTR(hexmap_door_t*, submap->doors, hexmap_door_cleanup)
}

int hexmap_submap_init_from_parser_context(hexmap_t *map,
    hexmap_submap_t *submap, char *filename,
    hexmap_submap_parser_context_t *context
){
    int err;

    /* WARNING: we do *NOT* init submap->collmap here, that is handled
    by hexcollmap_parse and friends, which should be called by our caller
    immediately after us */

    vecspace_t *space = map->space;

    submap->map = map;
    submap->filename = filename;

    submap->solid = context->solid;
    vec_cpy(space->dims, submap->pos, context->pos);
    vec_cpy(space->dims, submap->camera_pos, context->camera_pos);
    submap->camera_type = context->camera_type;

    err = valexpr_copy(&submap->visible_expr, &context->visible_expr);
    if(err)return err;
    submap->visible_not = context->visible_not;

    ARRAY_INIT(submap->text_exprs)
    for(int i = 0; i < context->text_exprs_len; i++){
        valexpr_t *text_expr = context->text_exprs[i];
        ARRAY_PUSH_NEW(valexpr_t*, submap->text_exprs, new_text_expr)
        err = valexpr_copy(new_text_expr, text_expr);
        if(err)return err;
    }

    submap->rgraph_map = NULL;
    submap->rgraph_minimap = NULL;

    submap->mapper = context->mapper;
    submap->palette = context->palette;
    submap->tileset = context->tileset;

    ARRAY_INIT(submap->doors)

    submap->visited = false;

    return 0;
}

int hexmap_submap_is_visible(hexmap_submap_t *submap, bool *visible_ptr){
    int err;

    /* NOTE: visible by default. */
    bool visible = true;
    val_t *result;
    {
        valexpr_context_t context = {
            .mapvars = &submap->map->vars,
            .globalvars = &submap->map->game->vars
        };
        err = valexpr_get(&submap->visible_expr, &context, &result);
        if(err){
            fprintf(stderr,
                "Error while evaluating visibility for submap: %s\n",
                submap->filename);
            return err;
        }else if(!result){
            /* Val not found: use default visible value */
        }else{
            visible = val_get_bool(result);
        }
    }

    if(submap->visible_not)visible = !visible;
    *visible_ptr = visible;
    return 0;
}

int hexmap_submap_is_solid(hexmap_submap_t *submap, bool *solid_ptr){
    int err;
    bool solid = true;
    if(!submap->solid){
        solid = false;
        goto done;
    }

    err = hexmap_submap_is_visible(submap, &solid);
    if(err)return err;

done:
    *solid_ptr = solid;
    return 0;
}

hexmap_door_t *hexmap_submap_get_door(hexmap_submap_t *submap,
    hexcollmap_elem_t *elem
){
    for(int i = 0; i < submap->doors_len; i++){
        hexmap_door_t *door = submap->doors[i];
        if(door->elem == elem)return door;
    }
    return NULL;
}


int hexmap_submap_parser_context_init(hexmap_submap_parser_context_t *context,
    hexmap_submap_parser_context_t *parent
){
    int err;

    /* Not inherited from parent (for backwards compat) */
    context->solid = true;

    context->rot = parent? parent->rot: 0;

    if(parent){
        vec_cpy(HEXSPACE_DIMS, context->pos, parent->pos);
        vec_cpy(HEXSPACE_DIMS, context->camera_pos, parent->camera_pos);
    }else{
        vec_zero(context->pos);
        vec_zero(context->camera_pos);
    }

    context->camera_type = parent? parent->camera_type: CAMERA_TYPE_STATIC;

    /* Visibility is *NOT* inherited by default.
    Opt in with the "inherit_visible" syntax. */
    valexpr_set_literal_bool(&context->visible_expr, true);
    context->visible_not = false;

    ARRAY_INIT(context->text_exprs)

    context->parent = parent;
    context->mapper = parent? parent->mapper: NULL;
    context->palette = parent? parent->palette: NULL;
    context->tileset = parent? parent->tileset: NULL;

    return 0;
}

void hexmap_submap_parser_context_cleanup(hexmap_submap_parser_context_t *context){
    valexpr_cleanup(&context->visible_expr);
    ARRAY_FREE_PTR(valexpr_t*, context->text_exprs, valexpr_cleanup)
}
