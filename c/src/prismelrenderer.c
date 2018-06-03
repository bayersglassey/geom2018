
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "prismelrenderer.h"
#include "lexer.h"
#include "bounds.h"
#include "util.h"


/***********
 * GENERAL *
 ***********/

bool get_animated_frame_visible(int n_frames,
    int frame_start, int frame_len, int frame_i
){
    /* This function answers that age-old question:
    Is frame_i in the interval [frame_start, frame_start + frame_len)
    in the space of integers modulo n_frames? */

    if(frame_i >= frame_start && frame_i < frame_start + frame_len){
        return true;}

    /* Same check, modulo n_frames */
    frame_i += n_frames;
    if(frame_i >= frame_start && frame_i < frame_start + frame_len){
        return true;}

    return false;
}

int get_animated_frame_i(const char *animation_type,
    int n_frames, int frame_i
){
    if(animation_type == rendergraph_animation_type_cycle){
        frame_i = frame_i % n_frames;
    }else if(animation_type == rendergraph_animation_type_oscillate){
        frame_i = frame_i % (n_frames * 2);
        if(frame_i >= n_frames)frame_i = n_frames - (frame_i - n_frames) - 1;
    }else{
        fprintf(stderr, "Unsupported animation_type: %s\n",
            animation_type);
        return 0;
    }
    return frame_i;
}

int get_n_bitmaps(vecspace_t *space, int n_frames){
    return space->rot_max * n_frames;
}

int get_bitmap_i(vecspace_t *space, rot_t rot, flip_t flip,
    int n_frames, int frame_i
){
    int n_bitmaps = get_n_bitmaps(space, n_frames);
    rot = rot_contain(space->rot_max, rot);
    rot = rot_flip(space->rot_max, rot, flip);
    int bitmap_i = frame_i * space->rot_max + rot;
    return bitmap_i;
}

void get_spaces(char *spaces, int max_spaces, int n_spaces){
    if(n_spaces > max_spaces){
        fprintf(stderr, "%s: %s: Can't handle %i spaces - max %i\n",
            __FILE__, __func__, n_spaces, max_spaces);
        n_spaces = max_spaces;
    }
    for(int i = 0; i < n_spaces; i++)spaces[i] = ' ';
    spaces[n_spaces] = '\0';
}



/***********
 * PRISMEL *
 ***********/


void prismel_cleanup(prismel_t *prismel){
    free(prismel->name);

    for(int i = 0; i < prismel->n_images; i++){
        prismel_image_t *image = &prismel->images[i];
        prismel_image_line_t *line = image->line_list;
        while(line != NULL){
            prismel_image_line_t *next = line->next;
            free(line);
            line = next;
        }
    }
    free(prismel->images);
}

int prismel_create_images(prismel_t *prismel, vecspace_t *space){
    int n_images = get_n_bitmaps(space, 1);
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

void prismel_get_boundary_box(prismel_t *prismel, boundary_box_t *box,
    int bitmap_i
){
    static const int line_h = 1;
    prismel_image_t *image = &prismel->images[bitmap_i];

    boundary_box_clear(box);

    prismel_image_line_t *line = image->line_list;
    while(line != NULL){
        boundary_box_t line_box;
        line_box.l = line->x;
        line_box.r = line->x + line->w;
        line_box.t = line->y;
        line_box.b = line->y + line_h;
        boundary_box_union(box, &line_box);

        line = line->next;
    }
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

void prismelrenderer_cleanup(prismelrenderer_t *renderer){
    prismel_t *prismel = renderer->prismel_list;
    while(prismel != NULL){
        prismel_t *next = prismel->next;
        prismel_cleanup(prismel);
        free(prismel);
        prismel = next;
    }

    rendergraph_map_t *rendergraph_map = renderer->rendergraph_map;
    while(rendergraph_map != NULL){
        rendergraph_map_t *next = rendergraph_map->next;
        rendergraph_map_cleanup(rendergraph_map);
        free(rendergraph_map);
        rendergraph_map = next;
    }
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
    err = prismel_create_images(prismel, renderer->space);
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

rendergraph_t *prismelrenderer_get_rendergraph(prismelrenderer_t *prend,
    const char *name
){
    return rendergraph_map_get(prend->rendergraph_map, name);
}

int prismelrenderer_load(prismelrenderer_t *prend, const char *filename,
    vecspace_t *space
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text);
    if(err)return err;

    err = prismelrenderer_init(prend, space);
    if(err)return err;

    err = prismelrenderer_parse(prend, &lexer);
    if(err)return err;

    free(text);
    return 0;
}

int prismelrenderer_render_all_bitmaps(prismelrenderer_t *prend,
    SDL_Color pal[], SDL_Renderer *renderer
){
    int err;
    rendergraph_map_t *rgraph_map = prend->rendergraph_map;
    while(rgraph_map != NULL){
        rendergraph_t *rgraph = rgraph_map->rgraph;
        for(int frame_i = 0; frame_i < rgraph->n_frames; frame_i++){
            for(int rot = 0; rot < rgraph->space->rot_max; rot++){
                err = rendergraph_get_or_render_bitmap(
                    rgraph, NULL, rot, false, frame_i, pal, renderer);
                if(err)return err;
            }
        }
        rgraph_map = rgraph_map->next;
    }
    return 0;
}

int prismelrenderer_get_rendergraphs(prismelrenderer_t *prend,
    int *n_rgraphs_ptr, rendergraph_t ***rgraphs_ptr
){
    int n_rgraphs = 0;

    for(
        rendergraph_map_t *rgraph_map = prend->rendergraph_map;
        rgraph_map != NULL;
        rgraph_map = rgraph_map->next
    ){
        n_rgraphs++;
    }

    rendergraph_t **rgraphs = malloc(sizeof(*rgraphs) * n_rgraphs);
    if(rgraphs == NULL)return 1;

    int rgraph_i = 0;
    for(
        rendergraph_map_t *rgraph_map = prend->rendergraph_map;
        rgraph_map != NULL;
        rgraph_map = rgraph_map->next
    ){
        rgraphs[rgraph_i] = rgraph_map->rgraph;
        rgraph_i++;
    }

    *n_rgraphs_ptr = n_rgraphs;
    *rgraphs_ptr = rgraphs;
    return 0;
}





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

void rendergraph_cleanup(rendergraph_t *rendergraph){
    free(rendergraph->name);

    prismel_trf_t *prismel_trf = rendergraph->prismel_trf_list;
    while(prismel_trf != NULL){
        /* NOTE: prismel_trfs don't own their prismels! */
        /* prismel_cleanup(prismel_trf->pismel); */

        prismel_trf_t *next = prismel_trf->next;
        free(prismel_trf);
        prismel_trf = next;
    }

    rendergraph_trf_t *rendergraph_trf = rendergraph->rendergraph_trf_list;
    while(rendergraph_trf != NULL){
        /* NOTE: rendergraph_trfs don't own their rendergraphs! */
        /* rendergraph_cleanup(rendergraph_trf->rendergraph); */

        rendergraph_trf_t *next = rendergraph_trf->next;
        free(rendergraph_trf);
        rendergraph_trf = next;
    }

    for(int i = 0; i < rendergraph->n_bitmaps; i++){
        rendergraph_bitmap_t *bitmap = &rendergraph->bitmaps[i];
        SDL_FreeSurface(bitmap->surface);
        SDL_DestroyTexture(bitmap->texture);
    }
    free(rendergraph->bitmaps);
}

int rendergraph_init(rendergraph_t *rendergraph, vecspace_t *space,
    const char *animation_type, int n_frames
){
    int err;
    rendergraph->space = space;
    rendergraph->prismel_trf_list = NULL;
    rendergraph->rendergraph_trf_list = NULL;

    rendergraph->animation_type = animation_type;
    rendergraph->n_frames = n_frames;

    err = rendergraph_create_bitmaps(rendergraph);
    if(err)return err;
    boundbox_init(rendergraph->boundbox, space->dims);
    return 0;
}

void rendergraph_bitmap_dump(rendergraph_bitmap_t *bitmap, FILE *f,
    int i, int n_spaces
){
    char spaces[20];
    get_spaces(spaces, 20, n_spaces);

    SDL_Surface *surface = bitmap->surface;
    fprintf(f, "%sbitmap %i: x=%i y=%i w=%i h=%i surface=%p\n",
        spaces, i,
        bitmap->pbox.x, bitmap->pbox.y, bitmap->pbox.w, bitmap->pbox.h,
        surface);
    if(surface != NULL){
        SDL_LockSurface(surface);
        for(int y = 0; y < surface->h; y++){
            fprintf(f, "%s  ", spaces);
            for(int x = 0; x < surface->w; x++){
                Uint32 c = *surface_get_pixel_ptr(surface, x, y);
                fprintf(f, " %08x", c);
            }
            fprintf(f, "\n");
        }
        SDL_UnlockSurface(surface);
    }
}

void rendergraph_dump(rendergraph_t *rendergraph, FILE *f, int n_spaces){
    char spaces[20];
    get_spaces(spaces, 20, n_spaces);

    fprintf(f, "%srendergraph: %p\n", spaces, rendergraph);
    if(rendergraph == NULL)return;
    if(rendergraph->name != NULL){
        fprintf(f, "%s  name: %s\n", spaces, rendergraph->name);
    }
    fprintf(f, "%s  space: %p\n", spaces, rendergraph->space);

    fprintf(f, "%s  prismel_trf_list:\n", spaces);
    for(prismel_trf_t *prismel_trf = rendergraph->prismel_trf_list;
        prismel_trf != NULL; prismel_trf = prismel_trf->next
    ){
        prismel_t *prismel = prismel_trf->prismel;
        fprintf(f, "%s    prismel_trf: %7s ", spaces,
            prismel == NULL? "NULL": prismel->name);
        trf_fprintf(f, rendergraph->space->dims, &prismel_trf->trf);
            fprintf(f, " % 2i [% 3i % 3i]\n", prismel_trf->color,
            prismel_trf->frame_start, prismel_trf->frame_len);
    }

    fprintf(f, "%s  rendergraph_trf_list:\n", spaces);
    for(
        rendergraph_trf_t *rendergraph_trf =
            rendergraph->rendergraph_trf_list;
        rendergraph_trf != NULL;
        rendergraph_trf = rendergraph_trf->next
    ){
        rendergraph_t *rendergraph = rendergraph_trf->rendergraph;
        fprintf(f, "%s    rendergraph_trf: %p ", spaces, rendergraph);
        trf_fprintf(f, rendergraph->space->dims, &rendergraph_trf->trf);
        fprintf(f, " % 3i%c [% 3i % 3i]\n", rendergraph_trf->frame_i,
            rendergraph_trf->frame_i_additive? '+': ' ',
            rendergraph_trf->frame_start, rendergraph_trf->frame_len);
    }

    fprintf(f, "%s  animation_type: %s\n", spaces, rendergraph->animation_type);
    fprintf(f, "%s  n_frames: %i\n", spaces, rendergraph->n_frames);
    fprintf(f, "%s  n_bitmaps: %i\n", spaces, rendergraph->n_bitmaps);
    fprintf(f, "%s  bitmaps:\n", spaces);
    for(int i = 0; i < rendergraph->n_bitmaps; i++){
        rendergraph_bitmap_t *bitmap = &rendergraph->bitmaps[i];
        rendergraph_bitmap_dump(bitmap, f, i, n_spaces+4);
    }

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

int rendergraph_push_rendergraph_trf(rendergraph_t *rendergraph){
    rendergraph_trf_t *rendergraph_trf =
        calloc(1, sizeof(rendergraph_trf_t));
    if(rendergraph_trf == NULL)return 1;
    rendergraph_trf->frame_start = 0;
    rendergraph_trf->frame_len = -1;
    rendergraph_trf->frame_i = 0;
    rendergraph_trf->frame_i_additive = true;
    rendergraph_trf->next = rendergraph->rendergraph_trf_list;
    rendergraph->rendergraph_trf_list = rendergraph_trf;
    return 0;
}

int rendergraph_push_prismel_trf(rendergraph_t *rendergraph){
    prismel_trf_t *prismel_trf = calloc(1, sizeof(prismel_trf_t));
    if(prismel_trf == NULL)return 1;
    prismel_trf->frame_start = 0;
    prismel_trf->frame_len = -1;
    prismel_trf->next = rendergraph->prismel_trf_list;
    rendergraph->prismel_trf_list = prismel_trf;
    return 0;
}

int rendergraph_get_bitmap_i(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i
){
    return get_bitmap_i(rendergraph->space, rot, flip,
        rendergraph->n_frames, frame_i);
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

int rendergraph_render_bitmap(rendergraph_t *rendergraph,
    rot_t rot, flip_t flip, int frame_i,
    SDL_Color pal[], SDL_Renderer *renderer
){
    int err;
    int bitmap_i = rendergraph_get_bitmap_i(rendergraph, rot, flip,
        frame_i);
    rendergraph_bitmap_t *bitmap = &rendergraph->bitmaps[bitmap_i];

    /* bitmap->pbox should be the union of its sub-bitmap's pboxes.
    (I mean the set-theoretic union, like the "OR" of Venn diagrams.
    And by sub-bitmaps I mean the bitmaps of
    rendergraph->rendergraph_trf_list.)
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

    for(
        prismel_trf_t *prismel_trf = rendergraph->prismel_trf_list;
        prismel_trf != NULL;
        prismel_trf = prismel_trf->next
    ){
        prismel_t *prismel = prismel_trf->prismel;
        bool visible = prismel_trf_get_frame_visible(
            prismel_trf, rendergraph->n_frames, frame_i);
        if(!visible)continue;

        /* Combine the transformations: trf and prismel_trf->trf */
        trf_t trf2 = prismel_trf->trf;
        trf_apply(rendergraph->space, &trf2,
            &(trf_t){flip, rot, {0, 0, 0, 0}});
        int bitmap_i2 = get_bitmap_i(rendergraph->space,
            trf2.rot, trf2.flip, 1, 0);
        int shift_x, shift_y;
        rendergraph->space->vec_render(trf2.add, &shift_x, &shift_y);

        /* Calculate & union prismel's bbox into our "accumulating" bbox */
        boundary_box_t bbox2;
        prismel_get_boundary_box(prismel, &bbox2, bitmap_i2);
        boundary_box_shift(&bbox2, shift_x, shift_y);
        boundary_box_union(&bbox, &bbox2);
    }

    for(
        rendergraph_trf_t *rendergraph_trf =
            rendergraph->rendergraph_trf_list;
        rendergraph_trf != NULL;
        rendergraph_trf = rendergraph_trf->next
    ){
        rendergraph_t *rendergraph2 = rendergraph_trf->rendergraph;
        int frame_i2 = rendergraph_trf_get_frame_i(
            rendergraph_trf, frame_i);
        bool visible = rendergraph_trf_get_frame_visible(
            rendergraph_trf, rendergraph->n_frames, frame_i2);
        if(!visible)continue;

        /* Combine the transformations: trf and prismel_trf->trf */
        trf_t trf2 = rendergraph_trf->trf;
        trf_apply(rendergraph->space, &trf2,
            &(trf_t){flip, rot, {0, 0, 0, 0}});
        int shift_x, shift_y;
        rendergraph->space->vec_render(trf2.add, &shift_x, &shift_y);

        /* Get or render sub-bitmap for this rendergraph_trf */
        rendergraph_bitmap_t *bitmap2;
        err = rendergraph_get_or_render_bitmap(rendergraph2,
            &bitmap2, trf2.rot, trf2.flip, frame_i2, pal, renderer);
        if(err)return err;

        /* Union sub-bitmap's bbox into our "accumulating" bbox */
        boundary_box_t bbox2;
        boundary_box_from_position_box(&bbox2, &bitmap2->pbox);
        boundary_box_shift(&bbox2, shift_x, shift_y);
        boundary_box_union(&bbox, &bbox2);
    }

    /* Store "accumulated" bbox on bitmap */
    position_box_from_boundary_box(&bitmap->pbox, &bbox);

    /* Bytes per pixel */
    int bpp = 32;

    /* Get rid of old bitmap, create new one */
    SDL_FreeSurface(bitmap->surface);
    SDL_DestroyTexture(bitmap->texture);
    bitmap->surface = NULL;
    bitmap->texture = NULL;
    SDL_Surface *surface = surface_create(bitmap->pbox.w, bitmap->pbox.h,
        bpp, true, true);
    if(surface == NULL)return 2;

    /* Fill new bitmap with transparent colour */
    SDL_LockSurface(surface);
    SDL_memset(surface->pixels, 0, surface->h * surface->pitch);

    /* Render prismels */
    for(
        prismel_trf_t *prismel_trf = rendergraph->prismel_trf_list;
        prismel_trf != NULL;
        prismel_trf = prismel_trf->next
    ){
        prismel_t *prismel = prismel_trf->prismel;
        bool visible = prismel_trf_get_frame_visible(
            prismel_trf, rendergraph->n_frames, frame_i);
        if(!visible)continue;

        int color_i = prismel_trf->color;
        SDL_Color *color = &pal[color_i];
        Uint32 c = SDL_MapRGB(surface->format,
            color->r, color->g, color->b);

        /* Combine the transformations: trf and prismel_trf->trf */
        trf_t trf2 = prismel_trf->trf;
        trf_apply(rendergraph->space, &trf2,
            &(trf_t){flip, rot, {0, 0, 0, 0}});
        int bitmap_i2 = get_bitmap_i(rendergraph->space,
            trf2.rot, trf2.flip, 1, 0);
        int shift_x, shift_y;
        rendergraph->space->vec_render(trf2.add, &shift_x, &shift_y);

        /* Draw prismel's image onto SDL surface */
        prismel_image_t *image = &prismel->images[bitmap_i2];
        prismel_image_line_t *line = image->line_list;
        while(line != NULL){
            int x = line->x + bitmap->pbox.x + shift_x;
            int y = line->y + bitmap->pbox.y + shift_y;
            Uint32 *p = surface_get_pixel_ptr(surface, x, y);
            for(int xx = 0; xx < line->w; xx++){
                // p[xx] = c;
                p[xx] = (color_i+1) * 0x22222222; /* for debugging */
            }
            line = line->next;
        }

    }

    /* Unlock surface so we can blit to it */
    SDL_UnlockSurface(surface);

    /* Render sub-rendergraphs */
    for(
        rendergraph_trf_t *rendergraph_trf =
            rendergraph->rendergraph_trf_list;
        rendergraph_trf != NULL;
        rendergraph_trf = rendergraph_trf->next
    ){
        rendergraph_t *rendergraph2 = rendergraph_trf->rendergraph;
        int frame_i2 = rendergraph_trf_get_frame_i(
            rendergraph_trf, frame_i);
        bool visible = rendergraph_trf_get_frame_visible(
            rendergraph_trf, rendergraph->n_frames, frame_i2);
        if(!visible)continue;

        /* Combine the transformations: trf and prismel_trf->trf */
        trf_t trf2 = rendergraph_trf->trf;
        trf_apply(rendergraph->space, &trf2,
            &(trf_t){flip, rot, {0, 0, 0, 0}});
        int shift_x, shift_y;
        rendergraph->space->vec_render(trf2.add, &shift_x, &shift_y);

        /* Blit sub-bitmap's surface onto ours */
        int bitmap_i2 = rendergraph_get_bitmap_i(rendergraph2,
            trf2.rot, trf2.flip, frame_i2);
        rendergraph_bitmap_t *bitmap2 = &rendergraph2->bitmaps[bitmap_i2];
        SDL_Surface *surface2 = bitmap2->surface;

        SDL_Rect dst_rect = {
            bitmap->pbox.x + shift_x - bitmap2->pbox.x,
            bitmap->pbox.y + shift_y - bitmap2->pbox.y,
            bitmap2->pbox.w,
            bitmap2->pbox.h
        };
        RET_IF_SDL_NZ(SDL_BlitSurface(bitmap2->surface, NULL,
            surface, &dst_rect));
    }

    /* Create texture, if an SDL_Renderer was provided */
    SDL_Texture *texture = NULL;
    if(renderer != NULL){
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        RET_IF_SDL_NULL(texture);
    }

    /* LET'S GO */
    bitmap->surface = surface;
    bitmap->texture = texture;
    return 0;
}

int rendergraph_get_or_render_bitmap(rendergraph_t *rendergraph,
    rendergraph_bitmap_t **bitmap_ptr,
    rot_t rot, flip_t flip, int frame_i,
    SDL_Color pal[], SDL_Renderer *renderer
){
    int err;
    int bitmap_i = rendergraph_get_bitmap_i(rendergraph, rot, flip, frame_i);

    rendergraph_bitmap_t *bitmap = &rendergraph->bitmaps[bitmap_i];
    if(bitmap->surface == NULL){
        err = rendergraph_render_bitmap(rendergraph, rot, flip, frame_i,
            pal, renderer);
        if(err)return err;
    }

    if(bitmap_ptr != NULL)*bitmap_ptr = bitmap;
    return 0;
}



/*******************
 * RENDERGRAPH_MAP *
 *******************/


void rendergraph_map_cleanup(rendergraph_map_t *map){
    free(map->name);
    rendergraph_cleanup(map->rgraph);
    free(map->rgraph);
}

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



