#ifndef _HEXCOLLMAP_H_
#define _HEXCOLLMAP_H_

#include <stdbool.h>

#include "array.h"
#include "lexer.h"
#include "geom.h"



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
    char *palmapper_name;
    trf_t trf;
    int frame_offset;
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
    HEXCOLLMAP_PART_TYPE_RENDERGRAPH,
};

static const char *hexmap_part_type_msg(int type){
    switch(type){
        case HEXCOLLMAP_PART_TYPE_HEXCOLLMAP: return "Hexcollmap";
        case HEXCOLLMAP_PART_TYPE_RECORDING: return "Recording";
        case HEXCOLLMAP_PART_TYPE_RENDERGRAPH: return "Rendergraph";
        default: return "Unknown";
    }
}

typedef struct hexcollmap_part {
    int type; /* enum hexcollmap_part_type */
    char part_c;
    char *filename;
    char *palmapper_name;
    int frame_offset;

    trf_t trf;
    int draw_z;
} hexcollmap_part_t;

typedef struct hexcollmap {
    char *name;
    int w;
    int h;
    int ox;
    int oy;
    hexcollmap_tile_t *tiles;
    ARRAY_DECL(hexmap_recording_t*, recordings)
    ARRAY_DECL(hexmap_rendergraph_t*, rendergraphs)

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
    char part_c, char *filename, char *palmapper_name, int frame_offset);
void hexcollmap_part_cleanup(hexcollmap_part_t *part);

void hexcollmap_cleanup(hexcollmap_t *collmap);
int hexcollmap_init(hexcollmap_t *collmap, vecspace_t *space,
    char *name);
void hexcollmap_dump(hexcollmap_t *collmap, FILE *f);
void hexcollmap_write_with_parts(hexcollmap_t *collmap, FILE *f,
    bool just_coll, bool extra, bool nodots,
    hexcollmap_part_t **parts, int parts_len);
void hexcollmap_write(hexcollmap_t *collmap, FILE *f,
    bool just_coll, bool extra, bool nodots);
int hexcollmap_parse_with_parts(hexcollmap_t *collmap, fus_lexer_t *lexer,
    bool just_coll,
    hexcollmap_part_t ***parts_ptr, int *parts_len_ptr);
int hexcollmap_parse(hexcollmap_t *collmap, fus_lexer_t *lexer,
    bool just_coll);
int hexcollmap_load(hexcollmap_t *collmap, const char *filename,
    vars_t *vars);
void hexcollmap_normalize_vert(trf_t *index);
void hexcollmap_normalize_edge(trf_t *index);
void hexcollmap_normalize_face(trf_t *index);
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