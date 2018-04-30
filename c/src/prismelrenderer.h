#ifndef _PRISMEL_H_
#define _PRISMEL_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "geom.h"



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


int prismel_create_images(prismel_t *prismel, int n_images);
int prismel_image_push_line(prismel_image_t *image, int x, int y, int w);



/*******************
 * PRISMELRENDERER *
 *******************/

typedef struct prismelrenderer_bitmap {
    int x, y; /* offset of bitmap's center ("origin") */
    int w, h; /* width, height */
    /*SDL_Surface *surface;*/

    struct prismelrenderer_bitmap *next;
} prismelrenderer_bitmap_t;

typedef struct prismelrenderer {
    vecspace_t *space;
    struct prismelrenderer_bitmap *bitmap_list;
    struct prismel *prismel_list;
} prismelrenderer_t;



int prismelrenderer_init(prismelrenderer_t *renderer, vecspace_t *space);
void prismelrenderer_dump(prismelrenderer_t *renderer, FILE *f);
int prismelrenderer_push_prismel(prismelrenderer_t *renderer);
prismel_t *prismelrenderer_get_prismel(prismelrenderer_t *renderer, char *name);



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

typedef struct rendergraph {
    vecspace_t *space;
    struct prismel_trf *prismel_trf_list;
    struct rendergraph_trf *rendergraph_trf_list;
    int n_bitmaps;
    prismelrenderer_bitmap_t **bitmaps;
    boundbox_t boundbox;
} rendergraph_t;


int rendergraph_init(rendergraph_t *rendergraph, vecspace_t *space);
void rendergraph_dump(rendergraph_t *rendergraph, FILE *f);
int rendergraph_create_bitmaps(rendergraph_t *rendergraph, int n_bitmaps);
int rendergraph_push_rendergraph_trf(rendergraph_t *rendergraph);
int rendergraph_push_prismel_trf(rendergraph_t *rendergraph);



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
void rendergraph_map_dump(rendergraph_map_t *map, FILE *f);




#endif