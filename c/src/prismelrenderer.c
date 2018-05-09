
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>

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
    renderer->prismel_list = NULL;
    renderer->rendergraph_map = NULL;
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
                fprintf(f, " (% i % i % i)", line->x, line->y, line->w);
            }
            fprintf(f, "\n");
        }
    }

    fprintf(f, "  rendergraph_map:\n");
    for(rendergraph_map_t *map = renderer->rendergraph_map;
        map != NULL; map = map->next
    ){
        fprintf(f, "    %s:\n", map->name);
        rendergraph_dump(map->rgraph, f, 6);
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

prismel_t *prismelrenderer_get_prismel(prismelrenderer_t *renderer,
    char *name
){
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

void rendergraph_dump(rendergraph_t *rendergraph, FILE *f, int n_spaces){
    char spaces[20+1];
    if(n_spaces > 20){
        fprintf(f, "%s: %s: Can't handle %i spaces, sorry!\n",
            __FILE__, __func__, n_spaces);
        return;
    }
    for(int i = 0; i < n_spaces; i++)spaces[i] = ' ';
    spaces[n_spaces] = '\0';

    fprintf(f, "%srendergraph: %p\n", spaces, rendergraph);
    if(rendergraph == NULL)return;
    fprintf(f, "%s  space: %p\n", spaces, rendergraph->space);

    fprintf(f, "%s  prismel_trf_list:\n", spaces);
    for(prismel_trf_t *prismel_trf = rendergraph->prismel_trf_list;
        prismel_trf != NULL; prismel_trf = prismel_trf->next
    ){
        prismel_t *prismel = prismel_trf->prismel;
        fprintf(f, "%s    prismel_trf: %7s ", spaces,
            prismel == NULL? "NULL": prismel->name);
        trf_fprintf(f, rendergraph->space->dims, &prismel_trf->trf);
            fprintf(f, " %i\n", prismel_trf->color);
    }

    fprintf(f, "%s  rendergraph_trf_list:\n", spaces);
    for(rendergraph_trf_t *rendergraph_trf = rendergraph->rendergraph_trf_list;
        rendergraph_trf != NULL; rendergraph_trf = rendergraph_trf->next
    ){
        rendergraph_t *rendergraph = rendergraph_trf->rendergraph;
        fprintf(f, "%s    rendergraph_trf: %p ", spaces, rendergraph);
        trf_fprintf(f, rendergraph->space->dims, &rendergraph_trf->trf);
            fprintf(f, "\n");
    }

    fprintf(f, "%s  n_bitmaps: %i\n", spaces, rendergraph->n_bitmaps);
    fprintf(f, "%s  bitmaps:\n", spaces);
    for(int i = 0; i < rendergraph->n_bitmaps; i++){
        rendergraph_bitmap_t *bitmap = &rendergraph->bitmaps[i];
        fprintf(f, "%s    bitmap: cx=%i cy=%i w=%i h=%i surface=%p\n", spaces,
            bitmap->cx, bitmap->cy, bitmap->w, bitmap->h, bitmap->surface);
    }

    fprintf(f, "%s  boundbox: ", spaces); boundbox_fprintf(f,
        rendergraph->space->dims, rendergraph->boundbox); fprintf(f, "\n");
}

int rendergraph_create_bitmaps(rendergraph_t *rendergraph, int n_bitmaps){
    rendergraph_bitmap_t *bitmaps = calloc(n_bitmaps,
        sizeof(rendergraph_bitmap_t));
    if(bitmaps == NULL)return 1;
    rendergraph->n_bitmaps = n_bitmaps;
    rendergraph->bitmaps = bitmaps;
    return 0;
}

int rendergraph_push_rendergraph_trf(rendergraph_t *rendergraph){
    rendergraph_trf_t *rendergraph_trf =
        calloc(1, sizeof(rendergraph_trf_t));
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

int rendergraph_get_bitmap_i(rendergraph_t *rendergraph, trf_t *trf){
    int bitmap_i = rendergraph->space->rot_max * (trf->flip? 1: 0)
        + trf->rot;
    if(bitmap_i < 0 || bitmap_i >= rendergraph->n_bitmaps){
        fprintf(stderr, "%s:%s:%i: Bitmap index %i out of range\n",
            __FILE__, __func__, __LINE__, bitmap_i);
        return -1;
    }
    return bitmap_i;
}

int rendergraph_render_bitmap(rendergraph_t *rendergraph, int bitmap_i,
    SDL_Color pal[]
){
    int w = 16;
    int h = 16;
    int bpp = 32;

    rendergraph_bitmap_t *bitmap = &rendergraph->bitmaps[bitmap_i];
    SDL_Surface *surface = bitmap->surface;
    bitmap->surface = NULL;
    SDL_FreeSurface(surface);
    surface = SDL_CreateRGBSurface(0, w, h, bpp, 0, 0, 0, 0);
    bitmap->surface = surface;
    return 0;
}

int rendergraph_get_or_render_bitmap(rendergraph_t *rendergraph,
    rendergraph_bitmap_t **bitmap_ptr, trf_t *trf, SDL_Color pal[]
){
    int err;
    int bitmap_i = rendergraph_get_bitmap_i(rendergraph, trf);
    if(bitmap_i < 0)return 2;

    rendergraph_bitmap_t *bitmap = &rendergraph->bitmaps[bitmap_i];
    if(bitmap->surface == NULL){
        err = rendergraph_render_bitmap(rendergraph, bitmap_i, pal);
        if(err)return err;
    }

    if(bitmap_ptr != NULL)*bitmap_ptr = bitmap;
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

rendergraph_t *rendergraph_map_get(rendergraph_map_t *map,
    const char *name
){
    while(map != NULL){
        if(name_eq(map->name, name))return map->rgraph;
        map = map->next;
    }
    return NULL;
}



