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



/******************
 * HEXMAP TILESET *
 ******************/

#define HEXMAP_TILESET_ENTRY_RGRAPHS 4

enum hexmap_tileset_entry_type {
    HEXMAP_TILESET_ENTRY_TYPE_ROTS,
    HEXMAP_TILESET_ENTRY_TYPE_WHEN_FACES_SOLID,
    HEXMAP_TILESET_ENTRY_TYPES
};

typedef struct hexmap_tileset_entry {
    int type; /* enum hexmap_tileset_entry_type */
    char tile_c;
        /* see hexcollmap_elem->tile_c */
    int n_rgraphs;
        /* Between 1 and HEXMAP_TILESET_ENTRY_RGRAPHS */
    int frame_offset;
        /* How tiles' positions should affect the frame offset of
        their animation.
        For now, should basically be treated as a bool (0/1). */

    /* Weakrefs */
    rendergraph_t *rgraphs[HEXMAP_TILESET_ENTRY_RGRAPHS];
} hexmap_tileset_entry_t;

typedef struct hexmap_tileset {
    const char *name;

    /* Weakrefs */
    ARRAY_DECL(hexmap_tileset_entry_t*, vert_entries)
    ARRAY_DECL(hexmap_tileset_entry_t*, edge_entries)
    ARRAY_DECL(hexmap_tileset_entry_t*, face_entries)
} hexmap_tileset_t;

void hexmap_tileset_cleanup(hexmap_tileset_t *tileset);
int hexmap_tileset_init(hexmap_tileset_t *tileset, const char *name);
int hexmap_tileset_load(hexmap_tileset_t *tileset,
    prismelrenderer_t *prend, const char *filename, vars_t *vars);


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
    hexmap_collision_elem_t water;
    hexmap_collision_elem_t door;
} hexmap_collision_t;


/**********
 * HEXMAP *
 **********/

typedef struct hexmap_door {
    hexgame_savelocation_t location;

    /* Weakrefs */
    hexcollmap_elem_t *elem;
        /* We use this to mark door's position within its submap's
        collmap, which is kind of silly but works well so long as
        hexmap_collision_t also stores hexcollmap_elem_t* instead of
        actual positions. */
} hexmap_door_t;

typedef struct hexmap_submap_group {
    const char *name;

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
    bool visible_not; /* Whether to invert the result of visible_expr */
    hexcollmap_t collmap;
    ARRAY_DECL(hexmap_door_t*, doors)

    /* Weakrefs */
    hexmap_submap_group_t *group;
    struct hexmap *map;
    rendergraph_t *rgraph_map;
    rendergraph_t *rgraph_minimap;
    prismelmapper_t *mapper;
    palette_t *palette;
    hexmap_tileset_t *tileset;
} hexmap_submap_t;

typedef struct hexmap_submap_parser_context {
    bool solid;
    vec_t pos;
    rot_t rot;
    vec_t camera_pos;
    int camera_type;

    valexpr_t visible_expr;
    bool visible_not;

    ARRAY_DECL(struct valexpr*, text_exprs)

    /* Weakrefs */
    hexmap_submap_group_t *submap_group;
    struct hexmap_submap_parser_context *parent;
    prismelmapper_t *mapper;
    palette_t *palette;
    hexmap_tileset_t *tileset;
} hexmap_submap_parser_context_t;

const char *submap_camera_type_msg(int camera_type);

typedef struct hexmap {
    const char *filename;
    const char *name;

    vars_t vars;

    hexgame_location_t spawn;
    vec_t unit;

    ARRAY_DECL(struct body*, bodies)
    ARRAY_DECL(hexmap_submap_t*, submaps)
    ARRAY_DECL(hexmap_submap_group_t*, submap_groups)
    ARRAY_DECL(hexmap_recording_t*, recordings)
    ARRAY_DECL(palette_t*, palettes)
    ARRAY_DECL(hexmap_tileset_t*, tilesets)
    ARRAY_DECL(hexmap_location_t*, locations)

    /* Weakrefs */
    struct hexgame *game;
    vecspace_t *space; /* Always hexspace! */
    prismelrenderer_t *prend;
} hexmap_t;


void hexmap_cleanup(hexmap_t *map);
int hexmap_init(hexmap_t *map, struct hexgame *game, const char *filename,
    vec_t unit);
hexgame_location_t *hexmap_get_location(hexmap_t *map, const char *name);
hexmap_submap_group_t *hexmap_get_submap_group(hexmap_t *map,
    const char *name);
int hexmap_get_or_add_submap_group(hexmap_t *map, const char *name,
    hexmap_submap_group_t **group_ptr);
int hexmap_load(hexmap_t *map, struct hexgame *game, const char *filename,
    vars_t *vars);
int hexmap_parse(hexmap_t *map, struct hexgame *game, const char *filename,
    fus_lexer_t *lexer);
int hexmap_parse_submap(hexmap_t *map, fus_lexer_t *lexer,
    hexmap_submap_parser_context_t *parent_context,
    hexmap_submap_group_t *group);
int hexmap_get_submap_index(hexmap_t *map, hexmap_submap_t *submap);
int hexmap_get_or_create_palette(hexmap_t *map, const char *name,
    palette_t **palette_ptr);
int hexmap_get_or_create_tileset(hexmap_t *map, const char *name,
    hexmap_tileset_t **tileset_ptr);
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

void hexmap_door_cleanup(hexmap_door_t *door);
void hexmap_submap_group_cleanup(hexmap_submap_group_t *group);
void hexmap_submap_group_init(hexmap_submap_group_t *group,
    const char *name);
void hexmap_submap_cleanup(hexmap_submap_t *submap);
int hexmap_submap_init_from_parser_context(hexmap_t *map,
    hexmap_submap_t *submap, const char *filename,
    hexmap_submap_parser_context_t *context);
void hexmap_submap_visit(hexmap_submap_t *submap);
bool hexmap_submap_is_visited(hexmap_submap_t *submap);
int hexmap_submap_is_visible(hexmap_submap_t *submap, bool *visible_ptr);
int hexmap_submap_is_solid(hexmap_submap_t *submap, bool *solid_ptr);
int hexmap_submap_create_rgraph_map(hexmap_submap_t *submap);
int hexmap_submap_create_rgraph_minimap(hexmap_submap_t *submap);
hexmap_door_t *hexmap_submap_get_door(hexmap_submap_t *submap,
    hexcollmap_elem_t *elem);

void hexmap_submap_parser_context_init(hexmap_submap_parser_context_t *context,
    hexmap_submap_parser_context_t *parent);
void hexmap_submap_parser_context_cleanup(hexmap_submap_parser_context_t *context);


#endif