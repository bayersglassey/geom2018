#ifndef _HEXMAP_H_
#define _HEXMAP_H_

#include "array.h"
#include "lexer.h"
#include "vars.h"
#include "valexpr.h"
#include "geom.h"
#include "hexgame_savelocation.h"
#include "prismelrenderer.h"
#include "hexcollmap.h"
#include "hexgame_audio.h"


/* BIG OL' HACK: If any "tile" rgraphs are animated, we need the
map's rgraph to be animated also.
The most correct way to do this is I guess to compute the LCD of
the tile rgraphs' n_frames, and set the map's rgraph's n_frames
to that.
But for now we use a magic number which has "many" divisors.
That's a lot of bitmaps to cache for the map's rgraph, though...
if we're going to allow complicated map animations, maybe we
should disable bitmap caching for it (somehow). */
#define HEXMAP_SUBMAP_RGRAPH_N_FRAMES 24


/********************
 * HEXMAP COLLISION *
 ********************/

typedef struct hexmap_collision_elem {
    /* Weakrefs */
    struct hexmap_submap *submap;
    hexcollmap_elem_t *elem;
} hexmap_collision_elem_t;

typedef struct hexmap_collision {
    hexmap_collision_elem_t savepoint;
    hexmap_collision_elem_t mappoint;
    hexmap_collision_elem_t water;
} hexmap_collision_t;


/**********
 * HEXMAP *
 **********/

typedef struct hexmap_submap_group {
    const char *name;
        /* Name is never NULL; there is one group per map with the name "",
        which is the "root" group (to which submaps belong by default) */

    valexpr_t target_expr;
        /* Whether this group's submaps should be rendered as "targets" on
        the minimap */
    bool target;
        /* Cached evaluation of target_expr each frame */

    bool visited;
        /* Controls whether this group's submaps show up on minimap */
} hexmap_submap_group_t;

typedef struct hexmap_submap {
    bool solid;
    vec_t pos;
    vec_t camera_pos;
    int camera_type; /* enum camera_type */
    const char *filename;
    ARRAY_DECL(valexpr_t*, text_exprs)
        /* Text displayed when camera is on this submap */
    valexpr_t visible_expr; /* Whether this submap is visible */
    hexcollmap_t collmap;

    hexgame_audio_callback_t *song;
    vars_t song_vars;

    /* Weakrefs */
    hexmap_submap_group_t *group;
    struct hexmap *map;
    rendergraph_t *rgraph_map;
    rendergraph_t *rgraph_minimap;
    prismelmapper_t *mapper;
    palette_t *palette;
    tileset_t *tileset;
} hexmap_submap_t;

typedef struct hexmap_submap_parser_context {
    bool solid;
    vec_t pos;
    rot_t rot;
    vec_t camera_pos;
    int camera_type;

    valexpr_t visible_expr;

    hexgame_audio_callback_t *song;
    vars_t song_vars;

    ARRAY_DECL(struct valexpr*, text_exprs)

    /* Weakrefs */
    hexmap_submap_group_t *submap_group;
    struct hexmap_submap_parser_context *parent;
    prismelmapper_t *mapper;
    palette_t *palette;
    tileset_t *tileset;
} hexmap_submap_parser_context_t;

const char *submap_camera_type_msg(int camera_type);

typedef struct hexmap {
    const char *filename;
    const char *name;

    vars_t vars;

    hexgame_location_t spawn;
    vec_t unit;

    bool loaded;
        /* Whether we have already been fully loaded once, in the sense
        of _hexmap_load.
        If this is true, we can call _hexmap_load again, in order
        to "reload" the map -- so that we can easily test
        changes we've made to any of the files describing it, etc.
        The behaviour of _hexmap_load is slightly different when
        map->loaded is true, e.g. it allows "redefinition" of submaps. */

    hexgame_audio_callback_t *song;
    vars_t song_vars;

    ARRAY_DECL(struct body*, bodies)
    ARRAY_DECL(hexmap_submap_t*, submaps)
    ARRAY_DECL(hexmap_submap_group_t*, submap_groups)
    ARRAY_DECL(hexmap_recording_t*, recordings)
    ARRAY_DECL(hexmap_location_t*, locations)

    /* Weakrefs */
    struct hexgame *game;
    vecspace_t *space; /* Always hexspace! */
    prismelrenderer_t *prend;
    palette_t *default_palette;
    tileset_t *default_tileset;
    prismelmapper_t *default_mapper;
} hexmap_t;


void hexmap_cleanup(hexmap_t *map);
int hexmap_init(hexmap_t *map, struct hexgame *game, const char *filename);
int hexmap_reload(hexmap_t *map);
hexgame_location_t *hexmap_get_location(hexmap_t *map, const char *name);
hexmap_submap_group_t *hexmap_get_submap_group(hexmap_t *map,
    const char *name);
int hexmap_add_submap_group(hexmap_t *map, const char *name,
    hexmap_submap_group_t **group_ptr);
int hexmap_get_or_add_submap_group(hexmap_t *map, const char *name,
    hexmap_submap_group_t **group_ptr);
int hexmap_load(hexmap_t *map, vars_t *vars);
int hexmap_parse(hexmap_t *map, fus_lexer_t *lexer);
int hexmap_parse_submap(hexmap_t *map, fus_lexer_t *lexer,
    hexmap_submap_parser_context_t *parent_context,
    hexmap_submap_group_t *group);
int hexmap_get_submap_index(hexmap_t *map, hexmap_submap_t *submap);
int hexmap_load_recording(hexmap_t *map, const char *filename,
    palettemapper_t *palmapper, bool loop, int offset, trf_t *trf,
    struct body **body_ptr);
int hexmap_collide(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, bool all, bool *collide_ptr);
int hexmap_collide_special(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf,
    hexmap_collision_t *collision);
int hexmap_step(hexmap_t *map);
int hexmap_refresh_vars(hexmap_t *map);

void hexmap_submap_group_cleanup(hexmap_submap_group_t *group);
void hexmap_submap_group_init(hexmap_submap_group_t *group,
    const char *name);
void hexmap_submap_cleanup(hexmap_submap_t *submap);
int hexmap_submap_init_from_parser_context(hexmap_t *map,
    hexmap_submap_t *submap, const char *filename,
    hexmap_submap_parser_context_t *context);
void hexmap_submap_visit(hexmap_submap_t *submap);
bool hexmap_submap_is_visited(hexmap_submap_t *submap);
bool hexmap_submap_is_target(hexmap_submap_t *submap);
int hexmap_submap_is_visible(hexmap_submap_t *submap, bool *visible_ptr);
int hexmap_submap_is_solid(hexmap_submap_t *submap, bool *solid_ptr);
int rendergraph_add_rgraphs_from_collmap(
    rendergraph_t *rgraph, hexcollmap_t *collmap,
    tileset_t *tileset, bool add_collmap_rendergraphs);
int hexmap_submap_create_rgraph_map(hexmap_submap_t *submap);
int hexmap_submap_create_rgraph_minimap(hexmap_submap_t *submap);

void hexmap_submap_parser_context_init(hexmap_submap_parser_context_t *context,
    hexmap_submap_parser_context_t *parent);
void hexmap_submap_parser_context_cleanup(hexmap_submap_parser_context_t *context);


#endif
