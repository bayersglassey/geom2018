

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "hexmap.h"
#include "lexer.h"
#include "util.h"
#include "mathutil.h"
#include "geom.h"
#include "vec4.h"
#include "hexspace.h"
#include "prismelrenderer.h"



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

static int hexmap_tileset_parse(hexmap_tileset_t *tileset,
    prismelrenderer_t *prend, char *name,
    fus_lexer_t *lexer
){
    int err;

    err = hexmap_tileset_init(tileset, name);
    if(err)return err;

    /* parse vert, edge, face, rgraphs */
    #define GET_RGRAPH(TYPE) { \
        err = fus_lexer_get(lexer, #TYPE"s"); \
        if(err)return err; \
        err = fus_lexer_get(lexer, "("); \
        if(err)return err; \
        while(1){ \
            if(fus_lexer_got(lexer, ")"))break; \
            \
            char tile_c; \
            err = fus_lexer_get_chr(lexer, &tile_c); \
            if(err)return err; \
            \
            err = fus_lexer_get(lexer, "("); \
            if(err)return err; \
            err = fus_lexer_get_str(lexer, &name); \
            if(err)return err; \
            rendergraph_t *rgraph = \
                prismelrenderer_get_rendergraph(prend, name); \
            if(rgraph == NULL){ \
                fus_lexer_err_info(lexer); \
                fprintf(stderr, "Couldn't find shape: %s\n", name); \
                free(name); return 2;} \
            free(name); \
            ARRAY_PUSH_NEW(hexmap_tileset_entry_t*, \
                tileset->TYPE##_entries, entry) \
            entry->tile_c = tile_c; \
            entry->rgraph = rgraph; \
            err = fus_lexer_get(lexer, ")"); \
            if(err)return err; \
        } \
        err = fus_lexer_next(lexer); \
        if(err)return err; \
    }
    GET_RGRAPH(vert)
    GET_RGRAPH(edge)
    GET_RGRAPH(face)
    #undef GET_RGRAPH

    return 0;
}

int hexmap_tileset_load(hexmap_tileset_t *tileset,
    prismelrenderer_t *prend, const char *filename
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    err = hexmap_tileset_parse(tileset, prend, strdup(filename),
        &lexer);
    if(err)return err;

    free(text);
    return 0;
}

rendergraph_t *hexmap_tileset_get_rgraph_vert(hexmap_tileset_t *tileset,
    char tile_c
){
    for(int i = 0; i < tileset->vert_entries_len; i++){
        hexmap_tileset_entry_t *entry = tileset->vert_entries[i];
        if(entry->tile_c == tile_c)return entry->rgraph;
    }
    return NULL;
}

rendergraph_t *hexmap_tileset_get_rgraph_edge(hexmap_tileset_t *tileset,
    char tile_c
){
    for(int i = 0; i < tileset->edge_entries_len; i++){
        hexmap_tileset_entry_t *entry = tileset->edge_entries[i];
        if(entry->tile_c == tile_c)return entry->rgraph;
    }
    return NULL;
}

rendergraph_t *hexmap_tileset_get_rgraph_face(hexmap_tileset_t *tileset,
    char tile_c
){
    for(int i = 0; i < tileset->face_entries_len; i++){
        hexmap_tileset_entry_t *entry = tileset->face_entries[i];
        if(entry->tile_c == tile_c)return entry->rgraph;
    }
    return NULL;
}



/********************
 * HEXMAP RECORDING *
 ********************/

void hexmap_recording_cleanup(hexmap_recording_t *recording){
    free(recording->filename);
}

int hexmap_recording_init(hexmap_recording_t *recording, char *filename,
    palettemapper_t *palmapper
){
    recording->filename = filename;
    recording->palmapper = palmapper;
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
    ARRAY_FREE_PTR(hexmap_recording_t*, map->actor_recordings,
        hexmap_recording_cleanup)
}

int hexmap_init(hexmap_t *map, char *name, vecspace_t *space,
    prismelrenderer_t *prend,
    vec_t unit
){
    int err;

    map->name = name;
    map->space = space;
    map->prend = prend;
    vec_cpy(prend->space->dims, map->unit, unit);
    vec_zero(space->dims, map->spawn);

    ARRAY_INIT(map->bodies)

    ARRAY_INIT(map->submaps)
    ARRAY_INIT(map->recordings)
    ARRAY_INIT(map->actor_recordings)
    return 0;
}

int hexmap_load(hexmap_t *map, prismelrenderer_t *prend,
    const char *filename
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    err = hexmap_parse(map, prend, strdup(filename), &lexer);
    if(err)return err;

    free(text);
    return 0;
}

int hexmap_parse(hexmap_t *map, prismelrenderer_t *prend, char *name,
    fus_lexer_t *lexer
){
    int err;
    vecspace_t *space = &hexspace;
        /* hexmap's space is always hexspace. we will pass it
        to hexmap_init below, instead of just settings it here,
        which would probably make more sense.
        But ultimately we should really just move the call to
        hexmap_init out of hexmap_parse into hexmap_load. SO DO THAT */


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
    err = hexmap_init(map, name, space, prend, unit);
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
        err = fus_lexer_get_vec(lexer, space, map->spawn);
        if(err)return err;
    }
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    /* default palette */
    char *default_palette_filename;
    err = fus_lexer_get(lexer, "default_palette");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_get_str(lexer, &default_palette_filename);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    /* default tileset */
    char *default_tileset_filename;
    err = fus_lexer_get(lexer, "default_tileset");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_get_str(lexer, &default_tileset_filename);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;


    /* parse actors */
    if(fus_lexer_got(lexer, "actors")){
    err = fus_lexer_next(lexer);
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    while(1){
        if(fus_lexer_got(lexer, ")"))break;

        char *filename;
        palettemapper_t *palmapper = NULL;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_str(lexer, &filename);
        if(err)return err;
        if(!fus_lexer_got(lexer, ")")){
            char *palmapper_name;
            err = fus_lexer_get_str(lexer, &palmapper_name);
            if(err)return err;
            palmapper =
                prismelrenderer_get_palmapper(prend, palmapper_name);
            if(palmapper == NULL){
                fprintf(stderr, "Couldn't find palmapper: %s\n",
                    palmapper_name);
                free(palmapper_name); return 2;
            }
            free(palmapper_name);
        }
        err = fus_lexer_get(lexer, ")");
        if(err)return err;

        ARRAY_PUSH_NEW(hexmap_recording_t*, map->actor_recordings,
            recording)
        err = hexmap_recording_init(recording,
            filename, palmapper);
        if(err)return err;
    }
    err = fus_lexer_next(lexer);
    if(err)return err;
    }


    /* parse submaps */
    err = fus_lexer_get(lexer, "submaps");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    while(1){
        if(fus_lexer_got(lexer, ")"))break;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = hexmap_parse_submap(map, lexer, true,
            (vec_t){0}, (vec_t){0}, 0, NULL,
            default_palette_filename, default_tileset_filename);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
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
        vec_cpy(map->space->dims, map->spawn, spawn_submap->pos);
        free(spawn_filename);
    }


    /* pheeew */
    return 0;
}

int hexmap_parse_submap(hexmap_t *map, fus_lexer_t *lexer, bool solid,
    vec_t parent_pos, vec_t parent_camera_pos, int parent_camera_type,
    prismelmapper_t *parent_mapper, char *palette_filename,
    char *tileset_filename
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

    if(fus_lexer_got(lexer, "bg")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        solid = false;
    }

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

    vec_t pos = {0};
    if(fus_lexer_got(lexer, "pos")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_vec(lexer, space, pos);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }
    vec_add(space->dims, pos, parent_pos);

    int camera_type = parent_camera_type;
    vec_t camera_pos;
    vec_cpy(space->dims, camera_pos, parent_camera_pos);
    if(fus_lexer_got(lexer, "camera")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        if(fus_lexer_got(lexer, "follow")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            camera_type = 1;
        }else{
            err = fus_lexer_get_vec(lexer, space, camera_pos);
            if(err)return err;
            vec_add(space->dims, camera_pos, pos);
        }
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    prismelmapper_t *mapper = parent_mapper;
    if(fus_lexer_got(lexer, "mapper")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_mapper(lexer, map->prend, NULL, &mapper);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "palette")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_str(lexer, &palette_filename);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "tileset")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_str(lexer, &tileset_filename);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(submap_filename != NULL){
        ARRAY_PUSH_NEW(hexmap_submap_t*, map->submaps, submap)
        err = hexmap_submap_init(map, submap, strdup(submap_filename),
            solid, pos, camera_type, camera_pos, mapper,
            palette_filename, tileset_filename);
        if(err)return err;

        /* load collmap */
        err = hexcollmap_load(&submap->collmap, submap_filename);
        if(err)return err;

        /* render submap->rgraph_map */
        err = hexmap_submap_create_rgraph(map, submap);
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "recordings")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        while(1){
            if(fus_lexer_got(lexer, ")"))break;

            char *filename;
            palettemapper_t *palmapper = NULL;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_str(lexer, &filename);
            if(err)return err;
            if(!fus_lexer_got(lexer, ")")){
                char *palmapper_name;
                err = fus_lexer_get_str(lexer, &palmapper_name);
                if(err)return err;
                palmapper =
                    prismelrenderer_get_palmapper(prend, palmapper_name);
                if(palmapper == NULL){
                    fprintf(stderr, "Couldn't find palmapper: %s\n",
                        palmapper_name);
                    free(palmapper_name); return 2;
                }
                free(palmapper_name);
            }
            err = fus_lexer_get(lexer, ")");
            if(err)return err;

            ARRAY_PUSH_NEW(hexmap_recording_t*, map->recordings,
                recording)
            err = hexmap_recording_init(recording,
                filename, palmapper);
            if(err)return err;
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
            err = hexmap_parse_submap(map, lexer, true, pos,
                camera_pos, camera_type, mapper,
                palette_filename, tileset_filename);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }
        err = fus_lexer_next(lexer);
        if(err)return err;
    }

    return 0;
}

static int hexmap_collide_elem(hexmap_t *map, int all_type,
    int x, int y, trf_t *trf,
    hexcollmap_elem_t *elems2, int rot,
    void (*normalize_elem)(trf_t *index),
    hexcollmap_elem_t *(get_elem)(
        hexcollmap_t *collmap, trf_t *index),
    bool *collide_savepoint_ptr, bool *collide_door_ptr
){
    /* Returns true (1) or false (0), or 2 if caller should continue
    checking for a collision. */

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
            if(!submap->solid)continue;

            hexcollmap_t *collmap1 = &submap->collmap;

            trf_t subindex;
            hexspace_set(subindex.add,
                index.add[0] - submap->pos[0],
                index.add[1] + submap->pos[1]);
            subindex.rot = index.rot;
            subindex.flip = index.flip;

            hexcollmap_elem_t *elem = get_elem(collmap1, &subindex);
            if(elem != NULL){
                if(elem->tile_c == 'S')*collide_savepoint_ptr = true;
                if(elem->tile_c == 'D')*collide_door_ptr = true;
            }
            if(hexcollmap_elem_is_solid(elem)){
                collide = true; break;}
        }
        if(all_type != 2){
            bool all = all_type;
            if((all && !collide) || (!all && collide))return collide;
        }else{
            /* Just looking for savepoints & doors... */
        }
    }
    return 2; /* Caller should keep looking for a collision */
}

static bool _hexmap_collide(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, int all_type,
    bool *collide_savepoint_ptr, bool *collide_door_ptr
){

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

            int collide;
            int x = x2 - ox2;
            int y = y2 - oy2;
            collide = hexmap_collide_elem(map, all_type,
                x, y, trf,
                tile2->vert, 1,
                hexcollmap_normalize_vert,
                hexcollmap_get_vert,
                collide_savepoint_ptr, collide_door_ptr);
            if(collide != 2)return collide;
            collide = hexmap_collide_elem(map, all_type,
                x, y, trf,
                tile2->edge, 3,
                hexcollmap_normalize_edge,
                hexcollmap_get_edge,
                collide_savepoint_ptr, collide_door_ptr);
            if(collide != 2)return collide;
            collide = hexmap_collide_elem(map, all_type,
                x, y, trf,
                tile2->face, 2,
                hexcollmap_normalize_face,
                hexcollmap_get_face,
                collide_savepoint_ptr, collide_door_ptr);
            if(collide != 2)return collide;
        }
    }
    if(all_type == 2){
        /* Return value doesn't matter, we were just looking for
        savepoints & doors */
        return false;
    }else{
        bool all = all_type;
        if(all)return true;
        else return false;
    }
}

bool hexmap_collide(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, bool all
){
    bool collide_savepoint;
    bool collide_door;
    return _hexmap_collide(map, collmap2, trf, all,
        &collide_savepoint, &collide_door);
}

void hexmap_collide_special(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, bool *collide_savepoint_ptr, bool *collide_door_ptr
){
    int all_type = 2;
    _hexmap_collide(map, collmap2, trf, all_type,
        collide_savepoint_ptr, collide_door_ptr);
}



/*****************
 * HEXMAP SUBMAP *
 *****************/

void hexmap_submap_cleanup(hexmap_submap_t *submap){
    free(submap->filename);
    hexcollmap_cleanup(&submap->collmap);
    palette_cleanup(&submap->palette);
    hexmap_tileset_cleanup(&submap->tileset);
}

int hexmap_submap_init(hexmap_t *map, hexmap_submap_t *submap,
    char *filename, bool solid, vec_t pos, int camera_type, vec_t camera_pos,
    prismelmapper_t *mapper, char *palette_filename, char *tileset_filename
){
    int err;

    submap->filename = filename;
    vec_cpy(MAX_VEC_DIMS, submap->pos, pos);

    submap->solid = solid;

    submap->camera_type = camera_type;
    vec_cpy(MAX_VEC_DIMS, submap->camera_pos, camera_pos);

    err = hexcollmap_init(&submap->collmap, map->space, strdup(filename));
    if(err)return err;

    submap->rgraph_map = NULL;
    submap->mapper = mapper;

    err = palette_load(&submap->palette, palette_filename);
    if(err)return err;

    err = hexmap_tileset_load(&submap->tileset, map->prend,
        tileset_filename);
    if(err)return err;

    return 0;
}

static int add_tile_rgraph(rendergraph_t *rgraph, rendergraph_t *rgraph2,
    vecspace_t *space, vec_t add, rot_t rot
){
    int err;
    rendergraph_trf_t *rendergraph_trf;
    err = rendergraph_push_rendergraph_trf(rgraph, &rendergraph_trf);
    if(err)return err;
    rendergraph_trf->rendergraph = rgraph2;
    rendergraph_trf->trf.rot = rot;
    vec_cpy(space->dims, rendergraph_trf->trf.add, add);
    return 0;
}

int hexmap_submap_create_rgraph(hexmap_t *map, hexmap_submap_t *submap){
    int err;

    prismelrenderer_t *prend = map->prend;
    vecspace_t *space = prend->space;
    hexcollmap_t *collmap = &submap->collmap;

    /* BIG OL' HACK: If any "tile" rgraphs are animated, we need the
    map's rgraph to be animated also.
    The most correct way to do this is I guess to compute the LCD of
    the tile rgraphs' n_frames, and set the map's rgraph's n_frames
    to that.
    But for now we use a magic number which has "many" divisors.
    That's a lot of bitmaps to cache for the map's rgraph, though...
    if we're going to allow complicated map animations, maybe we
    should disable bitmap caching for it (somehow). */
    int n_frames = 24;

    ARRAY_PUSH_NEW(rendergraph_t*, prend->rendergraphs, rgraph)
    err = rendergraph_init(rgraph, strdup(submap->filename), prend, NULL,
        rendergraph_animation_type_default,
        n_frames);
    if(err)return err;

    hexmap_tileset_t *tileset = &submap->tileset;

    for(int y = 0; y < collmap->h; y++){
        for(int x = 0; x < collmap->w; x++){
            int px = x - collmap->ox;
            int py = y - collmap->oy;

            vec_t v;
            vec4_set(v, px + py, 0, -py, 0);
            vec_mul(space, v, map->unit);

            hexcollmap_tile_t *tile =
                &collmap->tiles[y * collmap->w + x];

            #define HEXMAP_ADD_TILE(PART, ROT) \
                for(int i = 0; i < ROT; i++){ \
                    hexcollmap_elem_t *elem = &tile->PART[i]; \
                    if(!hexcollmap_elem_is_visible(elem))continue; \
                    rendergraph_t *rgraph_tile = \
                        hexmap_tileset_get_rgraph_##PART( \
                            tileset, elem->tile_c); \
                    if(rgraph_tile == NULL){ \
                        fprintf(stderr, "Couldn't find " #PART " tile " \
                            "for character: %c\n", elem->tile_c); \
                        return 2;} \
                    err = add_tile_rgraph(rgraph, rgraph_tile, \
                        space, v, i * 2); \
                    if(err)return err; \
                }
            HEXMAP_ADD_TILE(vert, 1)
            HEXMAP_ADD_TILE(edge, 3)
            HEXMAP_ADD_TILE(face, 2)
            #undef HEXMAP_ADD_TILE
        }
    }

    submap->rgraph_map = rgraph;
    return 0;
}

