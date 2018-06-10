
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "prismelrenderer.h"
#include "lexer.h"
#include "bounds.h"
#include "util.h"
#include "array.h"
#include "write.h"


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


int prismel_init(prismel_t *prismel, char *name, vecspace_t *space){
    prismel->name = name;
    return prismel_create_images(prismel, space);
}

void prismel_cleanup(prismel_t *prismel){
    free(prismel->name);

    for(int i = 0; i < prismel->n_images; i++){
        prismel_image_t *image = &prismel->images[i];
        ARRAY_FREE(prismel_image_line_t, *image, lines, (void))
    }
    free(prismel->images);
}

int prismel_create_images(prismel_t *prismel, vecspace_t *space){
    int n_images = get_n_bitmaps(space, 1);
    prismel_image_t *images = calloc(n_images, sizeof(prismel_image_t));
    if(images == NULL)return 1;
    prismel->n_images = n_images;
    prismel->images = images;

    for(int i = 0; i < n_images; i++){
        prismel_image_t *image = &images[i];
        ARRAY_INIT(*image, lines)
    }
    return 0;
}

int prismel_image_push_line(prismel_image_t *image, int x, int y, int w){
    ARRAY_PUSH_NEW(prismel_image_line_t, *image, lines, line)
    line->x = x;
    line->y = y;
    line->w = w;
    return 0;
}

void prismel_get_boundary_box(prismel_t *prismel, boundary_box_t *box,
    int bitmap_i
){
    static const int line_h = 1;
    prismel_image_t *image = &prismel->images[bitmap_i];

    boundary_box_clear(box);

    for(int i = 0; i < image->lines_len; i++){
        prismel_image_line_t *line = image->lines[i];

        boundary_box_t line_box;
        line_box.l = line->x;
        line_box.r = line->x + line->w;
        line_box.t = line->y;
        line_box.b = line->y + line_h;
        boundary_box_union(box, &line_box);
    }
}



/*******************
 * PRISMELRENDERER *
 *******************/

int prismelrenderer_init(prismelrenderer_t *renderer, vecspace_t *space){
    renderer->space = space;
    ARRAY_INIT(*renderer, prismels)
    ARRAY_INIT(*renderer, rendergraphs)
    ARRAY_INIT(*renderer, mappers)
    return 0;
}

void prismelrenderer_cleanup(prismelrenderer_t *renderer){
    ARRAY_FREE(prismel_t, *renderer, prismels, prismel_cleanup)
    ARRAY_FREE(rendergraph_t, *renderer, rendergraphs,
        rendergraph_cleanup)
    ARRAY_FREE(prismelmapper_t, *renderer, mappers,
        prismelmapper_cleanup)
}

void prismelrenderer_dump(prismelrenderer_t *renderer, FILE *f,
    bool dump_bitmap_surfaces
){
    fprintf(f, "prismelrenderer: %p\n", renderer);
    if(renderer == NULL)return;
    fprintf(f, "  space: %p\n", renderer->space);

    fprintf(f, "  prismels:\n");
    for(int i = 0; i < renderer->prismels_len; i++){
        prismel_t *prismel = renderer->prismels[i];
        fprintf(f, "    prismel: %p\n", prismel);
        fprintf(f, "      name: %s\n", prismel->name);
        fprintf(f, "      n_images: %i\n", prismel->n_images);
        fprintf(f, "      images:\n");
        for(int i = 0; i < prismel->n_images; i++){
            prismel_image_t *image = &prismel->images[i];
            fprintf(f, "        image: %p", image);
            for(int i = 0; i < image->lines_len; i++){
                prismel_image_line_t *line = image->lines[i];
                fprintf(f, " (% i % i % i)", line->x, line->y, line->w);
            }
            fprintf(f, "\n");
        }
    }

    fprintf(f, "  rendergraphs:\n");
    for(int i = 0; i < renderer->rendergraphs_len; i++){
        rendergraph_t *rgraph = renderer->rendergraphs[i];
        rendergraph_dump(rgraph, f, 4, dump_bitmap_surfaces);
    }

    fprintf(f, "  prismelmappers:\n");
    for(int i = 0; i < renderer->mappers_len; i++){
        prismelmapper_t *mapper = renderer->mappers[i];
        prismelmapper_dump(mapper, f, 4);
    }
}

int prismelrenderer_push_prismel(prismelrenderer_t *renderer, char *name,
    prismel_t **prismel_ptr
){
    int err;
    ARRAY_PUSH_NEW(prismel_t, *renderer, prismels, prismel)
    err = prismel_init(prismel, name, renderer->space);
    if(err)return err;
    *prismel_ptr = prismel;
    return 0;
}

prismel_t *prismelrenderer_get_prismel(prismelrenderer_t *renderer,
    char *name
){
    for(int i = 0; i < renderer->prismels_len; i++){
        prismel_t *prismel = renderer->prismels[i];
        if(strcmp(prismel->name, name) == 0)return prismel;
    }
    return NULL;
}

rendergraph_t *prismelrenderer_get_rendergraph(prismelrenderer_t *prend,
    const char *name
){
    for(int i = 0; i < prend->rendergraphs_len; i++){
        rendergraph_t *rgraph = prend->rendergraphs[i];
        if(!strcmp(rgraph->name, name))return rgraph;
    }
    return NULL;
}

prismelmapper_t *prismelrenderer_get_mapper(prismelrenderer_t *prend,
    const char *name
){
    for(int i = 0; i < prend->mappers_len; i++){
        prismelmapper_t *mapper = prend->mappers[i];
        if(!strcmp(mapper->name, name))return mapper;
    }
    return NULL;
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

int prismelrenderer_save(prismelrenderer_t *prend, const char *filename){
    int err;
    FILE *f = fopen(filename, "w");
    if(f == NULL)return 2;
    err = prismelrenderer_write(prend, f);
    if(err)return err;
    if(fclose(f))return 2;
    return 0;
}

int prismelrenderer_write(prismelrenderer_t *prend, FILE *f){
    int err;

    fprintf(f, "prismels:\n");
    for(int i = 0; i < prend->prismels_len; i++){
        prismel_t *prismel = prend->prismels[i];
        fprintf(f, "    ");
        fus_write_str(f, prismel->name);
        fprintf(f, ":\n");
        fprintf(f, "        images:\n");
        for(int i = 0; i < prismel->n_images; i++){
            prismel_image_t *image = &prismel->images[i];
            fprintf(f, "            :");
            for(int i = 0; i < image->lines_len; i++){
                prismel_image_line_t *line = image->lines[i];
                fprintf(f, " (% 3i % 3i % 3i)",
                    line->x, line->y, line->w);
            }
            fprintf(f, "\n");
        }
    }

    fprintf(f, "shapes:\n");
    for(int i = 0; i < prend->rendergraphs_len; i++){
        rendergraph_t *rgraph = prend->rendergraphs[i];
        fprintf(f, "    ");
        fus_write_str(f, rgraph->name);
        fprintf(f, ":\n");

        fprintf(f, "        animation: %s %i\n",
            rgraph->animation_type, rgraph->n_frames);

        if(rgraph->prismel_trfs != NULL){
            fprintf(f, "        prismels:\n");}
        for(int i = 0; i < rgraph->prismel_trfs_len; i++){
            prismel_trf_t *prismel_trf =
                rgraph->prismel_trfs[i];
            prismel_t *prismel = prismel_trf->prismel;
            trf_t *trf = &prismel_trf->trf;

            fprintf(f, "            : ");
            fus_write_str_padded(f, prismel->name, 7);
            fprintf(f, " (");
            fprintf(f, "% 3i", trf->add[0]);
            for(int i = 1; i < prend->space->dims; i++){
                fprintf(f, " % 3i", trf->add[i]);
            }
            fprintf(f, ") %2i %c %2i (%2i %2i)\n",
                trf->rot, trf->flip? 't': 'f',
                prismel_trf->color,
                prismel_trf->frame_start,
                prismel_trf->frame_len);
        }

        if(rgraph->rendergraph_trfs != NULL){
            fprintf(f, "        shapes:\n");}
        for(int i = 0; i < rgraph->rendergraph_trfs_len; i++){
            rendergraph_trf_t *rendergraph_trf =
                rgraph->rendergraph_trfs[i];
            rendergraph_t *rendergraph = rendergraph_trf->rendergraph;
            trf_t *trf = &rendergraph_trf->trf;

            fprintf(f, "            : ");
            fus_write_str_padded(f, rendergraph->name, 40);
            fprintf(f, " (");
            fprintf(f, "% 3i", trf->add[0]);
            for(int i = 1; i < prend->space->dims; i++){
                fprintf(f, " % 3i", trf->add[i]);
            }
            fprintf(f, ") %2i %c %2i%c (%2i %2i)\n",
                trf->rot, trf->flip? 't': 'f',
                rendergraph_trf->frame_i,
                rendergraph_trf->frame_i_additive? '+': ' ',
                rendergraph_trf->frame_start,
                rendergraph_trf->frame_len);
        }
        fprintf(f, "\n");
    }

    fprintf(f, "mappers:\n");
    for(int i = 0; i < prend->mappers_len; i++){
        prismelmapper_t *mapper = prend->mappers[i];

        fprintf(f, "    ");
        fus_write_str(f, mapper->name);
        fprintf(f, ":\n");

        fprintf(f, "        unit:");
        for(int i = 0; i < prend->space->dims; i++){
            fprintf(f, " % 3i", mapper->unit[i]);
        }
        fprintf(f, "\n");

        if(mapper->entries != NULL){
            fprintf(f, "        entries:\n");}
        for(int i = 0; i < mapper->entries_len; i++){
            prismelmapper_entry_t *entry = mapper->entries[i];
            fprintf(f, "            : ");
            fus_write_str(f, entry->prismel->name);
            fprintf(f, " -> ");
            fus_write_str(f, entry->rendergraph->name);
            fprintf(f, "\n");
        }
    }

    return 0;
}

int prismelrenderer_render_all_bitmaps(prismelrenderer_t *prend,
    SDL_Palette *pal, SDL_Renderer *renderer
){
    int err;
    for(int i = 0; i < prend->rendergraphs_len; i++){
        rendergraph_t *rgraph = prend->rendergraphs[i];
        for(int frame_i = 0; frame_i < rgraph->n_frames; frame_i++){
            for(int rot = 0; rot < rgraph->space->rot_max; rot++){
                err = rendergraph_get_or_render_bitmap(
                    rgraph, NULL, rot, false, frame_i, pal, renderer);
                if(err)return err;
            }
        }
    }
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
const char *rendergraph_animation_type_default =
    rendergraph_animation_type_cycle;
const int rendergraph_n_frames_default = 1;

void rendergraph_cleanup(rendergraph_t *rendergraph){
    free(rendergraph->name);

    ARRAY_FREE(prismel_trf_t, *rendergraph, prismel_trfs,
        (void))
    ARRAY_FREE(rendergraph_trf_t, *rendergraph, rendergraph_trfs,
        (void))

    for(int i = 0; i < rendergraph->n_bitmaps; i++){
        rendergraph_bitmap_t *bitmap = &rendergraph->bitmaps[i];
        SDL_FreeSurface(bitmap->surface);
        SDL_DestroyTexture(bitmap->texture);
    }
    free(rendergraph->bitmaps);
}

int rendergraph_init(rendergraph_t *rendergraph, char *name,
    vecspace_t *space,
    const char *animation_type, int n_frames
){
    int err;
    rendergraph->name = name;
    rendergraph->space = space;
    ARRAY_INIT(*rendergraph, prismel_trfs)
    ARRAY_INIT(*rendergraph, rendergraph_trfs)

    rendergraph->animation_type = animation_type;
    rendergraph->n_frames = n_frames;

    err = rendergraph_create_bitmaps(rendergraph);
    if(err)return err;
    boundbox_init(rendergraph->boundbox, space->dims);
    return 0;
}

void rendergraph_bitmap_dump(rendergraph_bitmap_t *bitmap, FILE *f,
    int i, int n_spaces, bool dump_bitmap_surfaces
){
    char spaces[20];
    get_spaces(spaces, 20, n_spaces);

    SDL_Surface *surface = bitmap->surface;
    fprintf(f, "%sbitmap %i: x=%i y=%i w=%i h=%i surface=%p texture=%p\n",
        spaces, i,
        bitmap->pbox.x, bitmap->pbox.y, bitmap->pbox.w, bitmap->pbox.h,
        surface, bitmap->texture);
    if(dump_bitmap_surfaces && surface != NULL){
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
    bool dump_bitmap_surfaces
){
    char spaces[20];
    get_spaces(spaces, 20, n_spaces);

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
        fprintf(f, " % 3i%c [% 3i % 3i]\n", rendergraph_trf->frame_i,
            rendergraph_trf->frame_i_additive? '+': ' ',
            rendergraph_trf->frame_start, rendergraph_trf->frame_len);
    }

    fprintf(f, "%s  animation_type: %s\n", spaces,
        rendergraph->animation_type);
    fprintf(f, "%s  n_frames: %i\n", spaces, rendergraph->n_frames);
    fprintf(f, "%s  n_bitmaps: %i\n", spaces, rendergraph->n_bitmaps);
    fprintf(f, "%s  bitmaps:\n", spaces);
    for(int i = 0; i < rendergraph->n_bitmaps; i++){
        rendergraph_bitmap_t *bitmap = &rendergraph->bitmaps[i];
        rendergraph_bitmap_dump(bitmap, f, i, n_spaces+4,
            dump_bitmap_surfaces);
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

int rendergraph_push_rendergraph_trf(rendergraph_t *rendergraph,
    rendergraph_trf_t **rendergraph_trf_ptr
){
    ARRAY_PUSH_NEW(rendergraph_trf_t, *rendergraph, rendergraph_trfs,
        rendergraph_trf)
    rendergraph_trf->frame_start = 0;
    rendergraph_trf->frame_len = -1;
    rendergraph_trf->frame_i = 0;
    rendergraph_trf->frame_i_additive = true;
    *rendergraph_trf_ptr = rendergraph_trf;
    return 0;
}

int rendergraph_push_prismel_trf(rendergraph_t *rendergraph,
    prismel_trf_t **prismel_trf_ptr
){
    ARRAY_PUSH_NEW(prismel_trf_t, *rendergraph, prismel_trfs,
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
    SDL_Palette *pal, SDL_Renderer *renderer
){
    int err;
    int bitmap_i = rendergraph_get_bitmap_i(rendergraph, rot, flip,
        frame_i);
    rendergraph_bitmap_t *bitmap = &rendergraph->bitmaps[bitmap_i];

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
        prismel_trf_t *prismel_trf = rendergraph->prismel_trfs[i];
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

    for(int i = 0; i < rendergraph->rendergraph_trfs_len; i++){
        rendergraph_trf_t *rendergraph_trf =
            rendergraph->rendergraph_trfs[i];
        rendergraph_t *rendergraph2 = rendergraph_trf->rendergraph;
        int frame_i2 = rendergraph_trf_get_frame_i(
            rendergraph_trf, frame_i);
        bool visible = rendergraph_trf_get_frame_visible(
            rendergraph_trf, rendergraph->n_frames, frame_i);
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

    /* Bits per pixel */
    int bpp = 8;

    /* Get rid of old bitmap, create new one */
    if(bitmap->surface != NULL){
        SDL_FreeSurface(bitmap->surface);
        bitmap->surface = NULL;
    }
    if(bitmap->texture != NULL){
        SDL_DestroyTexture(bitmap->texture);
        bitmap->texture = NULL;
    }
    SDL_Surface *surface = surface8_create(bitmap->pbox.w, bitmap->pbox.h,
        true, true, pal);
    if(surface == NULL)return 2;

    /* Fill new bitmap with transparent colour */
    SDL_LockSurface(surface);
    SDL_memset(surface->pixels, 0, surface->h * surface->pitch);

    /* Render prismels */
    for(int i = 0; i < rendergraph->prismel_trfs_len; i++){
        prismel_trf_t *prismel_trf = rendergraph->prismel_trfs[i];
        prismel_t *prismel = prismel_trf->prismel;
        bool visible = prismel_trf_get_frame_visible(
            prismel_trf, rendergraph->n_frames, frame_i);
        if(!visible)continue;

        Uint8 c = prismel_trf->color;

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
        for(int i = 0; i < image->lines_len; i++){
            prismel_image_line_t *line = image->lines[i];
            int x = line->x + bitmap->pbox.x + shift_x;
            int y = line->y + bitmap->pbox.y + shift_y;
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
        rendergraph_trf_t *rendergraph_trf =
            rendergraph->rendergraph_trfs[i];
        rendergraph_t *rendergraph2 = rendergraph_trf->rendergraph;
        int frame_i2 = rendergraph_trf_get_frame_i(
            rendergraph_trf, frame_i);
        bool visible = rendergraph_trf_get_frame_visible(
            rendergraph_trf, rendergraph->n_frames, frame_i);
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
    if(renderer != NULL && bitmap->pbox.w != 0 && bitmap->pbox.h != 0){
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
    SDL_Palette *pal, SDL_Renderer *renderer
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

int rendergraph_bitmap_get_texture(rendergraph_bitmap_t *bitmap,
    SDL_Renderer *renderer, SDL_Texture **texture_ptr
){
    if(bitmap->texture == NULL){
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer,
            bitmap->surface);
        RET_IF_SDL_NULL(texture);
        bitmap->texture = texture;
    }
    *texture_ptr = bitmap->texture;
    return 0;
}



/*****************
 * PRISMELMAPPER *
 *****************/

void prismelmapper_cleanup(prismelmapper_t *mapper){
    free(mapper->name);

    ARRAY_FREE(prismelmapper_entry_t, *mapper, entries,
        (void))
    ARRAY_FREE(prismelmapper_application_t, *mapper, applications,
        (void))
}

int prismelmapper_init(prismelmapper_t *mapper, char *name, vecspace_t *space){
    mapper->name = name;
    mapper->space = space;
    vec_zero(space->dims, mapper->unit);
    ARRAY_INIT(*mapper, entries)
    ARRAY_INIT(*mapper, applications)
    return 0;
}

void prismelmapper_dump(prismelmapper_t *mapper, FILE *f, int n_spaces){
    char spaces[20];
    get_spaces(spaces, 20, n_spaces);

    fprintf(f, "%sprismelmapper: %p\n", spaces, mapper);
    if(mapper == NULL)return;
    fprintf(f, "%s  name: %s\n", spaces, mapper->name);
    fprintf(f, "%s  space: %p\n", spaces, mapper->space);
    fprintf(f, "%s  unit: ", spaces);
    vec_fprintf(f, mapper->space->dims, mapper->unit);
    fprintf(f, "\n");

    fprintf(f, "%s  entries:\n", spaces);
    for(int i = 0; i < mapper->entries_len; i++){
        prismelmapper_entry_t *entry = mapper->entries[i];
        fprintf(f, "%s    %s -> %s\n", spaces,
            entry->prismel == NULL? "<NULL>": entry->prismel->name,
            entry->rendergraph == NULL? "<NULL>": entry->rendergraph->name);
    }

    fprintf(f, "%s  applications:\n", spaces);
    for(int i = 0; i < mapper->applications_len; i++){
        prismelmapper_application_t *application = mapper->applications[i];
        fprintf(f, "%s    %s -> %s\n", spaces,
            application->mapped_rgraph == NULL? "<NULL>":
                application->mapped_rgraph->name,
            application->resulting_rgraph == NULL? "<NULL>":
                application->resulting_rgraph->name);
    }
}

int prismelmapper_push_entry(prismelmapper_t *mapper,
    prismel_t *prismel, rendergraph_t *rendergraph
){
    ARRAY_PUSH_NEW(prismelmapper_entry_t, *mapper, entries, entry)
    entry->prismel = prismel;
    entry->rendergraph = rendergraph;
    return 0;
}

int prismelmapper_apply(prismelmapper_t *mapper, prismelrenderer_t *prend,
    rendergraph_t *mapped_rgraph,
    char *name, vecspace_t *space,
    rendergraph_t **rgraph_ptr
){
    int err;

    rendergraph_t *resulting_rgraph;

    /* Check whether this mapper has already been applied to this
    mapped_rgraph. If so, return the cached resulting_rgraph. */
    resulting_rgraph = prismelmapper_get_application(
        mapper, mapped_rgraph);
    if(resulting_rgraph != NULL){
        *rgraph_ptr = resulting_rgraph;
        return 0;
    }

    /* Create a new rendergraph */
    resulting_rgraph = calloc(1, sizeof(rendergraph_t));
    if(resulting_rgraph == NULL)return 1;
    err = rendergraph_init(resulting_rgraph, name, space,
        mapped_rgraph->animation_type, mapped_rgraph->n_frames);
    if(err)return err;

    /* Apply mapper to mapped_rgraph's prismels */
    for(int i = 0; i < mapped_rgraph->prismel_trfs_len; i++){
        prismel_trf_t *prismel_trf = mapped_rgraph->prismel_trfs[i];
        prismel_t *prismel = prismel_trf->prismel;

        bool entry_found = false;
        for(int i = 0; i < mapper->entries_len; i++){
            prismelmapper_entry_t *entry = mapper->entries[i];
            if(prismel == entry->prismel){
                rendergraph_trf_t *new_rendergraph_trf;
                err = rendergraph_push_rendergraph_trf(resulting_rgraph,
                    &new_rendergraph_trf);
                if(err)return err;
                new_rendergraph_trf->rendergraph = entry->rendergraph;
                new_rendergraph_trf->trf = prismel_trf->trf;
                vec_mul(mapper->space, new_rendergraph_trf->trf.add,
                    mapper->unit);
                new_rendergraph_trf->frame_start =
                    prismel_trf->frame_start;
                new_rendergraph_trf->frame_len =
                    prismel_trf->frame_len;

                entry_found = true;
                break;
            }
        }

        if(!entry_found){
            fprintf(stderr,
                "Prismel %s does not match any mapper entry\n",
                prismel->name);
            err = 2; return err;
        }
    }

    /* Apply mapper to mapped_rgraph's sub-rendergraphs */
    for(int i = 0; i < mapped_rgraph->rendergraph_trfs_len; i++){
        rendergraph_trf_t *rendergraph_trf =
            mapped_rgraph->rendergraph_trfs[i];
        rendergraph_t *rgraph = rendergraph_trf->rendergraph;

        /* Generate a name, e.g. "<curvy dodeca_sixth>" */
        int mapper_name_len = strlen(mapper->name);
        int rgraph_name_len = strlen(rgraph->name);
        int name_len = mapper_name_len + rgraph_name_len + 3;
        char *name = malloc(sizeof(*name) * (name_len + 1));
        if(name == NULL)return 1;
        name[0] = '<';
        strcpy(name + 1, mapper->name);
        name[1 + mapper_name_len] = ' ';
        strcpy(name + 1 + mapper_name_len + 1, rgraph->name);
        name[name_len - 1] = '>';
        name[name_len] = '\0';

        /* Recurse! */
        rendergraph_t *new_rgraph;
        err = prismelmapper_apply(mapper, prend,
            rgraph, name, space, &new_rgraph);
        if(err)return err;

        /* Add a rendergraph_trf to resulting_rgraph */
        rendergraph_trf_t *new_rendergraph_trf;
        err = rendergraph_push_rendergraph_trf(resulting_rgraph,
            &new_rendergraph_trf);
        if(err)return err;
        new_rendergraph_trf->rendergraph = new_rgraph;
        new_rendergraph_trf->trf = rendergraph_trf->trf;
        vec_mul(mapper->space, new_rendergraph_trf->trf.add,
            mapper->unit);
        new_rendergraph_trf->frame_i =
            rendergraph_trf->frame_i;
        new_rendergraph_trf->frame_i_additive =
            rendergraph_trf->frame_i_additive;
        new_rendergraph_trf->frame_start =
            rendergraph_trf->frame_start;
        new_rendergraph_trf->frame_len =
            rendergraph_trf->frame_len;
    }

    /* Cache this resulting_rgraph on the mapper in case it
    ever gets applied to the same mapped_rgraph again */
    err = prismelmapper_push_application(mapper,
        mapped_rgraph, resulting_rgraph);
    if(err)return err;

    /* Add the resulting_rgraph to the prismelrenderer, makes for
    easier debugging */
    ARRAY_PUSH(rendergraph_t, *prend, rendergraphs,
        resulting_rgraph)

    /* Success! */
    *rgraph_ptr = resulting_rgraph;
    return 0;
}

int prismelmapper_push_application(prismelmapper_t *mapper,
    rendergraph_t *mapped_rgraph, rendergraph_t *resulting_rgraph
){
    ARRAY_PUSH_NEW(prismelmapper_application_t, *mapper, applications,
        application)
    application->mapped_rgraph = mapped_rgraph;
    application->resulting_rgraph = resulting_rgraph;
    return 0;
}

rendergraph_t *prismelmapper_get_application(prismelmapper_t *mapper,
    rendergraph_t *mapped_rgraph
){
    for(int i = 0; i < mapper->applications_len; i++){
        prismelmapper_application_t *application = mapper->applications[i];
        if(application->mapped_rgraph == mapped_rgraph){
            return application->resulting_rgraph;}
    }
    return NULL;
}

