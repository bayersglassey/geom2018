#ifndef _RENDERGRAPH_H_
#define _RENDERGRAPH_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "geom.h"
#include "lexer.h"
#include "bounds.h"
#include "array.h"


/***************
 * RENDERGRAPH *
 ***************/

typedef struct prismel_trf {
    struct prismel *prismel;
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

    struct palettemapper *palmapper;
    int palmapper_n_applications;

    int frame_start;
    int frame_len;
} rendergraph_trf_t;

typedef struct rendergraph_bitmap {
    bool pbox_calculated;
    position_box_t pbox;
    SDL_Surface *surface;
} rendergraph_bitmap_t;

typedef struct rendergraph {
    struct prismelrenderer *prend;
    char *name;
    vecspace_t *space;
    ARRAY_DECL(struct prismel_trf*, prismel_trfs)
    ARRAY_DECL(struct rendergraph_trf*, rendergraph_trfs)

    struct rendergraph *copy_of;
        /* If not NULL, this rendergraph is a copy of another one.
        In particular, it does *NOT* own its prismel_trfs and
        rendergraph_trfs, so should *NOT* modify or free them. */

    struct palettemapper *palmapper;

    const char *animation_type;
    int n_frames;

    int n_bitmaps;
    struct rendergraph_bitmap *bitmaps;
    boundbox_t boundbox;
} rendergraph_t;


extern const char rendergraph_animation_type_once[];
extern const char rendergraph_animation_type_cycle[];
extern const char rendergraph_animation_type_oscillate[];
extern const char *rendergraph_animation_types[];
extern const char *rendergraph_animation_type_default;
extern const int rendergraph_n_frames_default;


struct prismelrenderer;
struct rendergraph_bitmap;

void rendergraph_cleanup(rendergraph_t *rendergraph);
int rendergraph_init(rendergraph_t *rendergraph, char *name,
    struct prismelrenderer *prend, struct palettemapper *palmapper,
    const char *animation_type, int n_frames);
int rendergraph_copy(rendergraph_t *rendergraph, char *name,
    rendergraph_t *copy_of);
void rendergraph_bitmap_dump(struct rendergraph_bitmap *bitmap, FILE *f,
    int i, int n_spaces, bool dump_surface);
void rendergraph_dump(rendergraph_t *rendergraph, FILE *f, int n_spaces,
    int dump_bitmaps);
int rendergraph_create_bitmaps(rendergraph_t *rendergraph);
int rendergraph_push_rendergraph_trf(rendergraph_t *rendergraph,
    rendergraph_trf_t **rendergraph_trf_ptr);
int rendergraph_push_prismel_trf(rendergraph_t *rendergraph,
    prismel_trf_t **prismel_trf_ptr);
int rendergraph_get_bitmap_i(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i);
rendergraph_bitmap_t *rendergraph_get_bitmap(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i);
int rendergraph_calculate_bitmap_bounds(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i);
int rendergraph_render_to_surface(rendergraph_t *rendergraph,
    SDL_Surface *surface, SDL_Rect *dst_rect,
    rot_t rot, flip_t flip, int frame_i,
    SDL_Palette *pal);
int rendergraph_render_bitmap(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i,
    SDL_Palette *pal);
int rendergraph_get_or_render_bitmap(rendergraph_t *rendergraph,
    rendergraph_bitmap_t **bitmap_ptr,
    rot_t rot, flip_t flip, int frame_i,
    SDL_Palette *pal);
struct prismelmapper;
int rendergraph_render(rendergraph_t *rgraph,
    SDL_Surface *surface,
    SDL_Palette *pal, struct prismelrenderer *prend,
    int x0, int y0, int zoom,
    vec_t pos, rot_t rot, flip_t flip, int frame_i,
    struct prismelmapper *mapper);
int rendergraph_render_all_bitmaps(rendergraph_t *rgraph, SDL_Palette *pal);


#endif