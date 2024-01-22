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

enum rendergraph_child_type {
    RENDERGRAPH_CHILD_TYPE_PRISMEL,
    RENDERGRAPH_CHILD_TYPE_RGRAPH,
    RENDERGRAPH_CHILD_TYPE_LABEL,
    RENDERGRAPH_CHILD_TYPES
};

static const char *rendergraph_child_type_msg(int type){
    switch(type){
        case RENDERGRAPH_CHILD_TYPE_PRISMEL: return "prismel";
        case RENDERGRAPH_CHILD_TYPE_RGRAPH: return "shape";
        case RENDERGRAPH_CHILD_TYPE_LABEL: return "label";
        default: return "<unknown>";
    }
}

static const char *rendergraph_child_type_plural(int type){
    switch(type){
        case RENDERGRAPH_CHILD_TYPE_PRISMEL: return "prismels";
        case RENDERGRAPH_CHILD_TYPE_RGRAPH: return "shapes";
        case RENDERGRAPH_CHILD_TYPE_LABEL: return "labels";
        default: return "<unknown>";
    }
}

typedef struct rendergraph_child {
    trf_t trf;
    int frame_start;
    int frame_len;

    int type; /* enum rendergraph_child_type */
    union {
        struct {
            Uint8 color;

            /* Weakrefs */
            struct prismel *prismel;
        } prismel;
        struct {
            int frame_i;
            bool frame_i_additive;
            bool frame_i_reversed;
            int palmapper_n_applications;

            /* Weakrefs */
            struct rendergraph *rendergraph;
            struct palettemapper *palmapper;
        } rgraph;
        struct {
            const char *name;
        } label;
    } u;
} rendergraph_child_t;

typedef struct rendergraph_bitmap {
    bool pbox_calculated;
    position_box_t pbox;
    SDL_Surface *surface;
} rendergraph_bitmap_t;

typedef struct rendergraph_label {
    /* Represents one of rendergraph's descendants (rendergraph_child_t)
    which have type RENDERGRAPH_CHILD_TYPE_LABEL */

    const char *name; /* Points to a child->u.label.name */
    trf_t trf;
} rendergraph_label_t;

typedef struct label_mapping {
    /* Weakrefs: */
    const char *label_name;
    struct rendergraph *rgraph;
    int frame_i;
} label_mapping_t;

typedef struct rendergraph_frame {
    /* Represents information about individual frames of a rendergraph */

    ARRAY_DECL(struct rendergraph_label*, labels)
        /* Labels visible on this frame */
} rendergraph_frame_t;

typedef struct rendergraph {
    const char *name;
    ARRAY_DECL(struct rendergraph_child*, children)

    bool cache_bitmaps;

    int n_frames;
    struct rendergraph_frame *frames;
    bool labels_calculated;
        /* Whether frame->labels has been populated for all frames */

    int n_bitmaps;
    struct rendergraph_bitmap *bitmaps;
    boundbox_t boundbox;

    /* Weakrefs: */
    struct prismelrenderer *prend;
    vecspace_t *space;
    const char *animation_type;
    struct palettemapper *palmapper;
    struct rendergraph *copy_of;
        /* If not NULL, this rendergraph is a copy of another one.
        In particular, it does *NOT* own its children or frames, so
        should *NOT* modify or free them.
        However, it *DOES* own its bitmaps.
        The reason for doing this is basically for one rgraph to be
        the same as another, but with a (different) palmapper applied
        to it. */
} rendergraph_t;


extern const char rendergraph_animation_type_once[];
extern const char rendergraph_animation_type_cycle[];
extern const char rendergraph_animation_type_oscillate[];
extern const char *rendergraph_animation_types[];
extern const int rendergraph_n_frames_default;


struct prismelrenderer;
struct rendergraph_bitmap;

void rendergraph_cleanup(rendergraph_t *rendergraph);
int rendergraph_init(rendergraph_t *rendergraph, const char *name,
    struct prismelrenderer *prend, struct palettemapper *palmapper,
    const char *animation_type, int n_frames);
int rendergraph_copy(rendergraph_t *rendergraph, const char *name,
    rendergraph_t *copy_of);
void rendergraph_bitmap_dump(struct rendergraph_bitmap *bitmap, FILE *f,
    int i, int n_spaces, bool dump_surface);
void rendergraph_dump(rendergraph_t *rendergraph, FILE *f, int n_spaces,
    int dump_bitmaps);
int rendergraph_push_child(rendergraph_t *rendergraph,
    int type, rendergraph_child_t **child_ptr);
int rendergraph_get_bitmap_i(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i);
rendergraph_bitmap_t *rendergraph_get_bitmap(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i);
int rendergraph_calculate_labels(rendergraph_t *rgraph);
int rendergraph_calculate_bitmap_bounds(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i);
int rendergraph_render_to_surface(rendergraph_t *rendergraph,
    SDL_Surface *surface, SDL_Rect *dst_rect,
    rot_t rot, flip_t flip, int frame_i,
    SDL_Palette *pal);
int rendergraph_get_or_render_bitmap(rendergraph_t *rendergraph,
    rendergraph_bitmap_t **bitmap_ptr,
    rot_t rot, flip_t flip, int frame_i,
    SDL_Palette *pal);
struct prismelmapper;
int rendergraph_render(
    rendergraph_t *rgraph, SDL_Surface *surface,
    SDL_Palette *pal, struct prismelrenderer *prend,
    int x0, int y0, int zoom,
    vec_t pos, rot_t rot, flip_t flip, int frame_i,
    struct prismelmapper *mapper);
int rendergraph_render_with_labels(
    rendergraph_t *rgraph, SDL_Surface *surface,
    SDL_Palette *pal, struct prismelrenderer *prend,
    int x0, int y0, int zoom,
    vec_t pos, rot_t rot, flip_t flip, int frame_i,
    struct prismelmapper *mapper,
    int label_mappings_len, label_mapping_t **label_mappings);
int rendergraph_render_all_bitmaps(rendergraph_t *rgraph, SDL_Palette *pal);


#endif
