

#include "rendergraph.h"


int rendergraph_init(rendergraph_t *rendergraph, vecspace_t *space){
    int err;
    rendergraph->space = space;
    rendergraph->prismel_trf_list = NULL;
    rendergraph->rendergraph_trf_list = NULL;
    err = rendergraph_create_bitmaps(rendergraph, space->rot_max);
    if(err)return err;
    boundbox_init(rendergraph->boundbox, space->dims);
    return 0;
}

void rendergraph_dump(rendergraph_t *rendergraph, FILE *f){
    fprintf(f, "rendergraph: %p\n", rendergraph);
    if(rendergraph == NULL)return;
    fprintf(f, "  space: %p\n", rendergraph->space);

    fprintf(f, "  prismel_trf_list:\n");
    for(prismel_trf_t *prismel_trf = rendergraph->prismel_trf_list;
        prismel_trf != NULL; prismel_trf = prismel_trf->next
    ){
        prismel_t *prismel = prismel_trf->prismel;
        fprintf(f, "    prismel_trf: %s ", prismel == NULL? "NULL": prismel->name);
        trf_fprintf(f, rendergraph->space->dims, &prismel_trf->trf);
            fprintf(f, " %i\n", prismel_trf->color);
    }

    fprintf(f, "  rendergraph_trf_list:\n");
    for(rendergraph_trf_t *rendergraph_trf = rendergraph->rendergraph_trf_list;
        rendergraph_trf != NULL; rendergraph_trf = rendergraph_trf->next
    ){
        rendergraph_t *rendergraph = rendergraph_trf->rendergraph;
        fprintf(f, "    rendergraph_trf: %p ", rendergraph);
        trf_fprintf(f, rendergraph->space->dims, &rendergraph_trf->trf);
            fprintf(f, "\n");
    }

    fprintf(f, "  n_bitmaps: %i\n", rendergraph->n_bitmaps);
    fprintf(f, "  bitmaps:\n");
    for(int i = 0; i < rendergraph->n_bitmaps; i++){
        fprintf(f, "    bitmap: %p\n", rendergraph->bitmaps[i]);
    }

    fprintf(f, "  boundbox: "); boundbox_fprintf(f,
        rendergraph->space->dims, rendergraph->boundbox); fprintf(f, "\n");
}

int rendergraph_create_bitmaps(rendergraph_t *rendergraph, int n_bitmaps){
    prismelrenderer_bitmap_t **bitmaps = calloc(n_bitmaps,
        sizeof(prismelrenderer_bitmap_t *));
    if(bitmaps == NULL)return 1;
    rendergraph->n_bitmaps = n_bitmaps;
    rendergraph->bitmaps = bitmaps;
    return 0;
}

int rendergraph_push_rendergraph_trf(rendergraph_t *rendergraph){
    rendergraph_trf_t *rendergraph_trf = calloc(1, sizeof(rendergraph_trf_t));
    if(rendergraph_trf == NULL)return 1;
    rendergraph_trf->next = rendergraph->rendergraph_trf_list;
    rendergraph->rendergraph_trf_list = rendergraph_trf;
    return 0;
}

int rendergraph_push_prismel_trf(rendergraph_t *rendergraph){
    prismel_trf_t *prismel_trf = calloc(1, sizeof(prismel_trf_t));
    if(prismel_trf == NULL)return 1;
    prismel_trf->next = rendergraph->prismel_trf_list;
    rendergraph->prismel_trf_list = prismel_trf;
    return 0;
}

