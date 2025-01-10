#ifndef _HEXCOLLMAP_H_
#define _HEXCOLLMAP_H_

#include <stdbool.h>

#include "array.h"
#include "lexer.h"
#include "vars.h"
#include "valexpr.h"
#include "stringstore.h"
#include "geom.h"
#include "hexbox.h"
#include "hexgame_location.h"



/************************
 * HELPERS & ALGORITHMS *
 ***********************/

void hexcollmap_normalize_vert(trf_t *index);
void hexcollmap_normalize_edge(trf_t *index);
void hexcollmap_normalize_face(trf_t *index);


/********************
 * HEXMAP RECORDING *
 ********************/

/* ...that is, enough information to start a recording at a given
location on a hexmap... */

enum hexmap_recording_type {
    HEXMAP_RECORDING_TYPE_RECORDING,
    HEXMAP_RECORDING_TYPE_ACTOR
};

static const char *hexmap_recording_type_msg(int type){
    switch(type){
        case HEXMAP_RECORDING_TYPE_RECORDING: return "Recording";
        case HEXMAP_RECORDING_TYPE_ACTOR: return "Actor";
        default: return "Unknown";
    }
}

typedef struct hexmap_recording {
    int type; /* enum hexmap_recording_type */
    const char *filename;
        /*
        HEXMAP_RECORDING_TYPE_RECORDING -> recording filename
        HEXMAP_RECORDING_TYPE_ACTOR -> actor stateset filename
        */
    const char *palmapper_name;
    trf_t trf;
    int frame_offset;

    valexpr_t visible_expr;
    valexpr_t target_expr;

    vars_t vars;
    vars_t bodyvars;
} hexmap_recording_t;

void hexmap_recording_cleanup(hexmap_recording_t *recording);
void hexmap_recording_init(hexmap_recording_t *recording, int type,
    const char *filename, const char *palmapper_name, int frame_offset);
int hexmap_recording_clone(hexmap_recording_t *recording1,
    hexmap_recording_t *recording2);


/**********************
 * HEXMAP RENDERGRAPH *
 **********************/

/* ...that is, enough information to display a rendergraph at a given
location on a hexmap... */

typedef struct hexmap_rendergraph {
    const char *name;
    const char *palmapper_name;
    trf_t trf;
        /* NOTE: trf is in hexspace! It's up to you to convert to
        prend->space once you actually have a prend and an rgraph! */
} hexmap_rendergraph_t;

void hexmap_rendergraph_cleanup(hexmap_rendergraph_t *rendergraph);
void hexmap_rendergraph_init(hexmap_rendergraph_t *rendergraph,
    const char *name, const char *palmapper_name);


/*******************
 * HEXMAP LOCATION *
 *******************/

typedef struct hexmap_location {
    const char *name;
    hexgame_location_t loc;
} hexmap_location_t;

void hexmap_location_cleanup(hexmap_location_t *location);
void hexmap_location_init(hexmap_location_t *location, const char *name);


/**************
 * HEXCOLLMAP *
 **************/

typedef struct hexcollmap_elem {
    char tile_c;
        /* Symbol representing the graphical tile (so, probably
        rendergraph) to use for this element.
        E.g. '0' might be the default, '1' a special version,
        'r', 'g', 'b' for red, green, blue versions...
        Should always be a printable character (in the sense of
        the standard library's isprint function), for debugging
        purposes.
        With that in mind, the default (no tile) value is ' '. */
    int z;
        /* z-depth: higher numbers overwrite lower ones */
} hexcollmap_elem_t;

typedef struct hexcollmap_tile {
    hexcollmap_elem_t vert[1];
    hexcollmap_elem_t edge[3];
    hexcollmap_elem_t face[2];
} hexcollmap_tile_t;

enum hexcollmap_part_type {
    HEXCOLLMAP_PART_TYPE_HEXCOLLMAP,
    HEXCOLLMAP_PART_TYPE_RECORDING,
    HEXCOLLMAP_PART_TYPE_ACTOR,
    HEXCOLLMAP_PART_TYPE_RENDERGRAPH,
    HEXCOLLMAP_PART_TYPE_LOCATION,
};

static const char *hexmap_part_type_msg(int type){
    switch(type){
        case HEXCOLLMAP_PART_TYPE_HEXCOLLMAP: return "hexcollmap";
        case HEXCOLLMAP_PART_TYPE_RECORDING: return "recording";
        case HEXCOLLMAP_PART_TYPE_ACTOR: return "actor";
        case HEXCOLLMAP_PART_TYPE_RENDERGRAPH: return "rendergraph";
        case HEXCOLLMAP_PART_TYPE_LOCATION: return "location";
        default: return "unknown";
    }
}

typedef struct hexcollmap_part {
    /* "Parts" only exist while hexcollmap is being parsed.
    They are declared at the top of hexcollmap's data, like this:

        parts:
            "a": ...etc...
            "b": ...etc...

    Each part is assigned a character ('a' and 'b' in the above example).
    They can be referred to using '?' (part) and '!' (part+vert), along
    with their character in the "tilebucket" (the [...] at end of line):

        collmap:
            ;; .   .   ?   .   [a]
            ;;
            ;;   .   ! - +     [b]
            ;;        \ /
            ;;     .   +

    Based on part's type (see enum hexcollmap_part_type), part may represent:
        * Another hexcollmap to be drawn onto current hexcollmap
        * A recording, actor, or rendergraph
    */

    int type; /* enum hexcollmap_part_type */
    char part_c;
    const char *filename;
    const char *palmapper_name;
    int frame_offset;

    trf_t trf;
    int draw_z;

    valexpr_t visible_expr;
    valexpr_t target_expr;

    vars_t vars;
    vars_t bodyvars;
} hexcollmap_part_t;

typedef struct hexcollmap {
    /*
    Hexcollmap's dimensions are:

        Y
         +
          \
          (.)- + X

    A hexcollmap with (w, h) = (1, 1) and (ox, oy) = (0, 0) looks like this:

         + - +
          \   \
          (+)- +

    A hexcollmap with (w, h) = (2, 2) and (ox, oy) = (1, 1) looks like this:

       + - + - +
        \       \
         +  (.)  +
          \       \
           + - + - +

    The array of tiles is actually stored with (0, 0) in the top-left, though!..
    So we often use coordinates in a "hexcollmap index" space, which is like
    regular hexspace, but with the y coordinate inverted.

    The hexbox's dimensions are like this:

             Y
           + - +
          /     \ X
         +  (.)  +
          \     / Z
           + - +

    */
    const char *filename;
    hexgame_location_t spawn;
    hexbox_t hexbox;
    int w;
    int h;
    int ox; /* origin's x */
    int oy; /* origin's y, in hexcollmap index space */
    hexcollmap_tile_t *tiles; /* array of length w * h */
    ARRAY_DECL(hexmap_recording_t*, recordings)
    ARRAY_DECL(hexmap_rendergraph_t*, rendergraphs)
    ARRAY_DECL(hexmap_location_t*, locations)
    ARRAY_DECL(valexpr_t*, text_exprs)

    /* Weakrefs */
    vecspace_t *space;
} hexcollmap_t;



static bool tile_c_is_visible(char tile_c){
    return strchr(" x.?", tile_c) == NULL;
}

static bool tile_c_is_solid(char tile_c){
    return strchr(" xSwo=@", tile_c) == NULL;
}

static bool tile_c_is_special(char tile_c){
    /* Whether tile_c represents a vert/edge/face, but is not one of the
    standard such chars (so, not in "+/-\\*") */
    return strchr("xSwot=@", tile_c) != NULL;
}

static bool hexcollmap_elem_is_visible(hexcollmap_elem_t *elem){
    if(elem == NULL)return false;
    return tile_c_is_visible(elem->tile_c);
}

static bool hexcollmap_elem_is_solid(hexcollmap_elem_t *elem){
    if(elem == NULL)return false;
    return tile_c_is_solid(elem->tile_c);
}

static bool hexcollmap_elem_is_special(hexcollmap_elem_t *elem){
    if(elem == NULL)return false;
    return tile_c_is_special(elem->tile_c);
}


void hexcollmap_part_init(hexcollmap_part_t *part, int type,
    char part_c, const char *filename, const char *palmapper_name,
    int frame_offset, valexpr_t *visible_expr, valexpr_t *target_expr,
    vars_t *vars, vars_t *bodyvars);
void hexcollmap_part_cleanup(hexcollmap_part_t *part);

void hexcollmap_cleanup(hexcollmap_t *collmap);
void hexcollmap_init(hexcollmap_t *collmap, vecspace_t *space,
    const char *filename);
void hexcollmap_init_clone(hexcollmap_t *collmap,
    hexcollmap_t *from_collmap, const char *filename);
int hexcollmap_init_tiles_from_hexbox(hexcollmap_t *collmap);
int hexcollmap_union_hexbox(hexcollmap_t *collmap, hexbox_t *hexbox);
void hexcollmap_dump(hexcollmap_t *collmap, FILE *f);
typedef struct hexcollmap_write_options {
    bool just_coll;
    bool extra;
    bool nodots;
    bool show_tiles;
    bool eol_semicolons;

    /* Weakrefs: */
    trf_t *marker;
        /* Optional marker, showing a position on the collmap */
} hexcollmap_write_options_t;
void hexcollmap_write_with_parts(hexcollmap_t *collmap, FILE *f,
    hexcollmap_write_options_t *opts,
    hexcollmap_part_t **parts, int parts_len);
void hexcollmap_write(hexcollmap_t *collmap, FILE *f,
    hexcollmap_write_options_t *opts);
int hexcollmap_parse_with_parts(hexcollmap_t *collmap, fus_lexer_t *lexer,
    vecspace_t *space, const char *filename, bool just_coll,
    hexcollmap_part_t ***parts_ptr, int *parts_len_ptr,
    stringstore_t *name_store, stringstore_t *filename_store);
int hexcollmap_parse(hexcollmap_t *collmap, fus_lexer_t *lexer,
    vecspace_t *space, const char *filename, bool just_coll,
    stringstore_t *name_store, stringstore_t *filename_store);
int hexcollmap_clone(hexcollmap_t *collmap,
    hexcollmap_t *collmap_from, rot_t rot);
int hexcollmap_load(hexcollmap_t *collmap, vecspace_t *space,
    const char *filename, vars_t *vars,
    stringstore_t *name_store, stringstore_t *filename_store);
hexcollmap_tile_t *hexcollmap_get_tile_xy(hexcollmap_t *collmap,
    int x, int y);
hexcollmap_tile_t *hexcollmap_get_tile(hexcollmap_t *collmap, trf_t *index);
hexcollmap_elem_t *hexcollmap_get_vert(hexcollmap_t *collmap, trf_t *index);
hexcollmap_elem_t *hexcollmap_get_edge(hexcollmap_t *collmap, trf_t *index);
hexcollmap_elem_t *hexcollmap_get_face(hexcollmap_t *collmap, trf_t *index);
bool hexcollmap_collide(
    hexcollmap_t *collmap1, trf_t *trf1,
    hexcollmap_t *collmap2, trf_t *trf2,
    vecspace_t *space, bool all);


#endif
