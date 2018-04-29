#ifndef _RENDERGRAPH_H_
#define _RENDERGRAPH_H_


#include "geom.h"
#include "prismel.h"


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

#endif