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


struct rendergraph;


/*****************
 * LABEL MAPPING *
 *****************/

typedef struct label_mapping {
    /* Maps label names to rendergraphs. Used for body->label_mappings.
    Is passed to rendergraph_render_with_labels, for rendering.
    Is set by body_vars_callback, i.e. when .fus files in anim/ set vars of body
    which were declared with the "label" property.
    So like:

        vars:
            nosave label "label:eye": null

    ...and then:

        set myvar("label:eye"): $PREFIX NS "eye"

    ...make sense? Wheee! */

    /* Weakrefs: */
    const char *label_name;
    struct rendergraph *rgraph;
    int frame_i;
        /* Incremented by body_step */
} label_mapping_t;


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
    /* Information about a child rendergraph.
    The whole point of a "render graph" is to be a tree (graph) of stuff to
    render!.. so this is basically an outgoing edge, pointing to some other
    node.
    Except actually, the graph isn't just built out of rendergraph nodes; it
    also has prismels and labels, which are always leaves.
    So, rendergraph_child->type says whether this edge is pointing to another
    rgraph node, or a prismel or a label. */

    trf_t trf;
    int frame_start;
    int frame_len;

    int type; /* enum rendergraph_child_type */
    union {
        struct {
            /* A child prismel node, so a leaf of the rgraph tree. */

            Uint8 color;

            /* Weakrefs */
            struct prismel *prismel;
        } prismel;
        struct {
            /* A child rgraph node, so a branch of the rgraph tree. */

            int frame_i;
            bool frame_i_additive;
            bool frame_i_reversed;

            /* Weakrefs */
            struct rendergraph *rendergraph;
            struct palettemapper *palmapper;
        } rgraph;
        struct {
            /* A child label node... which is kind of like a dynamic child rgraph node,
            in the sense that a label is a named reference to an rgraph!..
            NOTE: these are not rendered directly, they are only ever used to populate
            rendergraph->frames[i].labels, which are what we can render.
            See: rendergraph_calculate_labels, rendergraph_render_with_labels */

            const char *name;

            /* Weakrefs */
            struct rendergraph *default_rgraph;
            int default_frame_i; /* frame offset when rendering default_rgraph */
        } label;
    } u;
} rendergraph_child_t;

typedef struct rendergraph_bitmap {
    /* A bitmap, representing a rendergraph rendered with some frame_i/rot/flip
    combination.
    See: rendergraph->bitmaps */
    bool pbox_calculated;
    position_box_t pbox;
    SDL_Surface *surface;
} rendergraph_bitmap_t;

typedef struct rendergraph_label {
    /* Represents one of the labels on a single frame of a rendergraph.
    See: rendergraph_frame->labels */

    const char *name;
        /* Points to a child->u.label.name, for some child in
        rendergraph->children */
    trf_t trf;

    /* Weakrefs: */
    struct rendergraph *default_rgraph;
    struct palettemapper *palmapper;
    int default_frame_i; /* frame offset when rendering default_rgraph */
} rendergraph_label_t;

typedef struct rendergraph_frame {
    /* Represents information about individual frames of a rendergraph */

    ARRAY_DECL(struct rendergraph_label*, labels)
        /* Labels visible on this frame
        NOTE: only populated if our parent rendergraph's
        labels_calculated == true */
} rendergraph_frame_t;

typedef struct rendergraph {
    const char *name;
    ARRAY_DECL(struct rendergraph_child*, children)
        /* NOTE: may not be owned, see shared_copy_of */

    bool cache_bitmaps;

    int n_frames;
    struct rendergraph_frame *frames;
        /* NOTE: may not be owned, see shared_copy_of */
    bool labels_calculated;
        /* Whether frame->labels has been populated for all frames */

    int n_bitmaps;
    struct rendergraph_bitmap *bitmaps;
        /* Pre-rendered bitmaps for this rendergraph under various frame/flip/rot
        combinations.
        The formula for the indices is something like:

            (frame_i * 2 + (flip? 1: 0)) * space->rot_max + rot

        See: rendergraph_get_bitmap_i, get_bitmap_i */
    boundbox_t boundbox;

    /* Weakrefs: */
    struct prismelrenderer *prend;
    vecspace_t *space;
    const char *animation_type;
    struct palettemapper *palmapper;
    struct rendergraph *shared_copy_of;
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

void label_mapping_cleanup(label_mapping_t *mapping);


void rendergraph_cleanup(rendergraph_t *rendergraph);
int rendergraph_init(rendergraph_t *rendergraph, const char *name,
    struct prismelrenderer *prend, struct palettemapper *palmapper,
    const char *animation_type, int n_frames);
int rendergraph_copy_for_palmapping(rendergraph_t *rendergraph, const char *name,
    rendergraph_t *shared_copy_of);
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
    struct prismelmapper *mapper, struct palettemapper *palmapper);
int rendergraph_render_with_labels(
    rendergraph_t *rgraph, SDL_Surface *surface,
    SDL_Palette *pal, struct prismelrenderer *prend,
    int x0, int y0, int zoom,
    vec_t pos, rot_t rot, flip_t flip, int frame_i,
    struct prismelmapper *mapper, struct palettemapper *palmapper,
    int label_mappings_len, label_mapping_t **label_mappings);
int rendergraph_render_all_bitmaps(rendergraph_t *rgraph, SDL_Palette *pal);


#endif
