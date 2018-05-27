#ifndef _PRISMEL_H_
#define _PRISMEL_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "geom.h"
#include "lexer.h"
#include "bounds.h"


/***********
 * GENERAL *
 ***********/

int get_n_bitmaps(vecspace_t *space);
int get_bitmap_i(vecspace_t *space, trf_t *trf);


/***********
 * PRISMEL *
 ***********/

typedef struct prismel_image_line {
    int x, y, w;
    struct prismel_image_line *next;
} prismel_image_line_t;

typedef struct prismel_image {
    struct prismel_image_line *line_list;
} prismel_image_t;

typedef struct prismel {
    char *name;

    int n_images;
    struct prismel_image *images;

    struct prismel *next;
} prismel_t;


int prismel_create_images(prismel_t *prismel, vecspace_t *space);
int prismel_image_push_line(prismel_image_t *image, int x, int y, int w);
void prismel_get_boundary_box(prismel_t *prismel, boundary_box_t *box,
    int bitmap_i);



/*******************
 * PRISMELRENDERER *
 *******************/

typedef struct prismelrenderer {
    vecspace_t *space;
    struct prismel *prismel_list;
    struct rendergraph_map *rendergraph_map;
} prismelrenderer_t;



int prismelrenderer_init(prismelrenderer_t *renderer, vecspace_t *space);
void prismelrenderer_dump(prismelrenderer_t *renderer, FILE *f);
int prismelrenderer_push_prismel(prismelrenderer_t *renderer);
prismel_t *prismelrenderer_get_prismel(prismelrenderer_t *renderer,
    char *name);
int prismelrenderer_parse(prismelrenderer_t *prend, fus_lexer_t *lexer);



/***************
 * RENDERGRAPH *
 ***************/

typedef struct prismel_trf {
    prismel_t *prismel;
    int color;
    trf_t trf;
    struct prismel_trf *next;
} prismel_trf_t;

typedef struct rendergraph_trf {
    struct rendergraph *rendergraph;
    trf_t trf;
    struct rendergraph_trf *next;
} rendergraph_trf_t;

typedef struct rendergraph_bitmap {
    position_box_t pbox;
    SDL_Surface *surface;
} rendergraph_bitmap_t;

typedef struct rendergraph {
    vecspace_t *space;
    struct prismel_trf *prismel_trf_list;
    struct rendergraph_trf *rendergraph_trf_list;
    int n_bitmaps;
    rendergraph_bitmap_t *bitmaps;
    boundbox_t boundbox;
} rendergraph_t;


int rendergraph_init(rendergraph_t *rendergraph, vecspace_t *space);
void rendergraph_bitmap_dump(rendergraph_bitmap_t *bitmap, FILE *f,
    int n_spaces);
void rendergraph_dump(rendergraph_t *rendergraph, FILE *f, int n_spaces);
int rendergraph_create_bitmaps(rendergraph_t *rendergraph);
int rendergraph_push_rendergraph_trf(rendergraph_t *rendergraph);
int rendergraph_push_prismel_trf(rendergraph_t *rendergraph);
int rendergraph_get_bitmap_i(rendergraph_t *rendergraph, trf_t *trf);
int rendergraph_render_bitmap(rendergraph_t *rendergraph, trf_t *trf,
    SDL_Color pal[]);
int rendergraph_get_or_render_bitmap(rendergraph_t *rendergraph,
    rendergraph_bitmap_t **bitmap_ptr, trf_t *trf, SDL_Color pal[]);



/*******************
 * RENDERGRAPH_MAP *
 *******************/

typedef struct rendergraph_map {
    char *name;
    rendergraph_t *rgraph;
    struct rendergraph_map *next;
} rendergraph_map_t;

int rendergraph_map_push(rendergraph_map_t **map);
rendergraph_t *rendergraph_map_get(rendergraph_map_t *map, const char *name);


#endif