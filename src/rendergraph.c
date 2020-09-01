
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "prismelrenderer.h"
#include "rendergraph.h"
#include "lexer.h"
#include "bounds.h"
#include "util.h"
#include "sdl_util.h"
#include "array.h"
#include "write.h"






/***************
 * RENDERGRAPH *
 ***************/

const char rendergraph_animation_type_once[] = "once";
const char rendergraph_animation_type_cycle[] = "cycle";
const char rendergraph_animation_type_oscillate[] = "oscillate";
const char *rendergraph_animation_types[] = {
    rendergraph_animation_type_once,
    rendergraph_animation_type_cycle,
    rendergraph_animation_type_oscillate,
    NULL
};
const char *rendergraph_animation_type_default =
    rendergraph_animation_type_cycle;
const int rendergraph_n_frames_default = 1;

void rendergraph_cleanup(rendergraph_t *rendergraph){
    free(rendergraph->name);

    if(!rendergraph->copy_of){
        ARRAY_FREE_PTR(prismel_trf_t*, rendergraph->prismel_trfs,
            (void))
        ARRAY_FREE_PTR(rendergraph_trf_t*, rendergraph->rendergraph_trfs,
            (void))
    }

    for(int i = 0; i < rendergraph->n_bitmaps; i++){
        rendergraph_bitmap_t *bitmap = &rendergraph->bitmaps[i];
        SDL_FreeSurface(bitmap->surface);
    }
    free(rendergraph->bitmaps);
}

static int _rendergraph_init(rendergraph_t *rendergraph, char *name,
    prismelrenderer_t *prend, palettemapper_t *palmapper,
    const char *animation_type, int n_frames
){
    /* initialize everything except prismel_trfs and rendergraph_trfs */

    int err;

    vecspace_t *space = prend->space;

    rendergraph->name = name;
    rendergraph->prend = prend;
    rendergraph->space = space;

    rendergraph->animation_type = animation_type;
    rendergraph->n_frames = n_frames;

    rendergraph->palmapper = palmapper;
    rendergraph->copy_of = NULL;

    err = rendergraph_create_bitmaps(rendergraph);
    if(err)return err;
    boundbox_init(rendergraph->boundbox, space->dims);
    return 0;
}

int rendergraph_init(rendergraph_t *rendergraph, char *name,
    prismelrenderer_t *prend, palettemapper_t *palmapper,
    const char *animation_type, int n_frames
){
    int err;

    err = _rendergraph_init(rendergraph, name, prend, palmapper,
        animation_type, n_frames);
    if(err)return err;

    ARRAY_INIT(rendergraph->prismel_trfs)
    ARRAY_INIT(rendergraph->rendergraph_trfs)
    return 0;
}

int rendergraph_copy(rendergraph_t *rendergraph, char *name,
    rendergraph_t *copy_of
){
    int err;

    err = _rendergraph_init(rendergraph, name,
        copy_of->prend, copy_of->palmapper,
        copy_of->animation_type, copy_of->n_frames);
    if(err)return err;

    rendergraph->palmapper = copy_of->palmapper;
    rendergraph->copy_of = copy_of;

    rendergraph->prismel_trfs = copy_of->prismel_trfs;
    rendergraph->prismel_trfs_len = copy_of->prismel_trfs_len;
    rendergraph->rendergraph_trfs = copy_of->rendergraph_trfs;
    rendergraph->rendergraph_trfs_len = copy_of->rendergraph_trfs_len;

    return 0;
}

void rendergraph_bitmap_dump(rendergraph_bitmap_t *bitmap, FILE *f,
    int i, int n_spaces, bool dump_surface
){
    char spaces[MAX_SPACES];
    get_spaces(spaces, MAX_SPACES, n_spaces);

    SDL_Surface *surface = bitmap->surface;
    fprintf(f, "%sbitmap %i: x=%i y=%i w=%i h=%i surface=%p\n",
        spaces, i,
        bitmap->pbox.x, bitmap->pbox.y, bitmap->pbox.w, bitmap->pbox.h,
        surface);
    if(dump_surface && surface != NULL){
        SDL_LockSurface(surface);
        for(int y = 0; y < surface->h; y++){
            fprintf(f, "%s  ", spaces);
            for(int x = 0; x < surface->w; x++){
                Uint8 c = *surface8_get_pixel_ptr(surface, x, y);
                fprintf(f, " %02x", c);
            }
            fprintf(f, "\n");
        }
        SDL_UnlockSurface(surface);
    }
}

void rendergraph_dump(rendergraph_t *rendergraph, FILE *f, int n_spaces,
    int dump_bitmaps
){
    char spaces[MAX_SPACES];
    get_spaces(spaces, MAX_SPACES, n_spaces);

    fprintf(f, "%srendergraph: %p\n", spaces, rendergraph);
    if(rendergraph == NULL)return;
    if(rendergraph->name != NULL){
        fprintf(f, "%s  name: %s\n", spaces, rendergraph->name);
    }
    fprintf(f, "%s  space: %p\n", spaces, rendergraph->space);

    fprintf(f, "%s  prismel_trfs:\n", spaces);
    for(int i = 0; i < rendergraph->prismel_trfs_len; i++){
        prismel_trf_t *prismel_trf = rendergraph->prismel_trfs[i];
        prismel_t *prismel = prismel_trf->prismel;
        fprintf(f, "%s    prismel_trf: %7s ", spaces,
            prismel == NULL? "<NULL>": prismel->name);
        trf_fprintf(f, rendergraph->space->dims, &prismel_trf->trf);
            fprintf(f, " % 2i [% 3i % 3i]\n", prismel_trf->color,
            prismel_trf->frame_start, prismel_trf->frame_len);
    }

    fprintf(f, "%s  rendergraph_trfs:\n", spaces);
    for(int i = 0; i < rendergraph->rendergraph_trfs_len; i++){
        rendergraph_trf_t *rendergraph_trf =
            rendergraph->rendergraph_trfs[i];
        rendergraph_t *rendergraph = rendergraph_trf->rendergraph;
        fprintf(f, "%s    rendergraph_trf: %7s ", spaces,
            rendergraph == NULL? "<NULL>": rendergraph->name);
        trf_fprintf(f, rendergraph->space->dims, &rendergraph_trf->trf);
        if(rendergraph_trf->palmapper != NULL){
            fprintf(f, " %s", rendergraph_trf->palmapper->name);}
        fprintf(f, " % 3i%c [% 3i % 3i]\n", rendergraph_trf->frame_i,
            rendergraph_trf->frame_i_additive? '+': ' ',
            rendergraph_trf->frame_start, rendergraph_trf->frame_len);
    }

    fprintf(f, "%s  animation_type: %s\n", spaces,
        rendergraph->animation_type);
    fprintf(f, "%s  n_frames: %i\n", spaces, rendergraph->n_frames);
    fprintf(f, "%s  n_bitmaps: %i\n", spaces, rendergraph->n_bitmaps);
    if(dump_bitmaps > 0){
        fprintf(f, "%s  bitmaps:\n", spaces);
        for(int i = 0; i < rendergraph->n_bitmaps; i++){
            rendergraph_bitmap_t *bitmap = &rendergraph->bitmaps[i];
            rendergraph_bitmap_dump(bitmap, f, i, n_spaces+4,
                dump_bitmaps > 1);}}

    fprintf(f, "%s  boundbox: ", spaces); boundbox_fprintf(f,
        rendergraph->space->dims, rendergraph->boundbox); fprintf(f, "\n");
}

int rendergraph_create_bitmaps(rendergraph_t *rendergraph){
    int n_bitmaps = get_n_bitmaps(rendergraph->space, rendergraph->n_frames);
    rendergraph_bitmap_t *bitmaps = calloc(n_bitmaps,
        sizeof(rendergraph_bitmap_t));
    if(bitmaps == NULL)return 1;
    rendergraph->n_bitmaps = n_bitmaps;
    rendergraph->bitmaps = bitmaps;
    return 0;
}

int rendergraph_push_rendergraph_trf(rendergraph_t *rendergraph,
    rendergraph_trf_t **rendergraph_trf_ptr
){
    ARRAY_PUSH_NEW(rendergraph_trf_t*, rendergraph->rendergraph_trfs,
        rendergraph_trf)
    rendergraph_trf->frame_start = 0;
    rendergraph_trf->frame_len = -1;
    rendergraph_trf->frame_i = 0;
    rendergraph_trf->frame_i_additive = true;
    rendergraph_trf->palmapper_n_applications = 1;
    *rendergraph_trf_ptr = rendergraph_trf;
    return 0;
}

int rendergraph_push_prismel_trf(rendergraph_t *rendergraph,
    prismel_trf_t **prismel_trf_ptr
){
    ARRAY_PUSH_NEW(prismel_trf_t*, rendergraph->prismel_trfs,
        prismel_trf)
    prismel_trf->frame_start = 0;
    prismel_trf->frame_len = -1;
    *prismel_trf_ptr = prismel_trf;
    return 0;
}

int rendergraph_get_bitmap_i(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i
){
    return get_bitmap_i(rendergraph->space, rot, flip,
        rendergraph->n_frames, frame_i);
}

rendergraph_bitmap_t *rendergraph_get_bitmap(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i
){
    int bitmap_i = rendergraph_get_bitmap_i(rendergraph,
        rot, flip, frame_i);
    return &rendergraph->bitmaps[bitmap_i];
}

bool prismel_trf_get_frame_visible(prismel_trf_t *prismel_trf,
    int n_frames, int frame_i
){
    int frame_start = prismel_trf->frame_start;
    int frame_len = prismel_trf->frame_len;
    if(frame_len == -1)return true;
    return get_animated_frame_visible(
        n_frames, frame_start, frame_len, frame_i);
}

bool rendergraph_trf_get_frame_visible(rendergraph_trf_t *rendergraph_trf,
    int n_frames, int frame_i
){
    int frame_start = rendergraph_trf->frame_start;
    int frame_len = rendergraph_trf->frame_len;
    if(frame_len == -1)return true;
    return get_animated_frame_visible(
        n_frames, frame_start, frame_len, frame_i);
}

int rendergraph_trf_get_frame_i(rendergraph_trf_t *rendergraph_trf,
    int parent_frame_i
){
    int frame_i = rendergraph_trf->frame_i;
    rendergraph_t *rgraph = rendergraph_trf->rendergraph;
    if(rendergraph_trf->frame_i_additive)frame_i += parent_frame_i;
    return get_animated_frame_i(rgraph->animation_type,
        rgraph->n_frames, frame_i);
}

typedef struct {
    bool done, skip;
    prismel_trf_t *prismel_trf;
    prismel_t *prismel;
    trf_t trf2;
    int bitmap_i2;
    int shift_x, shift_y;
} rendergraph_prismel_trf_iter_t;

static int rendergraph_prismel_trf_iter(
    rendergraph_prismel_trf_iter_t *iter,
    rendergraph_t *rendergraph, int i,
    rot_t rot, flip_t flip, int frame_i
){
    int err;

    iter->prismel_trf = rendergraph->prismel_trfs[i];
    iter->prismel = iter->prismel_trf->prismel;
    bool visible = prismel_trf_get_frame_visible(
        iter->prismel_trf, rendergraph->n_frames, frame_i);
    if(!visible){iter->skip = true; return 0;}

    /* Combine the transformations: trf and prismel_trf->trf */
    iter->trf2 = iter->prismel_trf->trf;
    trf_apply(rendergraph->space, &iter->trf2,
        &(trf_t){flip, rot, {0, 0, 0, 0}});
    iter->bitmap_i2 = get_bitmap_i(rendergraph->space,
        iter->trf2.rot, iter->trf2.flip, 1, 0);
    rendergraph->space->vec_render(iter->trf2.add,
        &iter->shift_x, &iter->shift_y);

    return 0;
}

typedef struct {
    bool done, skip;
    rendergraph_trf_t *rendergraph_trf;
    rendergraph_t *rendergraph2;
    int frame_i2;
    trf_t trf2;
    int bitmap_i2;
    int shift_x, shift_y;
} rendergraph_rendergraph_trf_iter_t;

static int rendergraph_rendergraph_trf_iter(
    rendergraph_rendergraph_trf_iter_t *iter,
    rendergraph_t *rendergraph, int i,
    rot_t rot, flip_t flip, int frame_i
){
    int err;

    iter->rendergraph_trf = rendergraph->rendergraph_trfs[i];
    iter->rendergraph2 = iter->rendergraph_trf->rendergraph;
    iter->frame_i2 = rendergraph_trf_get_frame_i(
        iter->rendergraph_trf, frame_i);
    bool visible = rendergraph_trf_get_frame_visible(
        iter->rendergraph_trf, rendergraph->n_frames, frame_i);
    if(!visible){iter->skip = true; return 0;}

    /* Combine the transformations: trf and prismel_trf->trf */
    iter->trf2 = iter->rendergraph_trf->trf;
    trf_apply(rendergraph->space, &iter->trf2,
        &(trf_t){flip, rot, {0, 0, 0, 0}});
    iter->bitmap_i2 = rendergraph_get_bitmap_i(iter->rendergraph2,
        iter->trf2.rot, iter->trf2.flip, iter->frame_i2);
    rendergraph->space->vec_render(iter->trf2.add,
        &iter->shift_x, &iter->shift_y);

    return 0;
}

int rendergraph_calculate_bitmap_bounds(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i
){
    /* Looks up bitmap for given rot, flip, frame_i.
    Immediately exits if bitmap->pbox_calculated.
    Otherwise, calculates bitmap->pbox, recursively doing the
    same for all rendergraph's rendergraph_trfs. */

    int err;
    rendergraph_bitmap_t *bitmap = rendergraph_get_bitmap(rendergraph,
        rot, flip, frame_i);

    /* Already calculated bitmap->pbox, early exit */
    if(bitmap->pbox_calculated)return 0;

    /* bitmap->pbox should be the union of its sub-bitmap's pboxes.
    (I mean the set-theoretic union, like the "OR" of Venn diagrams.
    And by sub-bitmaps I mean the bitmaps of
    rendergraph->rendergraph_trfs.)
    It's easy to do unions with boundary_box_t, so we use one of those
    as an "accumulator" while iterating through sub-bitmaps.
    We will convert it back to a position_box_t when we store it in
    bitmap->pbox later. */
    boundary_box_t bbox;

    /* NOTE: Clearing sets all values to zero, which basically describes
    a single point at the origin. But it would actually make
    sense to have a separate "empty" state.
    Because e.g. if a rendergraph consists of a single prismel whose
    boundary does not contain the origin, we will end up unioning
    that with a boundary consisting of just the origin.
    Whereas it would have been "less wasteful" to just use the
    prismel's boundary. */
    boundary_box_clear(&bbox);

    for(int i = 0; i < rendergraph->prismel_trfs_len; i++){
        rendergraph_prismel_trf_iter_t iter = {0};
        err = rendergraph_prismel_trf_iter(&iter,
            rendergraph, i, rot, flip, frame_i);
        if(err)return err;
        if(iter.done)break;
        if(iter.skip)continue;

        /* Calculate & union prismel's bbox into our "accumulating" bbox */
        boundary_box_t bbox2;
        prismel_get_boundary_box(iter.prismel, &bbox2, iter.bitmap_i2);
        boundary_box_shift(&bbox2, iter.shift_x, iter.shift_y);
        boundary_box_union(&bbox, &bbox2);
    }

    for(int i = 0; i < rendergraph->rendergraph_trfs_len; i++){
        rendergraph_rendergraph_trf_iter_t iter = {0};
        err = rendergraph_rendergraph_trf_iter(&iter,
            rendergraph, i, rot, flip, frame_i);
        if(err)return err;
        if(iter.done)break;
        if(iter.skip)continue;

        /* Calculate bounds for sub-bitmap for this rendergraph_trf */
        err = rendergraph_calculate_bitmap_bounds(iter.rendergraph2,
            iter.trf2.rot, iter.trf2.flip, iter.frame_i2);
        if(err)return err;

        /* Union sub-bitmap's bbox into our "accumulating" bbox */
        rendergraph_bitmap_t *bitmap2 =
            &iter.rendergraph2->bitmaps[iter.bitmap_i2];
        boundary_box_t bbox2;
        boundary_box_from_position_box(&bbox2, &bitmap2->pbox);
        boundary_box_shift(&bbox2, iter.shift_x, iter.shift_y);
        boundary_box_union(&bbox, &bbox2);
    }

    /* Store "accumulated" bbox on bitmap */
    position_box_from_boundary_box(&bitmap->pbox, &bbox);
    bitmap->pbox_calculated = true;

    return 0;
}

int rendergraph_render_bitmap(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i,
    SDL_Palette *pal
){
    int err;
    rendergraph_bitmap_t *bitmap = rendergraph_get_bitmap(rendergraph,
        rot, flip, frame_i);

    /* Calculate our bounds */
    err = rendergraph_calculate_bitmap_bounds(rendergraph,
        rot, flip, frame_i);
    if(err)return err;

#ifdef GEOM_DEBUG_RENDERING_RGRAPH
    printf("Rendering rgraph: \"%s\" rot=%i flip=%c frame_i=%i\n",
        rendergraph->name, rot, flip? 'y': 'n', frame_i);
#endif

    /* Get rid of old bitmap, create new one */
    if(bitmap->surface != NULL){
        SDL_FreeSurface(bitmap->surface);
        bitmap->surface = NULL;
    }
    SDL_Surface *surface = surface8_create(bitmap->pbox.w, bitmap->pbox.h,
        false, true, pal);
    if(surface == NULL)return 2;

    /* Fill new bitmap with transparent colour */
    SDL_LockSurface(surface);
    SDL_memset(surface->pixels, 0, surface->h * surface->pitch);

    SDL_Rect dst_rect = {0, 0, bitmap->pbox.w, bitmap->pbox.h};
    err = rendergraph_render_to_surface(rendergraph, surface, &dst_rect,
        rot, flip, frame_i, pal);
    if(err)return err;

    bitmap->surface = surface;
    return 0;
}


int rendergraph_render_to_surface(rendergraph_t *rendergraph,
    SDL_Surface *surface, SDL_Rect *dst_rect,
    rot_t rot, flip_t flip, int frame_i,
    SDL_Palette *pal
){
    int err;
    rendergraph_bitmap_t *bitmap = rendergraph_get_bitmap(rendergraph,
        rot, flip, frame_i);

    /* NOTE: We seem to assume that bitmap's pbox has already been
    calculated, probably because this function was originally refactored
    out of rendergraph_render_bitmap which calls
    rendergraph_calculate_bitmap_bounds up front.
    But these days we can call rendergraph_calculate_bitmap_bounds
    and have it exit early if bounds were already calculated, so
    shouldn't we just rely on that instead?.. */

    /* Render prismels */
    for(int i = 0; i < rendergraph->prismel_trfs_len; i++){
        rendergraph_prismel_trf_iter_t iter = {0};
        err = rendergraph_prismel_trf_iter(&iter,
            rendergraph, i, rot, flip, frame_i);
        if(err)return err;
        if(iter.done)break;
        if(iter.skip)continue;

        /* Draw prismel's image onto SDL surface */
        Uint8 c = iter.prismel_trf->color;
        if(rendergraph->palmapper){
            c = palettemapper_apply_to_color(rendergraph->palmapper, c);
        }
        prismel_image_t *image = &iter.prismel->images[iter.bitmap_i2];
        for(int i = 0; i < image->lines_len; i++){
            prismel_image_line_t *line = image->lines[i];
            int x = dst_rect->x + line->x + bitmap->pbox.x + iter.shift_x;
            int y = dst_rect->y + line->y + bitmap->pbox.y + iter.shift_y;
            Uint8 *p = surface8_get_pixel_ptr(surface, x, y);
            for(int xx = 0; xx < line->w; xx++){
                p[xx] = c;
            }
        }
    }

    /* Unlock surface so we can blit to it */
    SDL_UnlockSurface(surface);

    /* Render sub-rendergraphs */
    for(int i = 0; i < rendergraph->rendergraph_trfs_len; i++){
        rendergraph_rendergraph_trf_iter_t iter = {0};
        err = rendergraph_rendergraph_trf_iter(&iter,
            rendergraph, i, rot, flip, frame_i);
        if(err)return err;
        if(iter.done)break;
        if(iter.skip)continue;

        rendergraph_bitmap_t *bitmap2 = rendergraph_get_bitmap(
            iter.rendergraph2,
            iter.trf2.rot, iter.trf2.flip, iter.frame_i2);
        SDL_Rect dst_rect2 = {
            dst_rect->x + bitmap->pbox.x + iter.shift_x - bitmap2->pbox.x,
            dst_rect->y + bitmap->pbox.y + iter.shift_y - bitmap2->pbox.y,
            bitmap2->pbox.w,
            bitmap2->pbox.h
        };

        if(!rendergraph->prend->cache_bitmaps){
            /* Recurse and continue */
            err = rendergraph_render_to_surface(iter.rendergraph2, surface,
                &dst_rect2,
                iter.trf2.rot, iter.trf2.flip, iter.frame_i2, pal);
            if(err)return err;
            continue;
        }

        /* Get or render sub-bitmap for this rendergraph_trf */
        /* NOTE: &bitmap2 should be redundant, it should result the same
        pointer value we already had */
        err = rendergraph_get_or_render_bitmap(iter.rendergraph2,
            &bitmap2, iter.trf2.rot, iter.trf2.flip, iter.frame_i2, pal);

        /* Blit sub-bitmap's surface onto ours */
        SDL_Surface *surface2 = bitmap2->surface;
        palettemapper_t *palmapper = iter.rendergraph_trf->palmapper;
        Uint8 *table = NULL;
        Uint8 _table[256];
        if(rendergraph->palmapper){
            for(int i = 0; i < 256; i++){
                _table[i] = rendergraph->palmapper->table[i];
            }
            table = _table;
        }
        if(palmapper){
            int n_applications =
                iter.rendergraph_trf->palmapper_n_applications;
            if(!table){
                for(int i = 0; i < 256; i++)_table[i] = i;
                table = _table;
            }
            for(int i = 0; i < n_applications; i++){
                palettemapper_apply_to_table(palmapper, table);
            }
        }
        RET_IF_SDL_NZ(SDL_PaletteMappedBlit(bitmap2->surface, NULL,
            surface, &dst_rect2, table));
    }

    /* LET'S GO */
    return 0;
}

int rendergraph_get_or_render_bitmap(rendergraph_t *rendergraph,
    rendergraph_bitmap_t **bitmap_ptr,
    rot_t rot, flip_t flip, int frame_i,
    SDL_Palette *pal
){
    int err;
    rendergraph_bitmap_t *bitmap = rendergraph_get_bitmap(rendergraph,
        rot, flip, frame_i);

    if(bitmap->surface == NULL){
        err = rendergraph_render_bitmap(rendergraph, rot, flip, frame_i,
            pal);
        if(err)return err;
    }

    if(bitmap_ptr != NULL)*bitmap_ptr = bitmap;
    return 0;
}

int rendergraph_render(rendergraph_t *rgraph,
    SDL_Surface *surface,
    SDL_Palette *pal, prismelrenderer_t *prend,
    int x0, int y0, int zoom,
    vec_t pos, rot_t rot, flip_t flip, int frame_i,
    prismelmapper_t *mapper
){
    int err;

    bool cache_bitmaps = surface == NULL? true: prend->cache_bitmaps;

    int animated_frame_i = get_animated_frame_i(
        rgraph->animation_type, rgraph->n_frames, frame_i);

    static const bool MAPPER_ZOOM = true;
    if(MAPPER_ZOOM){
        prismelmapper_t *zoom_mapper =
            zoom == 2? prismelrenderer_get_mapper(prend, "double"):
            zoom == 3? prismelrenderer_get_mapper(prend, "triple"):
            zoom == 4? prismelrenderer_get_mapper(prend, "quadruple"):
            NULL;
        if(mapper == NULL)mapper = zoom_mapper;
        else if(zoom_mapper != NULL){
            err = prismelmapper_apply_to_mapper(zoom_mapper, prend, mapper,
                NULL, rgraph->space, &mapper);
            if(err)return err;
        }
    }

    vec_t mapped_pos;
    vec_cpy(rgraph->space->dims, mapped_pos, pos);
    if(mapper != NULL){
        err = prismelmapper_apply_to_rendergraph(mapper, prend, rgraph,
            NULL, rgraph->space, NULL, &rgraph);
        if(err)return err;

        vec_mul(mapper->space, mapped_pos, mapper->unit);
    }

    err = rendergraph_calculate_bitmap_bounds(rgraph,
        rot, flip, animated_frame_i);
    if(err)return err;

    rendergraph_bitmap_t *bitmap = rendergraph_get_bitmap(rgraph,
        rot, flip, animated_frame_i);

    /* Exit early for dimensionless bitmap */
    if(bitmap->pbox.w == 0 || bitmap->pbox.h == 0)return 0;

    int x, y;
    rgraph->space->vec_render(mapped_pos, &x, &y);

    int rect_zoom = MAPPER_ZOOM? 1: zoom;
    SDL_Rect dst_rect = {
        x0 + (x - bitmap->pbox.x) * rect_zoom,
        y0 + (y - bitmap->pbox.y) * rect_zoom,
        bitmap->pbox.w * rect_zoom,
        bitmap->pbox.h * rect_zoom
    };

    /* Exit early if rgraph wouldn't even show up on target surface. */
    SDL_Rect target_rect = {0, 0, surface->w, surface->h};
    if(!SDL_HasIntersection(&dst_rect, &target_rect))return 0;

    if(cache_bitmaps){
        /* Render rgraph and cache result on one of rgraph's bitmaps */

        /* NOTE: Passing &bitmap in the following call is unnecessary,
        it should end up being the same as what we got by calling
        rendergraph_get_bitmap above */
        err = rendergraph_get_or_render_bitmap(rgraph, &bitmap,
            rot, flip, animated_frame_i, pal);
        if(err)return err;
    }

    /* Finally render, blit, or copy the rgraph onto target surface */
    if(cache_bitmaps){
        RET_IF_SDL_NZ(SDL_BlitScaled(bitmap->surface, NULL,
            surface, &dst_rect));
    }else{
        err = rendergraph_render_to_surface(rgraph, surface, &dst_rect,
            rot, flip, animated_frame_i, pal);
        if(err)return err;
    }

    return 0;
}

int rendergraph_render_all_bitmaps(rendergraph_t *rgraph, SDL_Palette *pal){
    int err;
    for(int frame_i = 0; frame_i < rgraph->n_frames; frame_i++){
        for(int rot = 0; rot < rgraph->space->rot_max; rot++){
            err = rendergraph_get_or_render_bitmap(
                rgraph, NULL, rot, false, frame_i, pal);
            if(err)return err;
        }
    }
    return 0;
}


