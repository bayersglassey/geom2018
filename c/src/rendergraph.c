#ifndef _RENDERGRAPH_H_
#define _RENDERGRAPH_H_


#include "geom.h" /* trf_t, boundbox_t */
#include "util.h" /* load_file */
#include "lexer.h" /* fus_lexer */
#include "prismel.h" /* prismelrenderer_t */


typedef struct prismel_trf {
    prismel_t *prismel;
    trf_t t;
} prismel_trf_t;

typedef struct rendergraph_node_trf {
    struct rendergraph_node *node;
    trf_t t;
} rendergraph_node_trf_t;

typedef struct rendergraph_node {
    int n_prismel_trfs;
    struct prismel_trf *prismel_trfs;
    int n_rendergraph_trfs;
    struct rendergraph_trf *rendergraph_trfs;
    int n_bitmaps;
    prismelrenderer_bitmap_t *bitmaps;
    boundbox_t boundbox;

    struct rendergraph_node *next;
} rendergraph_node_t;

typedef struct rendergraph {
    struct rendergraph_node *root;
    struct rendergraph_node *nodes;
} rendergraph_t;


#endif