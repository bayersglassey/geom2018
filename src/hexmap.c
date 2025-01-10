

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
#include "geom_lexer_utils.h"


static int _hexmap_load(hexmap_t *map, vars_t *vars);


/**********
 * HEXMAP *
 **********/

void body_cleanup(struct body *body);
void hexmap_cleanup(hexmap_t *map){
    ARRAY_FREE_PTR(body_t*, map->bodies, body_cleanup)

    ARRAY_FREE_PTR(hexmap_submap_t*, map->submaps, hexmap_submap_cleanup)
    ARRAY_FREE_PTR(hexmap_submap_group_t*, map->submap_groups,
        hexmap_submap_group_cleanup)
    ARRAY_FREE_PTR(hexmap_recording_t*, map->recordings,
        hexmap_recording_cleanup)
    ARRAY_FREE_PTR(hexmap_location_t*, map->locations,
        hexmap_location_cleanup)

    vars_cleanup(&map->song_vars);
    vars_cleanup(&map->vars);
}

int hexmap_init(hexmap_t *map, hexgame_t *game, const char *filename){
    int err;

    prismelrenderer_t *prend = game->prend;
    vecspace_t *space = &hexspace;

    map->loaded = false;
    map->filename = filename;
    map->game = game;
    map->space = space;
    hexgame_location_zero(&map->spawn);

    map->prend = prend;
    vec_zero(map->unit);

    vars_init_with_props(&map->vars, hexgame_vars_prop_names);

    map->song = NULL;
    vars_init(&map->song_vars);

    map->default_palette = NULL;
    map->default_tileset = NULL;
    map->default_mapper = NULL;

    ARRAY_INIT(map->bodies)
    ARRAY_INIT(map->submaps)
    ARRAY_INIT(map->submap_groups)
    ARRAY_INIT(map->recordings)
    ARRAY_INIT(map->locations)
    return 0;
}

int hexmap_reload(hexmap_t *map){
    int err;

    err = _hexmap_load(map, NULL);
    if(err)return err;

    return 0;
}

hexgame_location_t *hexmap_get_location(hexmap_t *map, const char *name){
    for(int i = 0; i < map->locations_len; i++){
        hexmap_location_t *location = map->locations[i];
        if(!strcmp(location->name, name))return &location->loc;
    }
    return NULL;
}

hexmap_submap_group_t *hexmap_get_submap_group(hexmap_t *map,
    const char *name
){
    for(int i = 0; i < map->submap_groups_len; i++){
        hexmap_submap_group_t *group = map->submap_groups[i];
        if(!strcmp(group->name, name))return group;
    }
    return NULL;
}

int hexmap_add_submap_group(hexmap_t *map, const char *name,
    hexmap_submap_group_t **group_ptr
){
    ARRAY_PUSH_NEW(hexmap_submap_group_t*, map->submap_groups, new_group)
    hexmap_submap_group_init(new_group, name);
    *group_ptr = new_group;
    return 0;
}

int hexmap_get_or_add_submap_group(hexmap_t *map, const char *name,
    hexmap_submap_group_t **group_ptr
){
    hexmap_submap_group_t *group = hexmap_get_submap_group(map, name);
    if(!group)return hexmap_add_submap_group(map, name, group_ptr);
    *group_ptr = group;
    return 0;
}

static int hexmap_parse_recording(fus_lexer_t *lexer, prismelrenderer_t *prend,
    const char **filename_ptr, const char **palmapper_name_ptr, int *frame_offset_ptr,
    trf_t *trf, vars_t *vars, vars_t *bodyvars, bool *relative_ptr,
    valexpr_t *visible_expr
){
    INIT

    valexpr_set_literal_bool(visible_expr, true);

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
    return 0;
}

static int hexmap_load_hexmap_recording(
    hexmap_t *map, hexmap_submap_t *submap,
    hexmap_recording_t *recording
){
    /* NOTE: submap may be NULL! */
    int err;
    vecspace_t *space = map->space;
    hexgame_t *game = map->game;

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

        /* We uhhhh, don't do anything with recording->bodyvars,
        interestingly enough. */
        err = vars_copy(&body->vars, &recording->vars);
        if(err)return err;
        err = body_execute_procs(body, STATESET_PROC_TYPE_ONSTATESETCHANGE);
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
        err = actor_init(actor, map, body, recording->filename, NULL, &trf);
        if(err)return err;

        err = valexpr_copy(&body->visible_expr, &recording->visible_expr);
        if(err)return err;

        err = vars_copy(&body->vars, &recording->bodyvars);
        if(err)return err;
        /*
        We don't actually do this here, because body->stateset is NULL!..
        We basically expect the actor's initial state to "play" a recording
        which will give the body its stateset.
        TODO: FIX THIS GODAWFUL HACK T__T
         err = body_execute_procs(body, STATESET_PROC_TYPE_ONSTATESETCHANGE);
         if(err)return err;
        */

        err = vars_copy(&actor->vars, &recording->vars);
        if(err)return err;
    }else{
        fprintf(stderr, "%s: Unrecognized hexmap recording type: %i\n",
            __func__, recording->type);
        return 2;
    }

    return 0;
}

static int _hexmap_load(hexmap_t *map, vars_t *vars){
    int err;
    fus_lexer_t lexer;

    fprintf(stderr, "Loading map: %s\n", map->filename);

    char *text = load_file(map->filename);
    if(text == NULL)return 1;

    err = fus_lexer_init_with_vars(&lexer, text, map->filename, vars);
    if(err)return err;

    err = hexmap_parse(map, &lexer);
    if(err)return err;

    fus_lexer_cleanup(&lexer);

    free(text);
    return 0;
}

int hexmap_load(hexmap_t *map, vars_t *vars){
    int err;

    err = _hexmap_load(map, vars);
    if(err)return err;

    /* Load recordings & actors */
    for(int i = 0; i < map->recordings_len; i++){
        hexmap_recording_t *recording = map->recordings[i];
        err = hexmap_load_hexmap_recording(map, NULL, recording);
        if(err)return err;
    }
    for(int j = 0; j < map->submaps_len; j++){
        hexmap_submap_t *submap = map->submaps[j];
        for(int i = 0; i < submap->collmap.recordings_len; i++){
            hexmap_recording_t *recording = submap->collmap.recordings[i];
            err = hexmap_load_hexmap_recording(map, submap, recording);
            if(err)return err;
        }
    }

    map->loaded = true;

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
        if(map->loaded){
            /* If we're reloading the map, skip parsing this */
            PARSE_SILENT
            NEXT
        }else{
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
                err = hexmap_parse_recording(lexer, prend,
                    &filename, &palmapper_name, &frame_offset, &trf,
                    &vars, &bodyvars, &relative, &visible_expr);
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
            }
            NEXT
        }
    }

    /* parse globalvars */
    if(GOT("globalvars")){
        NEXT
        OPEN
        if(map->loaded){
            /* If we're reloading the map, skip parsing this */
            PARSE_SILENT
            NEXT
        }else{
            err = vars_parse(&map->game->vars, lexer);
            if(err)return err;
            CLOSE
        }
    }

    /* parse mapvars */
    if(GOT("vars")){
        NEXT
        OPEN
        if(map->loaded){
            /* If we're reloading the map, skip parsing this */
            PARSE_SILENT
            NEXT
        }else{
            err = vars_parse(&map->vars, lexer);
            if(err)return err;
            CLOSE
        }
    }

    /* parse submaps */
    if(GOT("submaps")){
        NEXT
        OPEN
        while(1){
            if(GOT(")"))break;

            hexmap_submap_group_t *group = context->submap_group;
            if(GOT_STR){
                const char *group_name;
                GET_STR_CACHED(group_name, &prend->name_store)
                err = hexmap_get_or_add_submap_group(map, group_name, &group);
                if(err)return err;
            }

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

                err = hexmap_parse_submap(map, &sublexer, context, group);
                if(err){
                    fus_lexer_err_info(lexer);
                    fprintf(stderr, "...while importing here.\n");
                    return err;
                }

                if(!fus_lexer_done(&sublexer)){
                    err = fus_lexer_unexpected(&sublexer, "end of file");
                    fus_lexer_err_info(lexer);
                    fprintf(stderr, "...while importing here.\n");
                    return err;
                }

                /* We now call fus_lexer_next manually, see call to _fus_lexer_get_str
                above */
                err = fus_lexer_next(lexer);
                if(err)return err;

                fus_lexer_cleanup(&sublexer);
                free(filename);
                free(text);
            }else{
                err = hexmap_parse_submap(map, lexer, context, group);
                if(err)return err;
            }

            CLOSE
        }
        NEXT
    }

    return 0;
}

int hexmap_parse(hexmap_t *map, fus_lexer_t *lexer){
    INIT

    prismelrenderer_t *prend = map->game->prend;

    GET("name")
    OPEN
    GET_STR_CACHED(map->name, &prend->name_store)
    CLOSE

    /* parse unit */
    vec_t unit;
    GET("unit")
    OPEN
    for(int i = 0; i < prend->space->dims; i++){
        GET_INT(map->unit[i])
    }
    CLOSE

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

    /* The "root" submap group (to which submaps belong by default) */
    {
        hexmap_submap_group_t *root_group;
        err = hexmap_get_or_add_submap_group(map, "", &root_group);
        if(err)return err;
        context.submap_group = root_group;
    }

    /* default palette */
    {
        const char *palette_filename;
        GET("palette")
        OPEN
        GET_STR_CACHED(palette_filename, &prend->filename_store)
        err = prismelrenderer_get_or_create_palette(prend, palette_filename,
            &map->default_palette);
        if(err)return err;
        context.palette = map->default_palette;
        CLOSE
    }

    /* default tileset */
    {
        const char *tileset_filename;
        GET("tileset")
        OPEN
        GET_STR_CACHED(tileset_filename, &prend->filename_store)
        err = prismelrenderer_get_or_create_tileset(prend, tileset_filename,
            &map->default_tileset);
        if(err)return err;
        context.tileset = map->default_tileset;
        CLOSE
    }

    /* default mapper */
    if(GOT("mapper")){
        NEXT
        OPEN
        map->default_mapper = NULL;
        err = fus_lexer_get_mapper(lexer, map->prend, NULL,
            &map->default_mapper);
        if(err)return err;
        context.mapper = map->default_mapper;
        CLOSE
    }

    /* default song */
    if(GOT("song")){
        const char *song_name;
        NEXT
        OPEN
        GET_STR_CACHED(song_name, &prend->name_store)
        hexgame_audio_callback_t *song = hexgame_songs_get(song_name);
        if(!song){
            fprintf(stderr, "Couldn't find song: %s\n",
                song_name);
            return 2;
        }
        map->song = context.song = song;
        CLOSE
    }

    /* default song vars */
    if(GOT("song_vars")){
        NEXT
        OPEN
        err = vars_parse(&map->song_vars, lexer);
        if(err)return err;
        err = vars_copy(&context.song_vars, &map->song_vars);
        if(err)return err;
        CLOSE
    }

    /* Parse actors, vars, submaps... */
    err = _hexmap_parse(map, lexer, &context);
    if(err)return err;

    if(!DONE){
        return UNEXPECTED("end of file");
    }

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

int hexmap_parse_submap(hexmap_t *map, fus_lexer_t *lexer,
    hexmap_submap_parser_context_t *parent_context,
    hexmap_submap_group_t *group
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

    context->submap_group = group;

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
    }else if(GOT("visible")){
        NEXT
        OPEN
        err = valexpr_parse(&context->visible_expr, lexer);
        if(err)return err;
        CLOSE
    }

    if(GOT("target")){
        NEXT
        OPEN
        if(group == NULL){
            fprintf(stderr, "No submap group\n");
            return 2;
        }
        valexpr_cleanup(&group->target_expr);
        err = valexpr_parse(&group->target_expr, lexer);
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
        context->mapper = NULL;
        err = fus_lexer_get_mapper(lexer, map->prend, NULL, &context->mapper);
        if(err)return err;
        CLOSE
    }

    if(GOT("song")){
        const char *song_name;
        NEXT
        OPEN
        GET_STR_CACHED(song_name, &prend->name_store)
        hexgame_audio_callback_t *song = hexgame_songs_get(song_name);
        if(!song){
            fprintf(stderr, "Couldn't find song: %s\n",
                song_name);
            return 2;
        }
        context->song = song;
        CLOSE
    }

    if(GOT("song_vars")){
        NEXT
        OPEN
        err = vars_parse(&context->song_vars, lexer);
        if(err)return err;
        CLOSE
    }

    if(GOT("palette")){
        NEXT
        OPEN
        const char *palette_filename;
        GET_STR_CACHED(palette_filename, &prend->filename_store)
        err = prismelrenderer_get_or_create_palette(prend, palette_filename,
            &context->palette);
        if(err)return err;
        CLOSE
    }

    if(GOT("tileset")){
        NEXT
        OPEN
        const char *tileset_filename;
        GET_STR_CACHED(tileset_filename, &prend->filename_store)
        err = prismelrenderer_get_or_create_tileset(prend, tileset_filename,
            &context->tileset);
        if(err)return err;
        CLOSE
    }

    if(submap_filename != NULL){
        fprintf(stderr, "Loading submap: %s\n", submap_filename);

        /* TODO: if map->loaded, try to get existing submap & modify it... */
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
            err = hexmap_parse_recording(lexer, prend,
                &filename, &palmapper_name, &frame_offset, &trf,
                &vars, &bodyvars, &relative, &visible_expr);
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

int hexmap_load_recording(hexmap_t *map, const char *filename,
    palettemapper_t *palmapper, bool loop, int offset, trf_t *trf,
    body_t **body_ptr
){
    int err;

    hexgame_t *game = map->game;

    ARRAY_PUSH_NEW(body_t*, map->bodies, body)
    err = body_init(body, game, map, NULL, NULL, palmapper);
    if(err)return err;

    err = body_load_recording(body, filename, loop);
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
        /* NOTE: we iterate over submaps in reverse order, so that tunnels,
        i.e. elements where tile_c == 't', will work correctly, given the
        order in which submaps are rendered over one another. */
        for(int i = map->submaps_len - 1; i >= 0; i--){
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
                else if(elem->tile_c == 'w')collision_elem = &collision->water;
                else if(elem->tile_c == 't'){
                    /* We found a "tunnel" -- some non-colliding stuff which
                    overrides anything solid which might be (in a submap)
                    underneath it!..
                    So, we break out of iterating over submaps. */
                    break;
                }

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
            /* Just looking for savepoints, water, etc... */
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
            savepoints, water, etc */
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

void hexmap_submap_group_cleanup(hexmap_submap_group_t *group){
    valexpr_cleanup(&group->target_expr);
}

void hexmap_submap_group_init(hexmap_submap_group_t *group,
    const char *name
){
    group->name = name;
    group->visited = false;
    valexpr_set_literal_bool(&group->target_expr, false);
}

void hexmap_submap_cleanup(hexmap_submap_t *submap){
    ARRAY_FREE_PTR(valexpr_t*, submap->text_exprs, valexpr_cleanup)
    valexpr_cleanup(&submap->visible_expr);
    hexcollmap_cleanup(&submap->collmap);
    vars_cleanup(&submap->song_vars);
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

    submap->song = context->song;
    vars_init(&submap->song_vars);
    for(hexmap_submap_parser_context_t *cur_context = context;
        cur_context;
        cur_context=cur_context->parent
    ){
        err = vars_copy(&submap->song_vars, &cur_context->song_vars);
        if(err)return err;
    }

    ARRAY_INIT(submap->text_exprs)
    for(int i = 0; i < context->text_exprs_len; i++){
        valexpr_t *text_expr = context->text_exprs[i];
        ARRAY_PUSH_NEW(valexpr_t*, submap->text_exprs, new_text_expr)
        err = valexpr_copy(new_text_expr, text_expr);
        if(err)return err;
    }

    submap->rgraph_map = NULL;
    submap->rgraph_minimap = NULL;

    submap->group = context->submap_group;
    submap->mapper = context->mapper;
    submap->palette = context->palette;
    submap->tileset = context->tileset;

    return 0;
}

void hexmap_submap_visit(hexmap_submap_t *submap){
    if(submap->group)submap->group->visited = true;
}

bool hexmap_submap_is_visited(hexmap_submap_t *submap){
    if(!submap->group)return true;
    return submap->group->visited;
}

bool hexmap_submap_is_target(hexmap_submap_t *submap){
    if(!submap->group)return false;
    return submap->group->target;
}

int hexmap_submap_is_visible(hexmap_submap_t *submap, bool *visible_ptr){
    int err;

    /* NOTE: visible by default. */
    bool visible = true;
    {
        valexpr_result_t result = {0};
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
        }else if(!result.val){
            /* Val not found: use default visible value */
        }else{
            visible = val_get_bool(result.val);
        }
    }

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

    context->song = parent? parent->song: NULL;
    vars_init(&context->song_vars);

    /* Visibility is *NOT* inherited by default.
    Opt in with the "inherit_visible" syntax. */
    valexpr_set_literal_bool(&context->visible_expr, true);

    ARRAY_INIT(context->text_exprs)

    context->parent = parent;
    context->submap_group = parent? parent->submap_group: NULL;
    context->mapper = parent? parent->mapper: NULL;
    context->palette = parent? parent->palette: NULL;
    context->tileset = parent? parent->tileset: NULL;
}

void hexmap_submap_parser_context_cleanup(hexmap_submap_parser_context_t *context){
    valexpr_cleanup(&context->visible_expr);
    vars_cleanup(&context->song_vars);
    ARRAY_FREE_PTR(valexpr_t*, context->text_exprs, valexpr_cleanup)
}
