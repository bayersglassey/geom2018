#ifndef _PRISMEL_H_
#define _PRISMEL_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "geom.h"


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
int prismelrenderer_push_prismel(prismelrenderer_t *renderer);
int prismel_create_images(prismel_t *prismel, int n_images);
int prismel_image_push_line(prismel_image_t *image);


#endif