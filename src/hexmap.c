

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "hexmap.h"
#include "hexgame.h"
#include "lexer.h"
#include "lexer_macros.h"
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
#include "hexgame_vars_props.h"


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
    ARRAY_FREE_PTR(hexmap_tileset_entry_t*, tileset->vert_entries, (void))
    ARRAY_FREE_PTR(hexmap_tileset_entry_t*, tileset->edge_entries, (void))
    ARRAY_FREE_PTR(hexmap_tileset_entry_t*, tileset->face_entries, (void))
}

int hexmap_tileset_init(hexmap_tileset_t *tileset, const char *name){
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
    INIT

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

    GET(part_name)
    OPEN
    while(1){
        if(GOT(")"))break;

        char tile_c;
        GET_CHR(tile_c)

        OPEN
        ARRAY_PUSH_NEW(hexmap_tileset_entry_t*, entries, entry)
        entry->type = HEXMAP_TILESET_ENTRY_TYPE_ROTS;
        entry->n_rgraphs = 0;
        entry->tile_c = tile_c;
        entry->frame_offset = 0;

        if(GOT("frame_offset")){
            NEXT
            OPEN
            GET_INT(entry->frame_offset)
            CLOSE
        }

        const char **rgraph_keys = NULL;
        if(GOT("when_faces_solid")){
            NEXT
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
            OPEN
        }

        while(1){
            if(rgraph_keys){
                if(!GOT_NAME)break;
                const char *rgraph_key = rgraph_keys[entry->n_rgraphs];
                if(rgraph_key == NULL){
                    fus_lexer_err_info(lexer);
                    fprintf(stderr,
                        "Too many entries for when_faces_solid.\n");
                    return 2;
                }
                GET(rgraph_key)
                OPEN
            }else{
                if(!GOT_STR)break;
            }
            if(entry->n_rgraphs == HEXMAP_TILESET_ENTRY_RGRAPHS) {
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Already parsed max rgraphs (%i)!\n",
                    HEXMAP_TILESET_ENTRY_RGRAPHS);
                return 2;
            }
            const char *rgraph_name;
            GET_STR_CACHED(rgraph_name, &prend->name_store)
            rendergraph_t *rgraph =
                prismelrenderer_get_rendergraph(prend, rgraph_name);
            if(rgraph == NULL){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Couldn't find shape: %s\n",
                    rgraph_name);
                return 2;}
            entry->rgraphs[entry->n_rgraphs] = rgraph;
            entry->n_rgraphs++;
            if(rgraph_keys){
                CLOSE
            }
        }

        if(entry->n_rgraphs == 0){
            return UNEXPECTED("str");
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
            CLOSE
        }
        CLOSE
    }
    NEXT

    *entries_ptr = entries;
    *entries_len_ptr = entries_len;
    *entries_size_ptr = entries_size;
    return 0;
}

static int hexmap_tileset_parse(hexmap_tileset_t *tileset,
    prismelrenderer_t *prend, const char *name,
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

    err = hexmap_tileset_parse(tileset, prend, filename,
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
    ARRAY_FREE_PTR(body_t*, map->bodies, body_cleanup)

    ARRAY_FREE_PTR(hexmap_submap_t*, map->submaps, hexmap_submap_cleanup)
    ARRAY_FREE_PTR(hexmap_recording_t*, map->recordings,
        hexmap_recording_cleanup)
    ARRAY_FREE_PTR(palette_t*, map->palettes, palette_cleanup)
    ARRAY_FREE_PTR(hexmap_tileset_t*, map->tilesets,
        hexmap_tileset_cleanup)
    ARRAY_FREE_PTR(hexmap_location_t*, map->locations,
        hexmap_location_cleanup)

    vars_cleanup(&map->vars);
}

int hexmap_init(hexmap_t *map, hexgame_t *game, const char *name,
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

    vars_init_with_props(&map->vars, hexgame_vars_prop_names);

    ARRAY_INIT(map->bodies)
    ARRAY_INIT(map->submaps)
    ARRAY_INIT(map->recordings)
    ARRAY_INIT(map->palettes)
    ARRAY_INIT(map->tilesets)
    ARRAY_INIT(map->locations)
    return 0;
}

hexgame_location_t *hexmap_get_location(hexmap_t *map, const char *name){
    for(int i = 0; i < map->locations_len; i++){
        hexmap_location_t *location = map->locations[i];
        if(!strcmp(location->name, name))return &location->loc;
    }
    return NULL;
}

static int hexmap_parse_recording(fus_lexer_t *lexer, prismelrenderer_t *prend,
    const char **filename_ptr, const char **palmapper_name_ptr, int *frame_offset_ptr,
    trf_t *trf, vars_t *vars, vars_t *bodyvars, bool *relative_ptr,
    valexpr_t *visible_expr, bool *visible_not_ptr
){
    INIT

    valexpr_set_literal_bool(visible_expr, true);
    bool visible_not = false;

    OPEN

    bool relative = false;
    if(GOT("relative")){
        NEXT
        relative = true;
    }
    *relative_ptr = relative;

    /* NOTE: we are initializing a trf_t, but the fus format is that of a
    hexgame_location_t, so that we can e.g. dump player's current location
    and use it directly in maps. */
    hexgame_location_t loc = {0};
    if(GOT("(")){
        err = hexgame_location_parse(&loc, lexer);
        if(err)return err;
    }
    hexgame_location_init_trf(&loc, trf);

    GET_STR_CACHED(*filename_ptr, &prend->filename_store)
    if(
        !GOT(")") &&
        !GOT("visible") &&
        !GOT("vars") &&
        !GOT("bodyvars")
    ){
        if(GOT("empty")){
            NEXT
        }else{
            GET_STR_CACHED(*palmapper_name_ptr, &prend->name_store)
        }
        if(GOT_INT){
            GET_INT(*frame_offset_ptr)
        }
    }

    if(GOT("visible")){
        NEXT
        OPEN
        if(GOT("not")){
            NEXT
            visible_not = true;
        }
        err = valexpr_parse(visible_expr, lexer);
        if(err)return err;
        CLOSE
    }

    if(GOT("vars")){
        NEXT
        OPEN
        err = vars_parse(vars, lexer);
        if(err)return err;
        CLOSE
    }

    if(GOT("bodyvars")){
        NEXT
        OPEN
        err = vars_parse(bodyvars, lexer);
        if(err)return err;
        CLOSE
    }

    CLOSE

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

    err = hexmap_parse(map, game, filename, &lexer);
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

static int _hexmap_parse(hexmap_t *map, fus_lexer_t *lexer,
    hexmap_submap_parser_context_t *context
){
    INIT

    prismelrenderer_t *prend = map->prend;

    /* parse actors */
    if(GOT("actors")){
        NEXT
        OPEN
        while(1){
            if(GOT(")"))break;

            const char *filename;
            const char *palmapper_name = NULL;
            int frame_offset = 0;
            trf_t trf;
            vars_t vars;
            vars_init_with_props(&vars, hexgame_vars_prop_names);
            vars_t bodyvars;
            vars_init_with_props(&bodyvars, hexgame_vars_prop_names);
            bool relative; /* unused here -- only used when parsing submaps */
            valexpr_t visible_expr;
            bool visible_not;
            err = hexmap_parse_recording(lexer, prend,
                &filename, &palmapper_name, &frame_offset, &trf,
                &vars, &bodyvars, &relative, &visible_expr, &visible_not);
            if(err)return err;

            ARRAY_PUSH_NEW(hexmap_recording_t*, map->recordings,
                recording)
            hexmap_recording_init(recording,
                HEXMAP_RECORDING_TYPE_ACTOR,
                filename, palmapper_name, frame_offset);

            recording->trf = trf;
            recording->vars = vars;
            recording->bodyvars = bodyvars;
            recording->visible_expr = visible_expr;
            recording->visible_not = visible_not;
        }
        NEXT
    }

    /* parse vars */
    if(GOT("vars")){
        NEXT
        OPEN
        err = vars_parse(&map->vars, lexer);
        if(err)return err;
        CLOSE
    }

    /* parse submaps */
    if(GOT("submaps")){
        NEXT
        OPEN
        while(1){
            if(GOT(")"))break;
            OPEN

            if(GOT("import")){
                NEXT

                /* We use _fus_lexer_get_str to avoid calling fus_lexer_next until after
                the call to fus_lexer_init_with_vars is done, to make sure we don't modify
                lexer->vars first */
                char *filename;
                err = _fus_lexer_get_str(lexer, &filename);
                if(err)return err;

                char *text = load_file(filename);
                if(text == NULL)return 1;

                fus_lexer_t sublexer;
                err = fus_lexer_init_with_vars(&sublexer, text, filename,
                    lexer->vars);
                if(err)return err;

                err = hexmap_parse_submap(map, &sublexer, context);
                if(err)return err;

                if(!fus_lexer_done(&sublexer)){
                    return fus_lexer_unexpected(&sublexer, "end of file");
                }

                /* We now call fus_lexer_next manually, see call to _fus_lexer_get_str
                above */
                err = fus_lexer_next(lexer);
                if(err)return err;

                fus_lexer_cleanup(&sublexer);
                free(filename);
                free(text);
            }else{
                err = hexmap_parse_submap(map, lexer, context);
                if(err)return err;
            }

            CLOSE
        }
        NEXT
    }

    return 0;
}

int hexmap_parse(hexmap_t *map, hexgame_t *game, const char *name,
    fus_lexer_t *lexer
){
    INIT

    prismelrenderer_t *prend = game->prend;

    /* parse unit */
    vec_t unit;
    GET("unit")
    OPEN
    for(int i = 0; i < prend->space->dims; i++){
        GET_INT(unit[i])
    }
    CLOSE

    /* init the map */
    err = hexmap_init(map, game, name, unit);
    if(err)return err;

    /* parse spawn point */
    const char *spawn_filename = NULL;
    GET("spawn")
    OPEN
    if(fus_lexer_got_str(lexer)){
        GET_STR_CACHED(spawn_filename, &prend->filename_store)
    }else{
        GET_VEC(map->space, map->spawn.pos)
    }
    CLOSE

    hexmap_submap_parser_context_t context;
    hexmap_submap_parser_context_init(&context, NULL);

    /* default palette */
    {
        const char *palette_filename;
        GET("palette")
        OPEN
        GET_STR_CACHED(palette_filename, &prend->filename_store)
        err = hexmap_get_or_create_palette(map, palette_filename,
            &context.palette);
        if(err)return err;
        CLOSE
    }

    /* default tileset */
    {
        const char *tileset_filename;
        GET("tileset")
        OPEN
        GET_STR_CACHED(tileset_filename, &prend->filename_store)
        err = hexmap_get_or_create_tileset(map, tileset_filename,
            &context.tileset);
        if(err)return err;
        CLOSE
    }

    /* Parse actors, vars, submaps... */
    err = _hexmap_parse(map, lexer, &context);
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
    }

    /* pheeew */
    hexmap_submap_parser_context_cleanup(&context);
    return 0;
}

static int hexmap_parse_door(hexmap_t *map, hexmap_submap_t *submap,
    hexmap_door_t *door, fus_lexer_t *lexer, prismelrenderer_t *prend
){
    /* We assume door's memory starts off zero'd, e.g. it was calloc'd
    by ARRAY_PUSH_NEW */
    INIT

    vecspace_t *space = map->space;

    if(GOT("dud")){
        NEXT
        door->type = HEXMAP_DOOR_TYPE_DUD;
    }else if(GOT("new_game")){
        NEXT
        door->type = HEXMAP_DOOR_TYPE_NEW_GAME;
        GET_STR_CACHED(door->u.location.map_filename,
            &prend->filename_store)
    }else if(GOT("continue")){
        NEXT
        door->type = HEXMAP_DOOR_TYPE_CONTINUE;
    }else if(GOT("players")){
        NEXT
        door->type = HEXMAP_DOOR_TYPE_PLAYERS;
        GET_INT(door->u.n_players)
    }else if(GOT("exit")){
        NEXT
        door->type = HEXMAP_DOOR_TYPE_EXIT;
    }else if(GOT("camera_mapper")){
        NEXT
        door->type = HEXMAP_DOOR_TYPE_CAMERA_MAPPER;
        OPEN
        GET_STR_CACHED(door->u.mapper_name, &prend->name_store)
        CLOSE
    }else{
        door->type = HEXMAP_DOOR_TYPE_RESPAWN;

        if(GOT("map")){
            NEXT
            OPEN
            GET_STR_CACHED(door->u.location.map_filename,
                &prend->filename_store)
            CLOSE
        }else{
            /* Non-null door->u.location.map_filename indicates player should
            "teleport" to door->u.location */
            door->u.location.map_filename = map->name;
        }

        if(GOT("anim")){
            NEXT
            OPEN
            GET_STR_CACHED(door->u.location.stateset_filename,
                &prend->filename_store)
            CLOSE
        }

        GET("pos")
        OPEN
        GET_VEC(space, door->u.location.loc.pos)
        CLOSE

        GET("rot")
        OPEN
        GET_INT(door->u.location.loc.rot)
        CLOSE

        GET("turn")
        OPEN
        GET_YN(door->u.location.loc.turn)
        CLOSE
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

    if(GOT("skip")){
        NEXT
        PARSE_SILENT
        return 0;
    }

    hexmap_submap_parser_context_t _context, *context=&_context;
    hexmap_submap_parser_context_init(context, parent_context);

    if(GOT("bg")){
        NEXT
        context->solid = false;
    }

    const char *submap_filename = NULL;
    if(GOT("file")){
        NEXT
        OPEN
        GET_STR_CACHED(submap_filename, &prend->filename_store)
        CLOSE
    }

    /* You can inherit parent's texts, and/or define your own. */
    if(GOT("inherit_text")){
        NEXT
        for(int i = 0; i < parent_context->text_exprs_len; i++){
            valexpr_t *text_expr = parent_context->text_exprs[i];
            ARRAY_PUSH_NEW(valexpr_t*, context->text_exprs, new_text_expr)
            err = valexpr_copy(new_text_expr, text_expr);
            if(err)return err;
        }
    }
    while(GOT("text")){
        NEXT
        OPEN
        ARRAY_PUSH_NEW(valexpr_t*, context->text_exprs, text_expr)
        err = valexpr_parse(text_expr, lexer);
        if(err)return err;
        CLOSE
    }

    /* You can *either* inherit parent context's visibility, or define your
    own.
    (NOTE: context->visible_expr is initialized to a literal bool, which doesn't
    need to be cleaned up, so we can freely overwrite it, *once*.) */
    if(GOT("inherit_visible")){
        NEXT
        err = valexpr_copy(&context->visible_expr,
            &parent_context->visible_expr);
        if(err)return err;
        context->visible_not = parent_context->visible_not;
    }else if(GOT("visible")){
        NEXT
        OPEN
        if(GOT("not")){
            NEXT
            context->visible_not = true;
        }
        err = valexpr_parse(&context->visible_expr, lexer);
        if(err)return err;
        CLOSE
    }

    if(GOT("pos")){
        NEXT
        OPEN
        vec_t pos;
        GET_VEC(space, pos)
        vec_add(space->dims, context->pos, pos);
        CLOSE
    }

    if(GOT("inherit_rot")){
        NEXT
        context->rot = rot_contain(space->rot_max,
            context->rot + parent_context->rot);
    }
    if(GOT("rot")){
        NEXT
        OPEN
        rot_t rot = 0;
        GET_INT(rot)
        context->rot = rot_contain(space->rot_max, context->rot + rot);
        CLOSE
    }

    if(GOT("camera")){
        NEXT
        OPEN
        if(GOT("follow")){
            NEXT
            context->camera_type = CAMERA_TYPE_FOLLOW;
        }else{
            vec_t camera_pos;
            GET_VEC(space, camera_pos)

            /* Camera pos becomes submap's pos, plus whatever we parsed */
            vec_cpy(space->dims, context->camera_pos, context->pos);
            vec_add(space->dims, context->camera_pos, camera_pos);
        }
        CLOSE
    }

    if(GOT("mapper")){
        NEXT
        OPEN
        err = fus_lexer_get_mapper(lexer, map->prend, NULL, &context->mapper);
        if(err)return err;
        CLOSE
    }

    if(GOT("palette")){
        NEXT
        OPEN
        const char *palette_filename;
        GET_STR_CACHED(palette_filename, &prend->filename_store)
        err = hexmap_get_or_create_palette(map, palette_filename,
            &context->palette);
        if(err)return err;
        CLOSE
    }

    if(GOT("tileset")){
        NEXT
        OPEN
        const char *tileset_filename;
        GET_STR_CACHED(tileset_filename, &prend->filename_store)
        err = hexmap_get_or_create_tileset(map, tileset_filename,
            &context->tileset);
        if(err)return err;
        CLOSE
    }

    if(submap_filename != NULL){
        fprintf(stderr, "Loading submap: %s\n", submap_filename);

        ARRAY_PUSH_NEW(hexmap_submap_t*, map->submaps, submap)

        /* WARNING: submap's init does *NOT* init its collmap, so need
        to load submap->collmap immediately after this call. */
        err = hexmap_submap_init_from_parser_context(map, submap,
            submap_filename, context);
        if(err)return err;

        /* Load submap->collmap */
        if(context->rot){
            /* load & transform collmap */

            hexcollmap_t *collmap = calloc(1, sizeof(*collmap));
            if(!collmap)return 1;
            err = hexcollmap_load(collmap, map->space,
                submap_filename, lexer->vars,
                &prend->name_store, &prend->filename_store);
            if(err)return err;

            hexcollmap_init_clone(&submap->collmap, collmap, submap_filename);
            int err = hexcollmap_clone(&submap->collmap, collmap, context->rot);
            if(err)return err;

            hexcollmap_cleanup(collmap);
            free(collmap);
        }else{
            /* load collmap */
            err = hexcollmap_load(&submap->collmap, map->space,
                submap_filename, lexer->vars,
                &prend->name_store, &prend->filename_store);
            if(err)return err;
        }

        /* render submap->rgraph_map, submap->rgraph_minimap */
        err = hexmap_submap_create_rgraph_map(submap);
        if(err)return err;
        err = hexmap_submap_create_rgraph_minimap(submap);
        if(err)return err;

        /* copy submap->collmap's locations onto map */
        for(int i = 0; i < submap->collmap.locations_len; i++){
            hexmap_location_t *location = submap->collmap.locations[i];
            ARRAY_PUSH_NEW(hexmap_location_t*, map->locations, new_location)
            hexmap_location_init(new_location, location->name);
            new_location->loc = location->loc;
            vec_add(space->dims, new_location->loc.pos, submap->pos);
        }

        /* parse doors */
        if(GOT("doors")){
            NEXT
            OPEN

            while(1){
                if(GOT(")"))break;

                OPEN
                ARRAY_PUSH_NEW(hexmap_door_t*, submap->doors, door)
                err = hexmap_parse_door(map, submap, door, lexer, prend);
                if(err)return err;
                CLOSE
            }
            NEXT

            err = hexmap_populate_submap_doors(map, submap);
            if(err)return err;
        }
    }

    if(GOT("recs")){
        NEXT
        OPEN
        while(1){
            if(GOT(")"))break;

            const char *filename;
            const char *palmapper_name = NULL;
            int frame_offset = 0;
            trf_t trf;
            vars_t vars;
            vars_init_with_props(&vars, hexgame_vars_prop_names);
            vars_t bodyvars;
            vars_init_with_props(&bodyvars, hexgame_vars_prop_names);
            bool relative;
            valexpr_t visible_expr;
            bool visible_not;
            err = hexmap_parse_recording(lexer, prend,
                &filename, &palmapper_name, &frame_offset, &trf,
                &vars, &bodyvars, &relative, &visible_expr, &visible_not);
            if(err)return err;

            if(relative){
                trf_rot(space, &trf, context->rot);
                vec_add(space->dims, trf.add, context->pos);
            }

            ARRAY_PUSH_NEW(hexmap_recording_t*, map->recordings,
                recording)
            hexmap_recording_init(recording,
                HEXMAP_RECORDING_TYPE_RECORDING,
                filename, palmapper_name, frame_offset);

            recording->trf = trf;
            recording->vars = vars;
            recording->bodyvars = bodyvars;
            recording->visible_expr = visible_expr;
            recording->visible_not = visible_not;
        }
        NEXT
    }

    err = _hexmap_parse(map, lexer, context);
    if(err)return err;

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
        if(palette->name == name || !strcmp(palette->name, name)){
            return palette;
        }
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
        if(tileset->name == name || !strcmp(tileset->name, name)){
            return tileset;
        }
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

    /* Remove any bodies marked for removal */
    for(int i = 0; i < map->bodies_len; i++){
        body_t *body = map->bodies[i];
        if(body->remove){
            body_remove(body);
            i--;
        }
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
        default:
            break;
    }
}

void hexmap_submap_cleanup(hexmap_submap_t *submap){
    ARRAY_FREE_PTR(valexpr_t*, submap->text_exprs, valexpr_cleanup)
    valexpr_cleanup(&submap->visible_expr);
    hexcollmap_cleanup(&submap->collmap);
    ARRAY_FREE_PTR(hexmap_door_t*, submap->doors, hexmap_door_cleanup)
}

int hexmap_submap_init_from_parser_context(hexmap_t *map,
    hexmap_submap_t *submap, const char *filename,
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


void hexmap_submap_parser_context_init(hexmap_submap_parser_context_t *context,
    hexmap_submap_parser_context_t *parent
){
    /* Not inherited from parent (for backwards compat) */
    context->solid = true;

    /* Not inherited by parent (for backwards compat, and also because then
    it seems like parent's rot should affect child's pos?.. which would be
    weird somehow.) */
    context->rot = 0;

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
}

void hexmap_submap_parser_context_cleanup(hexmap_submap_parser_context_t *context){
    valexpr_cleanup(&context->visible_expr);
    ARRAY_FREE_PTR(valexpr_t*, context->text_exprs, valexpr_cleanup)
}
