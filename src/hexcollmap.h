#ifndef _HEXCOLLMAP_H_
#define _HEXCOLLMAP_H_

#include <stdbool.h>

#include "array.h"
#include "lexer.h"
#include "vars.h"
#include "valexpr.h"
#include "geom.h"
#include "hexbox.h"
#include "hexgame_location.h"



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
    char *filename;
        /*
        HEXMAP_RECORDING_TYPE_RECORDING -> recording filename
        HEXMAP_RECORDING_TYPE_ACTOR -> actor stateset filename
        */
    char *palmapper_name;
    trf_t trf;
    int frame_offset;

    valexpr_t visible_expr;
    bool visible_not;

    vars_t vars;
    vars_t bodyvars;
} hexmap_recording_t;

void hexmap_recording_cleanup(hexmap_recording_t *recording);
int hexmap_recording_init(hexmap_recording_t *recording, int type,
    char *filename, char *palmapper_name, int frame_offset);


/**********************
 * HEXMAP RENDERGRAPH *
 **********************/

/* ...that is, enough information to display a rendergraph at a given
location on a hexmap... */

typedef struct hexmap_rendergraph {
    char *name;
    char *palmapper_name;
    trf_t trf;
        /* NOTE: trf is in hexspace! It's up to you to convert to
        prend->space once you actually have a prend and an rgraph! */
} hexmap_rendergraph_t;

void hexmap_rendergraph_cleanup(hexmap_rendergraph_t *rendergraph);
int hexmap_rendergraph_init(hexmap_rendergraph_t *rendergraph,
    char *name, char *palmapper_name);


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
};

static const char *hexmap_part_type_msg(int type){
    switch(type){
        case HEXCOLLMAP_PART_TYPE_HEXCOLLMAP: return "Hexcollmap";
        case HEXCOLLMAP_PART_TYPE_RECORDING: return "Recording";
        case HEXCOLLMAP_PART_TYPE_ACTOR: return "Actor";
        case HEXCOLLMAP_PART_TYPE_RENDERGRAPH: return "Rendergraph";
        default: return "Unknown";
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
    char *filename;
    char *palmapper_name;
    int frame_offset;

    trf_t trf;
    int draw_z;

    valexpr_t visible_expr;
    bool visible_not;

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

    The array of tiles is actually stored with (0, 0) in the top-left, though.

    The hexbox's dimensions are like this:

             Y
           + - +
          /     \ X
         +  (.)  +
          \     / Z
           + - +

    */
    char *filename;
    hexgame_location_t spawn;
    hexbox_t hexbox;
    int w;
    int h;
    int ox;
    int oy;
    hexcollmap_tile_t *tiles; /* array of length w * h */
    ARRAY_DECL(hexmap_recording_t*, recordings)
    ARRAY_DECL(hexmap_rendergraph_t*, rendergraphs)
    ARRAY_DECL(valexpr_t*, text_exprs)

    /* Weakrefs */
    vecspace_t *space;
} hexcollmap_t;



static bool tile_c_is_visible(char tile_c){
    return tile_c != ' ' && tile_c != 'x';
}

static bool tile_c_is_solid(char tile_c){
    return strchr(" xSDw", tile_c) == NULL;
}

static bool tile_c_is_special(char tile_c){
    return strchr("xSDw", tile_c) != NULL;
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


int hexcollmap_part_init(hexcollmap_part_t *part, int type,
    char part_c, char *filename, char *palmapper_name, int frame_offset,
    valexpr_t *visible_expr, bool visible_not,
    vars_t *vars, vars_t *bodyvars);
void hexcollmap_part_cleanup(hexcollmap_part_t *part);

void hexcollmap_cleanup(hexcollmap_t *collmap);
void hexcollmap_init(hexcollmap_t *collmap, vecspace_t *space,
    char *filename);
void hexcollmap_init_clone(hexcollmap_t *collmap,
    hexcollmap_t *from_collmap, char *filename);
int hexcollmap_init_tiles_from_hexbox(hexcollmap_t *collmap);
int hexcollmap_union_hexbox(hexcollmap_t *collmap, hexbox_t *hexbox);
void hexcollmap_dump(hexcollmap_t *collmap, FILE *f);
void hexcollmap_write_with_parts(hexcollmap_t *collmap, FILE *f,
    bool just_coll, bool extra, bool nodots, bool show_tiles,
    bool eol_semicolons, hexcollmap_part_t **parts, int parts_len);
void hexcollmap_write(hexcollmap_t *collmap, FILE *f,
    bool just_coll, bool extra, bool nodots, bool show_tiles,
    bool eol_semicolons);
int hexcollmap_parse_with_parts(hexcollmap_t *collmap, fus_lexer_t *lexer,
    vecspace_t *space, char *filename, bool just_coll,
    hexcollmap_part_t ***parts_ptr, int *parts_len_ptr);
int hexcollmap_parse(hexcollmap_t *collmap, fus_lexer_t *lexer,
    vecspace_t *space, char *filename, bool just_coll);
int hexcollmap_clone(hexcollmap_t *collmap,
    hexcollmap_t *collmap_from, rot_t rot);
int hexcollmap_load(hexcollmap_t *collmap, vecspace_t *space,
    const char *filename, vars_t *vars);
void hexcollmap_normalize_vert(trf_t *index);
void hexcollmap_normalize_edge(trf_t *index);
void hexcollmap_normalize_face(trf_t *index);
hexcollmap_tile_t *hexcollmap_get_tile_xy(hexcollmap_t *collmap,
    int x, int y);
hexcollmap_tile_t *hexcollmap_get_tile(hexcollmap_t *collmap, trf_t *index);
hexcollmap_elem_t *hexcollmap_get_vert(hexcollmap_t *collmap, trf_t *index);
hexcollmap_elem_t *hexcollmap_get_edge(hexcollmap_t *collmap, trf_t *index);
hexcollmap_elem_t *hexcollmap_get_face(hexcollmap_t *collmap, trf_t *index);
bool hexcollmap_elem_is_visible(hexcollmap_elem_t *elem);
bool hexcollmap_elem_is_solid(hexcollmap_elem_t *elem);
bool hexcollmap_collide(
    hexcollmap_t *collmap1, trf_t *trf1,
    hexcollmap_t *collmap2, trf_t *trf2,
    vecspace_t *space, bool all);


#endif