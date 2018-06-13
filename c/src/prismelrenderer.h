#ifndef _PRISMEL_H_
#define _PRISMEL_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "geom.h"
#include "lexer.h"
#include "bounds.h"
#include "array.h"


/***********
 * GENERAL *
 ***********/

bool get_animated_frame_visible(int n_frames,
    int frame_start, int frame_len, int frame_i);
int get_animated_frame_i(const char *animation_type,
    int n_frames, int frame_i);
int get_n_bitmaps(vecspace_t *space, int n_frames);
int get_bitmap_i(vecspace_t *space, rot_t rot, flip_t flip,
    int n_frames, int frame_i);


/***********
 * PRISMEL *
 ***********/

typedef struct prismel_image_line {
    int x, y, w;
} prismel_image_line_t;

typedef struct prismel_image {
    ARRAY_DECL(struct prismel_image_line, lines)
} prismel_image_t;

typedef struct prismel {
    char *name;

    int n_images;
    struct prismel_image *images;
} prismel_t;


int prismel_init(prismel_t *prismel, char *name, vecspace_t *space);
void prismel_cleanup(prismel_t *prismel);
int prismel_create_images(prismel_t *prismel, vecspace_t *space);
int prismel_image_push_line(prismel_image_t *image, int x, int y, int w);
void prismel_get_boundary_box(prismel_t *prismel, boundary_box_t *box,
    int bitmap_i);



/*******************
 * PRISMELRENDERER *
 *******************/

typedef struct prismelrenderer {
    vecspace_t *space;
    ARRAY_DECL(struct prismel, prismels)
    ARRAY_DECL(struct rendergraph, rendergraphs)
    ARRAY_DECL(struct prismelmapper, mappers)
} prismelrenderer_t;



int prismelrenderer_init(prismelrenderer_t *renderer, vecspace_t *space);
void prismelrenderer_cleanup(prismelrenderer_t *renderer);
void prismelrenderer_dump(prismelrenderer_t *renderer, FILE *f,
    bool dump_bitmap_surfaces);
int prismelrenderer_push_prismel(prismelrenderer_t *renderer, char *name,
    prismel_t **prismel_ptr);
prismel_t *prismelrenderer_get_prismel(prismelrenderer_t *renderer,
    char *name);
int prismelrenderer_parse(prismelrenderer_t *prend, fus_lexer_t *lexer);
struct rendergraph *prismelrenderer_get_rendergraph(prismelrenderer_t *prend,
    const char *name);
struct prismelmapper *prismelrenderer_get_mapper(prismelrenderer_t *prend,
    const char *name);
int prismelrenderer_load(prismelrenderer_t *prend, const char *filename);
int prismelrenderer_save(prismelrenderer_t *prend, const char *filename);
int prismelrenderer_write(prismelrenderer_t *prend, FILE *f);
int prismelrenderer_render_all_bitmaps(prismelrenderer_t *prend,
    SDL_Palette *pal, SDL_Renderer *renderer);



/***************
 * RENDERGRAPH *
 ***************/

typedef struct prismel_trf {
    prismel_t *prismel;
    Uint8 color;
    trf_t trf;

    int frame_start;
    int frame_len;
} prismel_trf_t;

typedef struct rendergraph_trf {
    struct rendergraph *rendergraph;
    trf_t trf;
    int frame_i;
    bool frame_i_additive;

    int frame_start;
    int frame_len;
} rendergraph_trf_t;

typedef struct rendergraph_bitmap {
    position_box_t pbox;
    SDL_Surface *surface;
    SDL_Texture *texture;
} rendergraph_bitmap_t;

typedef struct rendergraph {
    char *name;
    vecspace_t *space;
    ARRAY_DECL(struct prismel_trf, prismel_trfs)
    ARRAY_DECL(struct rendergraph_trf, rendergraph_trfs)

    const char *animation_type;
    int n_frames;

    int n_bitmaps;
    rendergraph_bitmap_t *bitmaps;
    boundbox_t boundbox;
} rendergraph_t;


extern const char rendergraph_animation_type_once[];
extern const char rendergraph_animation_type_cycle[];
extern const char rendergraph_animation_type_oscillate[];
extern const char *rendergraph_animation_types[];
extern const char *rendergraph_animation_type_default;
extern const int rendergraph_n_frames_default;


void rendergraph_cleanup(rendergraph_t *rendergraph);
int rendergraph_init(rendergraph_t *rendergraph, char *name,
    vecspace_t *space,
    const char *animation_type, int n_frames);
void rendergraph_bitmap_dump(rendergraph_bitmap_t *bitmap, FILE *f,
    int i, int n_spaces, bool dump_bitmap_surfaces);
void rendergraph_dump(rendergraph_t *rendergraph, FILE *f, int n_spaces,
    bool dump_bitmap_surfaces);
int rendergraph_create_bitmaps(rendergraph_t *rendergraph);
int rendergraph_push_rendergraph_trf(rendergraph_t *rendergraph,
    rendergraph_trf_t **rendergraph_trf_ptr);
int rendergraph_push_prismel_trf(rendergraph_t *rendergraph,
    prismel_trf_t **prismel_trf_ptr);
int rendergraph_get_bitmap_i(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i);
int rendergraph_render_bitmap(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i,
    SDL_Palette *pal, SDL_Renderer *renderer);
int rendergraph_get_or_render_bitmap(rendergraph_t *rendergraph,
    rendergraph_bitmap_t **bitmap_ptr,
    rot_t rot, flip_t flip, int frame_i,
    SDL_Palette *pal, SDL_Renderer *renderer);
int rendergraph_bitmap_get_texture(rendergraph_bitmap_t *bitmap,
    SDL_Renderer *renderer, SDL_Texture **texture_ptr);



/*****************
 * PRISMELMAPPER *
 *****************/

typedef struct prismelmapper_application {
    rendergraph_t *mapped_rgraph;
    rendergraph_t *resulting_rgraph;
} prismelmapper_application_t;

typedef struct prismelmapper_mapplication {
    struct prismelmapper *mapped_mapper;
    struct prismelmapper *resulting_mapper;
} prismelmapper_mapplication_t;

typedef struct prismelmapper_entry {
    prismel_t *prismel;
    rendergraph_t *rendergraph;
} prismelmapper_entry_t;

typedef struct prismelmapper {
    char *name;
    vecspace_t *space;
    vec_t unit;
    ARRAY_DECL(prismelmapper_entry_t, entries)
    ARRAY_DECL(prismelmapper_application_t, applications)
    ARRAY_DECL(prismelmapper_mapplication_t, mapplications)
} prismelmapper_t;


void prismelmapper_cleanup(prismelmapper_t *mapper);
int prismelmapper_init(prismelmapper_t *mapper, char *name, vecspace_t *space);
void prismelmapper_dump(prismelmapper_t *mapper, FILE *f, int n_spaces);
int prismelmapper_push_entry(prismelmapper_t *mapper,
    prismel_t *prismel, rendergraph_t *rendergraph);
int prismelmapper_apply_to_rendergraph(prismelmapper_t *mapper,
    prismelrenderer_t *prend,
    rendergraph_t *mapped_rgraph,
    char *name, vecspace_t *space,
    rendergraph_t **rgraph_ptr);
int prismelmapper_apply_to_mapper(prismelmapper_t *mapper,
    prismelrenderer_t *prend,
    prismelmapper_t *mapped_mapper,
    char *name, vecspace_t *space,
    prismelmapper_t **mapper_ptr);
int prismelmapper_push_application(prismelmapper_t *mapper,
    rendergraph_t *mapped_rgraph, rendergraph_t *resulting_rgraph);
int prismelmapper_push_mapplication(prismelmapper_t *mapper,
    prismelmapper_t *mapped_mapper, prismelmapper_t *resulting_mapper);
rendergraph_t *prismelmapper_get_application(prismelmapper_t *mapper,
    rendergraph_t *mapped_rgraph);
prismelmapper_t *prismelmapper_get_mapplication(prismelmapper_t *mapper,
    prismelmapper_t *mapped_mapper);



#endif