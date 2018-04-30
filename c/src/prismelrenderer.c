
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "prismelrenderer.h"



/***********
 * PRISMEL *
 ***********/


int prismel_create_images(prismel_t *prismel, int n_images){
    prismel_image_t *images = calloc(n_images, sizeof(prismel_image_t));
    if(images == NULL)return 1;
    prismel->n_images = n_images;
    prismel->images = images;
    return 0;
}

int prismel_image_push_line(prismel_image_t *image, int x, int y, int w){
    prismel_image_line_t *line = calloc(1, sizeof(prismel_image_line_t));
    if(line == NULL)return 1;
    line->x = x;
    line->y = y;
    line->w = w;
    line->next = image->line_list;
    image->line_list = line;
    return 0;
}



/*******************
 * PRISMELRENDERER *
 *******************/

int prismelrenderer_init(prismelrenderer_t *renderer, vecspace_t *space){
    renderer->space = space;
    renderer->bitmap_list = NULL;
    renderer->prismel_list = NULL;
    return 0;
}

void prismelrenderer_dump(prismelrenderer_t *renderer, FILE *f){
    fprintf(f, "prismelrenderer: %p\n", renderer);
    if(renderer == NULL)return;
    fprintf(f, "  space: %p\n", renderer->space);

    fprintf(f, "  prismels:\n");
    for(prismel_t *prismel = renderer->prismel_list;
        prismel != NULL; prismel = prismel->next
    ){
        fprintf(f, "    prismel: %p\n", prismel);
        fprintf(f, "      name: %s\n", prismel->name);
        fprintf(f, "      n_images: %i\n", prismel->n_images);
        fprintf(f, "      images:\n");
        for(int i = 0; i < prismel->n_images; i++){
            prismel_image_t *image = &prismel->images[i];
            fprintf(f, "        image: %p", image);
            for(prismel_image_line_t *line = image->line_list;
                line != NULL; line = line->next
            ){
                fprintf(f, " (%i %i %i)", line->x, line->y, line->w);
            }
            fprintf(f, "\n");
        }
    }

    fprintf(f, "  bitmaps:\n");
    for(prismelrenderer_bitmap_t *bitmap = renderer->bitmap_list;
        bitmap != NULL; bitmap = bitmap->next
    ){
        fprintf(f, "    %p\n", bitmap);
    }
}

int prismelrenderer_push_prismel(prismelrenderer_t *renderer){
    int err;
    prismel_t *prismel = calloc(1, sizeof(prismel_t));
    if(prismel == NULL)return 1;
    prismel->next = renderer->prismel_list;
    renderer->prismel_list = prismel;
    err = prismel_create_images(prismel, renderer->space->rot_max);
    if(err)goto err;
    return 0;
err:
    free(prismel);
    return err;
}

prismel_t *prismelrenderer_get_prismel(prismelrenderer_t *renderer, char *name){
    prismel_t *prismel = renderer->prismel_list;
    while(prismel != NULL){
        if(strcmp(prismel->name, name) == 0)return prismel;
        prismel = prismel->next;
    }
    return NULL;
}




/***************
 * RENDERGRAPH *
 ***************/

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



/*******************
 * RENDERGRAPH_MAP *
 *******************/


int rendergraph_map_push(rendergraph_map_t **map){
    rendergraph_map_t *new_map = calloc(1, sizeof(rendergraph_map_t));
    if(new_map == NULL)return 1;
    new_map->next = *map;
    *map = new_map;
    return 0;
}

bool name_eq(const char *text, const char *name){
    if(text == NULL || name == NULL)return text == name;
    return strcmp(text, name) == 0;
}

rendergraph_t *rendergraph_map_get(rendergraph_map_t *map, const char *name){
    while(map != NULL){
        if(name_eq(map->name, name))return map->rgraph;
        map = map->next;
    }
    return NULL;
}

void rendergraph_map_dump(rendergraph_map_t *map, FILE *f){
    fprintf(f, "rendergraph_map:\n");
    for(rendergraph_map_t *_map = map; _map != NULL; _map = _map->next){
        fprintf(f, "  %s: %p\n", _map->name, _map->rgraph);
    }
    for(rendergraph_map_t *_map = map; _map != NULL; _map = _map->next){
        rendergraph_dump(_map->rgraph, f);
    }
}



