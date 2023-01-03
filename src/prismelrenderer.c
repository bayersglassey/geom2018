
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "prismelrenderer.h"
#include "rendergraph.h"
#include "lexer.h"
#include "lexer_macros.h"
#include "bounds.h"
#include "util.h"
#include "array.h"
#include "write.h"
#include "font.h"
#include "geomfont.h"


/***********
 * GENERAL *
 ***********/

static char *_generate_mapped_name(const char *mapper_name, const char *mappee_name,
    const char *prefix, const char *separator, const char *suffix
){
    int prefix_len = strlen(prefix);
    int separator_len = strlen(separator);
    int suffix_len = strlen(suffix);
    int mapper_name_len = strlen(mapper_name);
    int mappee_name_len = strlen(mappee_name);
    int name_len = mapper_name_len + mappee_name_len
        + prefix_len + separator_len + suffix_len;
    char *name = malloc(sizeof(*name) * (name_len + 1));
    if(name == NULL)return NULL;

    char *s = name;
    strcpy(s, prefix);
    s += prefix_len;
    strcpy(s, mapper_name);
    s += mapper_name_len;
    strcpy(s, separator);
    s += separator_len;
    strcpy(s, mappee_name);
    s += mappee_name_len;
    strcpy(s, suffix);

    name[name_len] = '\0';
    return name;
}

char *generate_mapped_name(const char *mapper_name, const char *mappee_name){
    /* Generate a name, e.g. "<curvy dodeca_sixth>" */
    return _generate_mapped_name(mapper_name, mappee_name, "<", " ", ">");
}

char *generate_palmapped_name(const char *mapper_name, const char *mappee_name){
    /* Generate a name, e.g. "<pm:red dodeca_sixth>" */
    return _generate_mapped_name(mapper_name, mappee_name, "<pm:", " ", ">");
}

char *generate_indexed_name(const char *base_name, int i){
    /* Generate a name, e.g. "<shape 6>" */
    int base_name_len = strlen(base_name);
    int i_len = strlen_of_int(i);
    int name_len = base_name_len + i_len + 3;
    char *name = malloc(sizeof(*name) * (name_len + 1));
    if(name == NULL)return NULL;
    name[0] = '<';
    strcpy(name + 1, base_name);
    name[1 + base_name_len] = ' ';
    strncpy_of_int(name + 1 + base_name_len + 1, i, i_len);
    name[name_len - 1] = '>';
    name[name_len] = '\0';
    return name;
}

bool get_animated_frame_visible(int n_frames,
    int frame_start, int frame_len, int frame_i
){
    /* This function answers that age-old question:
    Is frame_i in the interval [frame_start, frame_start + frame_len)
    in the space of integers modulo n_frames?

    We seem to assume that frame_i >= 0 and < n_frames, such as is true
    of values returned by get_animated_frame_i.

    NOTE: frame_len == -1 is a magic value meaning "always visible" */

    if(frame_len == -1)return true;

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
    /* Returns a value >= 0 and < n_frames.
    Assumes frame_i >= 0. */

    if(animation_type == rendergraph_animation_type_once){
        /* Is this correct??? */
        if(frame_i >= n_frames)frame_i = n_frames - 1;
    }else if(animation_type == rendergraph_animation_type_cycle){
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
    /* The "* 2" is for flip (true or false) */
    return n_frames * 2 * space->rot_max;
}

int get_bitmap_i(vecspace_t *space, rot_t rot, flip_t flip,
    int n_frames, int frame_i
){
    int n_bitmaps = get_n_bitmaps(space, n_frames);
    rot = rot_contain(space->rot_max, rot);
    rot = rot_flip(space->rot_max, rot, flip);

    if(n_frames == 0)frame_i = 0;
    else{
        while(frame_i < 0)frame_i += n_frames;
        while(frame_i >= n_frames)frame_i -= n_frames;
    }

    int bitmap_i = (frame_i * 2 + (flip? 1: 0)) * space->rot_max + rot;
    return bitmap_i;
}


/***********
 * TILESET *
 ***********/

void tileset_cleanup(tileset_t *tileset){
    ARRAY_FREE_PTR(tileset_entry_t*, tileset->vert_entries, (void))
    ARRAY_FREE_PTR(tileset_entry_t*, tileset->edge_entries, (void))
    ARRAY_FREE_PTR(tileset_entry_t*, tileset->face_entries, (void))
}

int tileset_init(tileset_t *tileset, const char *name,
    vecspace_t *space, vec_t unit
){
    tileset->name = name;
    ARRAY_INIT(tileset->vert_entries)
    ARRAY_INIT(tileset->edge_entries)
    ARRAY_INIT(tileset->face_entries)

    vec_cpy(space->dims, tileset->unit, unit);
    tileset->space = space;

    return 0;
}

static const char TILESET_VERTS[] = "verts";
static const char TILESET_EDGES[] = "edges";
static const char TILESET_FACES[] = "faces";
static const char *WHEN_FACES_SOLID_RGRAPH_KEYS_VERT[] = {
    "none", "some", "all", NULL};
static const char *WHEN_FACES_SOLID_RGRAPH_KEYS_EDGE[] = {
    "neither", "bottom", "top", "both", NULL};
static int _tileset_parse_part(
    const char *part_name /* one of TILESET_VERTS, TILESET_EDGES, or TILESET_FACES */,
    tileset_entry_t ***entries_ptr,
    int *entries_len_ptr, int *entries_size_ptr,
    prismelrenderer_t *prend, fus_lexer_t *lexer
){
    /* WELCOME TO THE FUNCTION FROM HELL
    AREN'T WE GLAD WE ROLLED OUR OWN DATA FORMAT? */
    INIT

    /* The following hack is an indication that our array.h system
    of doing things was itself a hack.
    That is, we will use ARRAY_PUSH_NEW below, passing it the token
    "entries", and it expects the tokens "entries_len" and "entries_size"
    to also exist.
    So we create them here and populate them from pointers, then assign
    their values back through the pointers at end of function.
    It's weird, man... but it works! */
    tileset_entry_t **entries = *entries_ptr;
    int entries_len = *entries_len_ptr;
    int entries_size = *entries_size_ptr;

    GET(part_name)
    OPEN
    while(1){
        if(GOT(")"))break;

        char tile_c;
        GET_CHR(tile_c)

        OPEN
        ARRAY_PUSH_NEW(tileset_entry_t*, entries, entry)
        entry->type = TILESET_ENTRY_TYPE_ROTS;
        entry->n_rgraphs = 0;
        entry->tile_c = tile_c;
        entry->frame_offset = 0;

        if(GOT("frame_offset")){
            NEXT
            OPEN
            GET_INT(entry->frame_offset)
            CLOSE
        }

        const char **rgraph_keys = NULL;
        if(GOT("when_faces_solid")){
            NEXT
            if(part_name == TILESET_VERTS){
                rgraph_keys = WHEN_FACES_SOLID_RGRAPH_KEYS_VERT;
            }else if(part_name == TILESET_EDGES){
                rgraph_keys = WHEN_FACES_SOLID_RGRAPH_KEYS_EDGE;
            }else{
                fus_lexer_err_info(lexer);
                fprintf(stderr, "when_faces_solid only makes sense for "
                    "verts and edges!\n");
                return 2;
            }

            entry->type = TILESET_ENTRY_TYPE_WHEN_FACES_SOLID;
            OPEN
        }

        while(1){
            if(rgraph_keys){
                if(!GOT_NAME)break;
                const char *rgraph_key = rgraph_keys[entry->n_rgraphs];
                if(rgraph_key == NULL){
                    fus_lexer_err_info(lexer);
                    fprintf(stderr,
                        "Too many entries for when_faces_solid.\n");
                    return 2;
                }
                GET(rgraph_key)
                OPEN
            }else{
                if(!GOT_STR)break;
            }
            if(entry->n_rgraphs == TILESET_ENTRY_RGRAPHS) {
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Already parsed max rgraphs (%i)!\n",
                    TILESET_ENTRY_RGRAPHS);
                return 2;
            }
            const char *rgraph_name;
            GET_STR_CACHED(rgraph_name, &prend->name_store)
            rendergraph_t *rgraph =
                prismelrenderer_get_rendergraph(prend, rgraph_name);
            if(rgraph == NULL){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Couldn't find shape: %s\n",
                    rgraph_name);
                return 2;}
            entry->rgraphs[entry->n_rgraphs] = rgraph;
            entry->n_rgraphs++;
            if(rgraph_keys){
                CLOSE
            }
        }

        if(entry->n_rgraphs == 0){
            return UNEXPECTED("str");
        }
        if(rgraph_keys){
            const char *rgraph_key = rgraph_keys[entry->n_rgraphs];
            if(rgraph_key != NULL){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Missing an entry for when_faces_solid. "
                    "Expected entries:");
                for(int i = 0; rgraph_keys[i]; i++){
                    fprintf(stderr, " %s", rgraph_keys[i]);
                }
                fputc('\n', stderr);
                return 2;
            }
            CLOSE
        }
        CLOSE
    }
    NEXT
    *entries_ptr = entries;
    *entries_len_ptr = entries_len;
    *entries_size_ptr = entries_size;
    return 0;
}

static int tileset_parse(tileset_t *tileset,
    prismelrenderer_t *prend, const char *name,
    fus_lexer_t *lexer
){
    int err;

    vecspace_t *space = prend->space;

    /* parse unit */
    vec_t unit;
    GET("unit")
    OPEN
    for(int i = 0; i < space->dims; i++){
        GET_INT(unit[i])
    }
    CLOSE

    err = tileset_init(tileset, name, space, unit);
    if(err)return err;

    err = _tileset_parse_part(TILESET_VERTS,
        &tileset->vert_entries,
        &tileset->vert_entries_len,
        &tileset->vert_entries_size,
        prend, lexer);
    if(err)return err;
    err = _tileset_parse_part(TILESET_EDGES,
        &tileset->edge_entries,
        &tileset->edge_entries_len,
        &tileset->edge_entries_size,
        prend, lexer);
    if(err)return err;
    err = _tileset_parse_part(TILESET_FACES,
        &tileset->face_entries,
        &tileset->face_entries_len,
        &tileset->face_entries_size,
        prend, lexer);
    if(err)return err;

    return 0;
}

int tileset_load(tileset_t *tileset,
    prismelrenderer_t *prend, const char *filename, vars_t *vars
){
    int err;
    fus_lexer_t lexer;

    fprintf(stderr, "Loading tileset: %s\n", filename);

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init_with_vars(&lexer, text, filename, vars);
    if(err)return err;

    err = tileset_parse(tileset, prend, filename,
        &lexer);
    if(err)return err;

    fus_lexer_cleanup(&lexer);

    free(text);
    return 0;
}


/***********
 * PALETTE *
 ***********/

int palette_init(palette_t *pal, const char *name){
    pal->name = name;
    for(int i = 0; i < 256; i++){
        palette_entry_t *entry = &pal->entries[i];
        entry->frame_i = 0;
        entry->n_frames = 1;
        entry->frame_offset = 0;
        ARRAY_INIT(entry->keyframes)
    }
    return 0;
}

void palette_cleanup(palette_t *pal){
    for(int i = 0; i < 256; i++){
        palette_entry_t *entry = &pal->entries[i];
        ARRAY_FREE_PTR(palette_entry_keyframe_t*, entry->keyframes, (void))
    }
}

int palette_reset(palette_t *pal){
    for(int i = 0; i < 256; i++){
        int entry_i = i;
        palette_entry_t *entry = &pal->entries[i];
        entry->frame_i = entry->frame_offset;
    }
    return 0;
}

int palette_step(palette_t *pal){
    for(int i = 0; i < 256; i++){
        int entry_i = i;
        palette_entry_t *entry = &pal->entries[i];
        entry->frame_i++;
        if(entry->frame_i >= entry->n_frames)entry->frame_i = 0;
    }
    return 0;
}

int palette_update_colors(palette_t *pal, SDL_Color *colors,
    int t, int t_max
){
    /* SDL_Color colors[256] */
    for(int i = 0; i < 256; i++){
        int entry_i = i;
        palette_entry_t *entry = &pal->entries[i];
        SDL_Color *c = &colors[i];

        if(entry->keyframes_len == 0){
            interpolate_color(c, 0, 0, 0, t, t_max);
            continue;}

        int frame_i = entry->frame_i;
        palette_entry_keyframe_t *keyframe = NULL;
        palette_entry_keyframe_t *next_keyframe = NULL;

        for(int i = 0; i < entry->keyframes_len; i++){
            keyframe = entry->keyframes[i];
            if(frame_i < keyframe->n_frames){
                int j = (i + 1) % entry->keyframes_len;
                next_keyframe = entry->keyframes[j];
                break;
            }else{
                frame_i -= keyframe->n_frames;}
        }

        if(next_keyframe == NULL){
            fprintf(stderr, "Palette %s: entry %i: "
                "frame %i out of bounds\n",
                pal->name, entry_i, entry->frame_i);
            return 2;}

        Uint8 r = linear_interpolation(keyframe->color.r,
            next_keyframe->color.r, frame_i, keyframe->n_frames);
        Uint8 g = linear_interpolation(keyframe->color.g,
            next_keyframe->color.g, frame_i, keyframe->n_frames);
        Uint8 b = linear_interpolation(keyframe->color.b,
            next_keyframe->color.b, frame_i, keyframe->n_frames);
        interpolate_color(c, r, g, b, t, t_max);
    }
    return 0;
}

int update_sdl_palette(SDL_Palette *sdl_pal, SDL_Color *colors){
    RET_IF_SDL_NZ(SDL_SetPaletteColors(sdl_pal, colors, 0, 256));
    return 0;
}

int palette_update_sdl_palette(palette_t *pal, SDL_Palette *sdl_pal){
    int err;
    SDL_Color colors[256] = {0};
    err = palette_update_colors(pal, colors, 1, 1);
    if(err)return err;
    return update_sdl_palette(sdl_pal, colors);
}

static int palette_parse_color(fus_lexer_t *lexer, SDL_Color *color){
    int err;

    int r, g, b;
    err = fus_lexer_get_int(lexer, &r);
    if(err)return err;
    err = fus_lexer_get_int(lexer, &g);
    if(err)return err;
    err = fus_lexer_get_int(lexer, &b);
    if(err)return err;

    color->r = r;
    color->g = g;
    color->b = b;
    color->a = 255;

    return 0;
}

static int palette_parse(palette_t *pal, fus_lexer_t *lexer){
    int err;

    int entry_i = 1; /* First one was the transparent color */

    err = fus_lexer_get(lexer, "colors");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;

    while(1){
        if(fus_lexer_got(lexer, ")"))break;

        if(fus_lexer_got_int(lexer)){
            err = fus_lexer_get_int(lexer, &entry_i);
            if(err)return err;
        }

        err = fus_lexer_get(lexer, "(");
        if(err)return err;

        palette_entry_t *entry = &pal->entries[entry_i];

        if(fus_lexer_got(lexer, "animate")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;

            if(fus_lexer_got(lexer, "+")){
                err = fus_lexer_next(lexer);
                if(err)return err;
                err = fus_lexer_get_int_fancy(lexer,
                    &entry->frame_offset);
                if(err)return err;
            }

            while(1){
                if(fus_lexer_got(lexer, ")"))break;

                err = fus_lexer_get(lexer, "(");
                if(err)return err;

                ARRAY_PUSH_NEW(palette_entry_keyframe_t*, entry->keyframes,
                    keyframe)
                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                err = palette_parse_color(lexer, &keyframe->color);
                if(err)return err;
                err = fus_lexer_get(lexer, ")");
                if(err)return err;

                err = fus_lexer_get_int(lexer, &keyframe->n_frames);
                if(err)return err;
                if(keyframe->n_frames <= 0){
                    fus_lexer_err_info(lexer);
                    fprintf(stderr, "Palette entry %i: keyframe %i: "
                        "n_frames <= 0: %i\n",
                        entry_i, entry->keyframes_len-1,
                        keyframe->n_frames);
                    return 2;}

                err = fus_lexer_get(lexer, ")");
                if(err)return err;
            }
            err = fus_lexer_next(lexer);
            if(err)return err;

            if(entry->keyframes_len == 0){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Palette entry %i has no keyframes.\n",
                    entry_i);
                return 2;}
        }else{
            ARRAY_PUSH_NEW(palette_entry_keyframe_t*, entry->keyframes,
                keyframe)
            keyframe->n_frames = 1;
            err = palette_parse_color(lexer, &keyframe->color);
            if(err)return err;
        }

        err = fus_lexer_get(lexer, ")");
        if(err)return err;

        int n_frames = 0;
        for(int i = 0; i < entry->keyframes_len; i++){
            n_frames += entry->keyframes[i]->n_frames;}
        entry->n_frames = n_frames;

        if(entry->frame_offset >= entry->n_frames){
            fus_lexer_err_info(lexer);
            fprintf(stderr, "Palette entry %i: "
                "frame_offset >= n_frames: %i >= %i\n",
                entry_i, entry->frame_offset, entry->n_frames);
            return 2;}

        entry_i++;
    }
    err = fus_lexer_next(lexer);
    if(err)return err;

    return 0;
}

int palette_load(palette_t *pal, const char *filename, vars_t *vars){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init_with_vars(&lexer, text, filename, vars);
    if(err)return err;

    err = palette_init(pal, filename);
    if(err)return err;

    err = palette_parse(pal, &lexer);
    if(err)return err;

    err = palette_reset(pal);
    if(err)return err;

    fus_lexer_cleanup(&lexer);

    free(text);
    return 0;
}


/***********
 * PRISMEL *
 ***********/


int prismel_init(prismel_t *prismel, const char *name, vecspace_t *space){
    prismel->name = name;
    return prismel_create_images(prismel, space);
}

void prismel_cleanup(prismel_t *prismel){
    for(int i = 0; i < prismel->n_images; i++){
        prismel_image_t *image = &prismel->images[i];
        ARRAY_FREE_PTR(prismel_image_line_t*, image->lines, (void))
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
        ARRAY_INIT(image->lines)
    }
    return 0;
}

int prismel_image_push_line(prismel_image_t *image, int x, int y, int w){
    ARRAY_PUSH_NEW(prismel_image_line_t*, image->lines, line)
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

int prismelrenderer_init(prismelrenderer_t *prend, vecspace_t *space){
    prend->cache_bitmaps = true;
    prend->space = space;

    stringstore_init(&prend->filename_store);
    stringstore_init(&prend->name_store);

    ARRAY_INIT(prend->fonts)
    ARRAY_INIT(prend->geomfonts)
    ARRAY_INIT(prend->prismels)
    ARRAY_INIT(prend->rendergraphs)
    ARRAY_INIT(prend->palettes)
    ARRAY_INIT(prend->tilesets)
    ARRAY_INIT(prend->mappers)
    ARRAY_INIT(prend->palmappers)
    return 0;
}

void prismelrenderer_cleanup(prismelrenderer_t *prend){
    stringstore_cleanup(&prend->filename_store);
    stringstore_cleanup(&prend->name_store);

    ARRAY_FREE_PTR(font_t*, prend->fonts, font_cleanup)
    ARRAY_FREE_PTR(geomfont_t*, prend->geomfonts, geomfont_cleanup)
    ARRAY_FREE_PTR(prismel_t*, prend->prismels, prismel_cleanup)
    ARRAY_FREE_PTR(rendergraph_t*, prend->rendergraphs,
        rendergraph_cleanup)
    ARRAY_FREE_PTR(palette_t*, prend->palettes, palette_cleanup)
    ARRAY_FREE_PTR(tileset_t*, prend->tilesets, tileset_cleanup)
    ARRAY_FREE_PTR(prismelmapper_t*, prend->mappers,
        prismelmapper_cleanup)
    ARRAY_FREE_PTR(palettemapper_t*, prend->palmappers,
        palettemapper_cleanup)
}

void prismelrenderer_dump(prismelrenderer_t *prend, FILE *f,
    int dump_bitmaps
){
    /* dump_bitmaps: if 1, dumps bitmaps. If 2, also dumps their surfaces. */

    fprintf(f, "prismelrenderer: %p\n", prend);
    if(prend == NULL)return;
    fprintf(f, "  space: %p\n", prend->space);

    fprintf(f, "  prismels:\n");
    for(int i = 0; i < prend->prismels_len; i++){
        prismel_t *prismel = prend->prismels[i];
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
    for(int i = 0; i < prend->rendergraphs_len; i++){
        rendergraph_t *rgraph = prend->rendergraphs[i];
        rendergraph_dump(rgraph, f, 4, dump_bitmaps);
    }

    fprintf(f, "  palettes:\n");
    for(int i = 0; i < prend->palettes_len; i++){
        palette_t *palette = prend->palettes[i];
        fprintf(f, "    palette: %s\n", palette->name);
    }

    fprintf(f, "  tilesets:\n");
    for(int i = 0; i < prend->tilesets_len; i++){
        tileset_t *tileset = prend->tilesets[i];
        fprintf(f, "    tileset: %s\n", tileset->name);
    }

    fprintf(f, "  prismelmappers:\n");
    for(int i = 0; i < prend->mappers_len; i++){
        prismelmapper_t *mapper = prend->mappers[i];
        prismelmapper_dump(mapper, f, 4);
    }

    fprintf(f, "  palmappers:\n");
    for(int i = 0; i < prend->palmappers_len; i++){
        palettemapper_t *palmapper = prend->palmappers[i];
        fprintf(f, "    palmapper: %p\n", palmapper);
        fprintf(f, "      name: %s\n", palmapper->name);
        fprintf(f, "      table:\n");
        for(int i = 0; i < 16; i++){
            fprintf(f, "        ");
            for(int j = 0; j < 16; j++){
                fprintf(f, "%2X ", palmapper->table[i * 16 + j]);
            }
            fprintf(f, "\n");
        }
    }
}

static void _dump_size(int size, int count, FILE *f){
    int size_avg = size / count;
    fprintf(f, "      B   %12i %8i\n",
        size,
        size_avg);
    fprintf(f, "      KiB %12i %8i\n",
        size / 1024,
        size_avg / 1024);
    fprintf(f, "      MiB %12i %8i\n",
        size / 1024 / 1024,
        size_avg / 1024 / 1024);
    fprintf(f, "      GiB %12i %8i\n",
        size / 1024 / 1024 / 1024,
        size_avg / 1024 / 1024 / 1024);
}

void prismelrenderer_dump_stats(prismelrenderer_t *prend, FILE *f){
    int n_bitmaps = 0;
    int n_surfaces = 0;
    int surfaces_size = 0;

    for(int i = 0; i < prend->rendergraphs_len; i++){
        rendergraph_t *rgraph = prend->rendergraphs[i];
        n_bitmaps += rgraph->n_bitmaps;
        for(int i = 0; i < rgraph->n_bitmaps; i++){
            rendergraph_bitmap_t *bitmap = &rgraph->bitmaps[i];
            SDL_Surface *surface = bitmap->surface;
            if(surface != NULL){
                n_surfaces++;
                surfaces_size += surface->h * surface->pitch;
            }
        }
    }

    fprintf(f, "Prismelrenderer stats:\n");
    fprintf(f, "  bitmaps: %i\n", n_bitmaps);
    fprintf(f, "  surfaces: %i\n", n_surfaces);
    fprintf(f, "    size (total | avg):\n");
    _dump_size(surfaces_size, n_bitmaps, f);
}

void prismelrenderer_dump_stringstores(prismelrenderer_t *prend, FILE *f){
    fprintf(f, "* NAME STORE:\n");
    stringstore_dump(&prend->name_store, f);
    fprintf(f, "* FILENAME STORE:\n");
    stringstore_dump(&prend->filename_store, f);
}


int prismelrenderer_push_prismel(prismelrenderer_t *prend, const char *name,
    prismel_t **prismel_ptr
){
    int err;
    ARRAY_PUSH_NEW(prismel_t*, prend->prismels, prismel)
    if(!name){
        char *_name = generate_indexed_name("prismel",
            prend->prismels_len - 1);
        if(!name)return 1;
        name = stringstore_get_donate(&prend->name_store, _name);
        if(!name)return 1;
    }
    err = prismel_init(prismel, name, prend->space);
    if(err)return err;
    *prismel_ptr = prismel;
    return 0;
}


#define DICT_IMPLEMENT(TYPE, THING, THINGS, NAME) \
    TYPE##_t *prismelrenderer_get_##THING(prismelrenderer_t *prend, \
        const char *NAME \
    ){ \
        for(int i = 0; i < prend->THINGS##_len; i++){ \
            TYPE##_t *entry = prend->THINGS[i]; \
            if(strcmp(entry->NAME, NAME) == 0)return entry; \
        } \
        return NULL; \
    }

DICT_IMPLEMENT(font, font, fonts, filename)
DICT_IMPLEMENT(geomfont, geomfont, geomfonts, name)
DICT_IMPLEMENT(prismel, prismel, prismels, name)
DICT_IMPLEMENT(rendergraph, rendergraph, rendergraphs, name)
DICT_IMPLEMENT(palette, palette, palettes, name)
DICT_IMPLEMENT(tileset, tileset, tilesets, name)
DICT_IMPLEMENT(prismelmapper, mapper, mappers, name)
DICT_IMPLEMENT(palettemapper, palmapper, palmappers, name)
#undef DICT_IMPLEMENT


int prismelrenderer_get_or_create_font(
    prismelrenderer_t *prend, const char *filename,
    font_t **font_ptr
){
    int err;
    font_t *font = prismelrenderer_get_font(prend, filename);
    if(font == NULL){
        ARRAY_PUSH_NEW(font_t*, prend->fonts, _font)
        font = _font;
        err = font_load(font, filename, NULL);
        if(err)return err;
    }
    *font_ptr = font;
    return 0;
}

int prismelrenderer_get_or_create_palette(prismelrenderer_t *prend, const char *name,
    palette_t **palette_ptr
){
    int err;
    palette_t *palette = prismelrenderer_get_palette(prend, name);
    if(palette == NULL){
        ARRAY_PUSH_NEW(palette_t*, prend->palettes, _palette);
        palette = _palette;

        err = palette_load(palette, name, NULL);
        if(err)return err;
    }
    *palette_ptr = palette;
    return 0;
}

int prismelrenderer_get_or_create_tileset(prismelrenderer_t *prend, const char *name,
    tileset_t **tileset_ptr
){
    int err;
    tileset_t *tileset = prismelrenderer_get_tileset(prend, name);
    if(tileset == NULL){
        ARRAY_PUSH_NEW(tileset_t*, prend->tilesets, _tileset);
        tileset = _tileset;

        err = tileset_load(tileset, prend, name, NULL);
        if(err)return err;
    }
    *tileset_ptr = tileset;
    return 0;
}

int prismelrenderer_get_or_create_solid_palettemapper(
    prismelrenderer_t *prend, int color,
    palettemapper_t **palmapper_ptr
){
    int err;
    const char *name;
    {
        char *_name = generate_indexed_name("solid", color);
        if(!_name)return 1;
        name = stringstore_get_donate(&prend->name_store, _name);
        if(!name)return 1;
    }
    palettemapper_t *palmapper = prismelrenderer_get_palmapper(
        prend, name);
    if(palmapper == NULL){
        ARRAY_PUSH_NEW(palettemapper_t*, prend->palmappers, _palmapper)
        palmapper = _palmapper;
        err = palettemapper_init(palmapper, name, color);
        if(err)return err;
    }
    *palmapper_ptr = palmapper;
    return 0;
}


int prismelrenderer_load(prismelrenderer_t *prend, const char *filename,
    vars_t *vars
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init_with_vars(&lexer, text, filename, vars);
    if(err)return err;

    err = prismelrenderer_parse(prend, &lexer);
    if(err)return err;

    fus_lexer_cleanup(&lexer);

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

    fprintf(f, "palmappers:\n");
    for(int i = 0; i < prend->palmappers_len; i++){
        palettemapper_t *palmapper = prend->palmappers[i];
        fprintf(f, "    ");
        fus_write_str(f, palmapper->name);
        fprintf(f, ":\n");
        for(int i = 0; i < 256; i++){
            fprintf(f, "        : %i\n", palmapper->table[i]);
        }
    }

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

        /* prev_type: takes a value from enum rendergraph_child_type, or -1
        (an invalid value), so that child->type != prev_type for any child to
        begin with */
        int prev_type = -1;

        for(int i = 0; i < rgraph->children_len; i++){
            rendergraph_child_t *child = rgraph->children[i];

            if(child->type != prev_type){
                const char *type_msg =
                    rendergraph_child_type_plural(child->type);
                fprintf(f, "        %s:\n", type_msg);
                prev_type = child->type;
            }

            trf_t *trf = &child->trf;
            switch(child->type){
                case RENDERGRAPH_CHILD_TYPE_RGRAPH: {
                    rendergraph_t *rendergraph = child->u.rgraph.rendergraph;
                    fprintf(f, "            : ");
                    fus_write_str_padded(f, rendergraph->name, 40);
                    fprintf(f, " (");
                    fprintf(f, "% 3i", trf->add[0]);
                    for(int i = 1; i < prend->space->dims; i++){
                        fprintf(f, " % 3i", trf->add[i]);
                    }
                    fprintf(f, ") %2i %c %2i%c%c (%2i %2i)\n",
                        trf->rot, trf->flip? 't': 'f',
                        child->u.rgraph.frame_i,
                        child->u.rgraph.frame_i_additive? '+': ' ',
                        child->u.rgraph.frame_i_reversed? 'r': ' ',
                        child->frame_start,
                        child->frame_len);
                    break;
                }
                case RENDERGRAPH_CHILD_TYPE_PRISMEL: {
                    prismel_t *prismel = child->u.prismel.prismel;
                    fprintf(f, "            : ");
                    fus_write_str_padded(f, prismel->name, 7);
                    fprintf(f, " (");
                    fprintf(f, "% 3i", trf->add[0]);
                    for(int i = 1; i < prend->space->dims; i++){
                        fprintf(f, " % 3i", trf->add[i]);
                    }
                    fprintf(f, ") %2i %c %2i (%2i %2i)\n",
                        trf->rot, trf->flip? 't': 'f',
                        child->u.prismel.color,
                        child->frame_start,
                        child->frame_len);
                    break;
                }
                case RENDERGRAPH_CHILD_TYPE_LABEL:
                    fprintf(f, "            : ");
                    fus_write_str_padded(f, child->u.label.name, 40);
                    fputc('\n', f);
                    break;
                default:
                    fprintf(stderr, "Unknown rgraph child type: %i\n",
                        child->type);
                    return 2;
            }
        }
        fprintf(f, "\n");
    }

    fprintf(f, "mappers:\n");
    for(int i = 0; i < prend->mappers_len; i++){
        prismelmapper_t *mapper = prend->mappers[i];

        fprintf(f, "    ");
        fus_write_str(f, mapper->name);
        fprintf(f, ":\n");

        if(mapper->solid){
            fprintf(f, "        solid\n");
        }

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
    SDL_Palette *pal
){
    int err;
    for(int i = 0; i < prend->rendergraphs_len; i++){
        rendergraph_t *rgraph = prend->rendergraphs[i];
        err = rendergraph_render_all_bitmaps(rgraph, pal);
        if(err)return err;
    }
    return 0;
}

int prismelrenderer_get_rgraph_i(prismelrenderer_t *prend,
    const char *name
){
    for(int i = 0; i < prend->rendergraphs_len; i++){
        rendergraph_t *rgraph = prend->rendergraphs[i];
        if(rgraph->name == name || !strcmp(rgraph->name, name))return i;
    }
    return -1;
}

rendergraph_t *prismelrenderer_get_rgraph(prismelrenderer_t *prend,
    const char *name
){
    int rgraph_i = prismelrenderer_get_rgraph_i(prend, name);
    if(rgraph_i < 0)return NULL;
    return prend->rendergraphs[rgraph_i];
}




/*****************
 * PRISMELMAPPER *
 *****************/

void prismelmapper_cleanup(prismelmapper_t *mapper){
    ARRAY_FREE_PTR(prismelmapper_entry_t*, mapper->entries,
        (void))
    ARRAY_FREE_PTR(prismelmapper_application_t*, mapper->applications,
        (void))
    ARRAY_FREE_PTR(prismelmapper_mapplication_t*, mapper->mapplications,
        (void))
}

int prismelmapper_init(prismelmapper_t *mapper, const char *name,
    vecspace_t *space, bool solid
){
    mapper->name = name;
    mapper->solid = solid;
    mapper->space = space;
    vec_zero(mapper->unit);
    ARRAY_INIT(mapper->entries)
    ARRAY_INIT(mapper->applications)
    ARRAY_INIT(mapper->mapplications)
    return 0;
}

void prismelmapper_dump(prismelmapper_t *mapper, FILE *f, int n_spaces){
    char spaces[MAX_SPACES];
    get_spaces(spaces, MAX_SPACES, n_spaces);

    fprintf(f, "%sprismelmapper: %p\n", spaces, mapper);
    if(mapper == NULL)return;
    fprintf(f, "%s  name: %s\n", spaces, mapper->name);
    fprintf(f, "%s  space: %p\n", spaces, mapper->space);
    fprintf(f, "%s  solid: %c\n", spaces, mapper->solid? 'y': 'n');
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

    fprintf(f, "%s  mapplications:\n", spaces);
    for(int i = 0; i < mapper->mapplications_len; i++){
        prismelmapper_mapplication_t *mapplication = mapper->mapplications[i];
        fprintf(f, "%s    %s -> %s\n", spaces,
            mapplication->mapped_mapper == NULL? "<NULL>":
                mapplication->mapped_mapper->name,
            mapplication->resulting_mapper == NULL? "<NULL>":
                mapplication->resulting_mapper->name);
    }
}

int prismelmapper_push_entry(prismelmapper_t *mapper,
    prismel_t *prismel, rendergraph_t *rendergraph
){
    ARRAY_PUSH_NEW(prismelmapper_entry_t*, mapper->entries, entry)
    entry->prismel = prismel;
    entry->rendergraph = rendergraph;
    return 0;
}

int prismelmapper_apply_to_rendergraph(prismelmapper_t *mapper,
    prismelrenderer_t *prend,
    rendergraph_t *mapped_rgraph,
    const char *name, vecspace_t *space, Uint8 *table,
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

    /* If no name specified, generate one like "<curvy dodeca_sixth>" */
    if(name == NULL){
        char *_name = generate_mapped_name(mapper->name,
            mapped_rgraph->name);
        if(!_name)return 1;
        name = stringstore_get_donate(&prend->name_store, _name);
        if(!name)return 1;
    }

    /* Create a new rendergraph */
    resulting_rgraph = calloc(1, sizeof(rendergraph_t));
    if(resulting_rgraph == NULL)return 1;
    err = rendergraph_init(resulting_rgraph, name, prend,
        mapped_rgraph->palmapper,
        mapped_rgraph->animation_type, mapped_rgraph->n_frames);
    if(err)return err;
    resulting_rgraph->cache_bitmaps = mapped_rgraph->cache_bitmaps;

    /* Apply mapper to mapped_rgraph's children */
    for(int i = 0; i < mapped_rgraph->children_len; i++){
        rendergraph_child_t *child = mapped_rgraph->children[i];
        switch(child->type){
            case RENDERGRAPH_CHILD_TYPE_RGRAPH: {
                rendergraph_t *rgraph = child->u.rgraph.rendergraph;

                /* Recurse! */
                rendergraph_t *new_rgraph;
                err = prismelmapper_apply_to_rendergraph(mapper, prend,
                    rgraph, NULL, space, table, &new_rgraph);
                if(err)return err;

                /* Add a child to resulting_rgraph */
                rendergraph_child_t *new_child;
                err = rendergraph_push_child(resulting_rgraph,
                    RENDERGRAPH_CHILD_TYPE_RGRAPH,
                    &new_child);
                if(err)return err;
                new_child->u.rgraph.rendergraph = new_rgraph;
                new_child->trf = child->trf;
                vec_mul(mapper->space, new_child->trf.add,
                    mapper->unit);
                if(mapper->solid){
                    /* TODO: think about this and make sure it's correct */
                    new_child->u.rgraph.palmapper =
                        child->u.rgraph.palmapper;}
                new_child->u.rgraph.frame_i =
                    child->u.rgraph.frame_i;
                new_child->u.rgraph.frame_i_additive =
                    child->u.rgraph.frame_i_additive;
                new_child->u.rgraph.frame_i_reversed =
                    child->u.rgraph.frame_i_reversed;
                new_child->frame_start =
                    child->frame_start;
                new_child->frame_len =
                    child->frame_len;
                break;
            }
            case RENDERGRAPH_CHILD_TYPE_PRISMEL: {
                prismel_t *prismel = child->u.prismel.prismel;

                bool entry_found = false;
                for(int i = 0; i < mapper->entries_len; i++){
                    prismelmapper_entry_t *entry = mapper->entries[i];
                    if(prismel == entry->prismel){
                        rendergraph_child_t *new_child;
                        err = rendergraph_push_child(resulting_rgraph,
                            RENDERGRAPH_CHILD_TYPE_RGRAPH,
                            &new_child);
                        if(err)return err;
                        new_child->u.rgraph.rendergraph = entry->rendergraph;
                        new_child->trf = child->trf;
                        vec_mul(mapper->space, new_child->trf.add,
                            mapper->unit);
                        new_child->frame_start =
                            child->frame_start;
                        new_child->frame_len =
                            child->frame_len;

                        if(mapper->solid){
                            Uint8 color = child->u.prismel.color;
                            if(table != NULL)color = table[color];
                            err = prismelrenderer_get_or_create_solid_palettemapper(
                                prend, color,
                                &new_child->u.rgraph.palmapper);
                            if(err)return err;
                        }

                        entry_found = true;
                        break;
                    }
                }

                if(!entry_found){
                    fprintf(stderr,
                        "Prismel %s does not match any entry of mapper %s\n",
                        prismel->name, mapper->name);
                    err = 2; return err;
                }
                break;
            }
            case RENDERGRAPH_CHILD_TYPE_LABEL: {
                /* Add a child to resulting_rgraph */
                rendergraph_child_t *new_child;
                err = rendergraph_push_child(resulting_rgraph,
                    RENDERGRAPH_CHILD_TYPE_LABEL,
                    &new_child);
                if(err)return err;
                new_child->u.label.name = child->u.label.name;
                new_child->trf = child->trf;
                vec_mul(mapper->space, new_child->trf.add,
                    mapper->unit);
                break;
            }
            default:
                fprintf(stderr, "Unknown rgraph child type: %i\n",
                    child->type);
                return 2;
        }
    }

    /* Cache this resulting_rgraph on the mapper in case it
    ever gets applied to the same mapped_rgraph again */
    err = prismelmapper_push_application(mapper,
        mapped_rgraph, resulting_rgraph);
    if(err)return err;

    /* Add the resulting_rgraph to the prismelrenderer, makes for
    easier debugging */
    ARRAY_PUSH(rendergraph_t*, prend->rendergraphs,
        resulting_rgraph)

    /* Success! */
    *rgraph_ptr = resulting_rgraph;
    return 0;
}

int prismelmapper_apply_to_mapper(prismelmapper_t *mapper,
    prismelrenderer_t *prend,
    prismelmapper_t *mapped_mapper,
    const char *name, vecspace_t *space,
    prismelmapper_t **mapper_ptr
){
    /* Hey dawg, we applied mapper to mapped_mapper, resulting in
    resulting_mapper
    Question: do mapper, mapped_mapper, and resulting_mapper all
    need to share the same vecspace?.. */
    int err;

    prismelmapper_t *resulting_mapper;

    /* Check whether this mapper has already been applied to this
    mapped_mapper. If so, return the cached resulting_mapper. */
    resulting_mapper = prismelmapper_get_mapplication(
        mapper, mapped_mapper);
    if(resulting_mapper != NULL){
        *mapper_ptr = resulting_mapper;
        return 0;
    }

    /* If no name specified, generate one like "<curvy double>" */
    if(name == NULL){
        char *_name = generate_mapped_name(mapper->name,
            mapped_mapper->name);
        if(!_name)return 1;
        name = stringstore_get_donate(&prend->name_store, _name);
        if(!name)return 1;
    }

    /* Create a new prismelmapper */
    resulting_mapper = calloc(1, sizeof(prismelmapper_t));
    if(resulting_mapper == NULL)return 1;
    err = prismelmapper_init(resulting_mapper, name, space, false);
    if(err)return err;

    /* Calculate the new mapper's unit */
    vec_cpy(space->dims, resulting_mapper->unit, mapped_mapper->unit);
    vec_mul(space, resulting_mapper->unit, mapper->unit);

    /* Copy entries from mapped_mapper to resulting_mapper, applying
    mapper to each entry */
    for(int i = 0; i < mapped_mapper->entries_len; i++){
        prismelmapper_entry_t *entry = mapped_mapper->entries[i];

        const char *name;
        {
            char *_name = generate_mapped_name(mapper->name,
                entry->rendergraph->name);
            if(!_name)return 1;
            name = stringstore_get_donate(&prend->name_store, _name);
            if(!name)return 1;
        }

        rendergraph_t *new_rgraph;
        err = prismelmapper_apply_to_rendergraph(mapper, prend,
            entry->rendergraph, name, space, NULL, &new_rgraph);
        if(err)return err;

        err = prismelmapper_push_entry(resulting_mapper,
            entry->prismel, new_rgraph);
        if(err)return err;
    }

    /* Cache this resulting_mapper on the mapper in case it
    ever gets applied to the same mapped_mapper again */
    err = prismelmapper_push_mapplication(mapper,
        mapped_mapper, resulting_mapper);
    if(err)return err;

    /* Add the resulting_mapper to the prismelrenderer, makes for
    easier debugging */
    ARRAY_PUSH(prismelmapper_t*, prend->mappers,
        resulting_mapper)

    /* Success! */
    *mapper_ptr = resulting_mapper;
    return 0;
}

int prismelmapper_push_application(prismelmapper_t *mapper,
    rendergraph_t *mapped_rgraph, rendergraph_t *resulting_rgraph
){
    ARRAY_PUSH_NEW(prismelmapper_application_t*, mapper->applications,
        application)
    application->mapped_rgraph = mapped_rgraph;
    application->resulting_rgraph = resulting_rgraph;
    return 0;
}

int prismelmapper_push_mapplication(prismelmapper_t *mapper,
    prismelmapper_t *mapped_mapper, prismelmapper_t *resulting_mapper
){
    ARRAY_PUSH_NEW(prismelmapper_mapplication_t*, mapper->mapplications,
        mapplication)
    mapplication->mapped_mapper = mapped_mapper;
    mapplication->resulting_mapper = resulting_mapper;
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

prismelmapper_t *prismelmapper_get_mapplication(prismelmapper_t *mapper,
    prismelmapper_t *mapped_mapper
){
    for(int i = 0; i < mapper->mapplications_len; i++){
        prismelmapper_mapplication_t *mapplication = mapper->mapplications[i];
        if(mapplication->mapped_mapper == mapped_mapper){
            return mapplication->resulting_mapper;}
    }
    return NULL;
}



/******************
 * PALETTE MAPPER *
 ******************/

int palettemapper_init(palettemapper_t *palmapper, const char *name, int color){
    palmapper->name = name;
    palmapper->table[0] = 0; /* 0 is the transparent color */
    if(color < 0){
        for(int i = 1; i < 256; i++)palmapper->table[i] = i;
    }else{
        for(int i = 1; i < 256; i++)palmapper->table[i] = color;
    }
    ARRAY_INIT(palmapper->pmapplications)
    return 0;
}

void palettemapper_cleanup(palettemapper_t *palmapper){
    ARRAY_FREE_PTR(palettemapper_application_t*, palmapper->applications,
        (void))
    ARRAY_FREE_PTR(palettemapper_pmapplication_t*, palmapper->pmapplications,
        (void))
}

Uint8 palettemapper_apply_to_color(palettemapper_t *palmapper, Uint8 c){
    return palmapper->table[c];
}

void palettemapper_apply_to_table(palettemapper_t *palmapper, Uint8 *table){
    for(int i = 0; i < 256; i++){
        table[i] = palettemapper_apply_to_color(palmapper, table[i]);
    }
}

int palettemapper_apply_to_rendergraph(palettemapper_t *mapper,
    prismelrenderer_t *prend,
    rendergraph_t *mapped_rgraph,
    const char *name, vecspace_t *space,
    rendergraph_t **rgraph_ptr
){
    int err;

    rendergraph_t *resulting_rgraph;

    /* Check whether this mapper has already been applied to this
    mapped_rgraph. If so, return the cached resulting_rgraph. */
    resulting_rgraph = palettemapper_get_application(
        mapper, mapped_rgraph);
    if(resulting_rgraph != NULL){
        *rgraph_ptr = resulting_rgraph;
        return 0;
    }

    /* If no name specified, generate one like "<pm:red dodeca_sixth>" */
    if(name == NULL){
        char *_name = generate_palmapped_name(mapper->name,
            mapped_rgraph->name);
        if(!_name)return 1;
        name = stringstore_get_donate(&prend->name_store, _name);
        if(!name)return 1;
    }

    /* resulting_rgraph starts off as a copy of mapped_rgraph */
    resulting_rgraph = calloc(1, sizeof(rendergraph_t));
    if(resulting_rgraph == NULL)return 1;
    err = rendergraph_copy(resulting_rgraph, name, mapped_rgraph);
    if(err)return err;

    /* Set resulting_rgraph's palmapper to mapper
    (this was the whole point) */
    resulting_rgraph->palmapper = mapper;

    /* Cache this resulting_rgraph on the mapper in case it
    ever gets applied to the same mapped_rgraph again */
    err = palettemapper_push_application(mapper,
        mapped_rgraph, resulting_rgraph);
    if(err)return err;

    /* Add the resulting_rgraph to the prismelrenderer, makes for
    easier debugging */
    ARRAY_PUSH(rendergraph_t*, prend->rendergraphs,
        resulting_rgraph)

    /* Success! */
    *rgraph_ptr = resulting_rgraph;
    return 0;
}

int palettemapper_apply_to_palettemapper(palettemapper_t *palmapper,
    prismelrenderer_t *prend, palettemapper_t *mapped_palmapper,
    const char *name, palettemapper_t **palmapper_ptr
){
    int err;

    palettemapper_t *resulting_palmapper;

    /* Check whether this palmapper has already been applied to this
    mapped_palmapper. If so, return the cached resulting_palmapper. */
    resulting_palmapper = palettemapper_get_pmapplication(
        palmapper, mapped_palmapper);
    if(resulting_palmapper != NULL){
        *palmapper_ptr = resulting_palmapper;
        return 0;
    }

    /* If no name specified, generate one like "<cycle reverse>" */
    if(name == NULL){
        char *_name = generate_mapped_name(palmapper->name,
            mapped_palmapper->name);
        if(!_name)return 1;
        name = stringstore_get_donate(&prend->name_store, _name);
        if(!name)return 1;
    }

    /* Create a new palettemapper */
    resulting_palmapper = calloc(1, sizeof(palettemapper_t));
    if(resulting_palmapper == NULL)return 1;
    err = palettemapper_init(resulting_palmapper, name, -1);
    if(err)return err;

    /* Apply tables to each other... that's why we're here, really */
    for(int i = 0; i < 256; i++){
        resulting_palmapper->table[i] = palmapper->table
            [mapped_palmapper->table[i]];
    }

    /* Cache this resulting_palmapper on the palmapper in case it
    ever gets applied to the same mapped_palmapper again */
    err = palettemapper_push_pmapplication(palmapper,
        mapped_palmapper, resulting_palmapper);
    if(err)return err;

    /* Add the resulting_palmapper to the prismelrenderer, makes for
    easier debugging */
    ARRAY_PUSH(palettemapper_t*, prend->palmappers, resulting_palmapper)

    /* Success! */
    *palmapper_ptr = resulting_palmapper;
    return 0;
}

int palettemapper_push_application(palettemapper_t *mapper,
    rendergraph_t *mapped_rgraph, rendergraph_t *resulting_rgraph
){
    ARRAY_PUSH_NEW(palettemapper_application_t*, mapper->applications,
        application)
    application->mapped_rgraph = mapped_rgraph;
    application->resulting_rgraph = resulting_rgraph;
    return 0;
}

rendergraph_t *palettemapper_get_application(palettemapper_t *mapper,
    rendergraph_t *mapped_rgraph
){
    for(int i = 0; i < mapper->applications_len; i++){
        palettemapper_application_t *application =
            mapper->applications[i];
        if(application->mapped_rgraph == mapped_rgraph){
            return application->resulting_rgraph;}
    }
    return NULL;
}

int palettemapper_push_pmapplication(palettemapper_t *mapper,
    palettemapper_t *mapped_mapper, palettemapper_t *resulting_mapper
){
    ARRAY_PUSH_NEW(palettemapper_pmapplication_t*, mapper->pmapplications,
        pmapplication)
    pmapplication->mapped_mapper = mapped_mapper;
    pmapplication->resulting_mapper = resulting_mapper;
    return 0;
}

palettemapper_t *palettemapper_get_pmapplication(palettemapper_t *mapper,
    palettemapper_t *mapped_mapper
){
    for(int i = 0; i < mapper->pmapplications_len; i++){
        palettemapper_pmapplication_t *pmapplication =
            mapper->pmapplications[i];
        if(pmapplication->mapped_mapper == mapped_mapper){
            return pmapplication->resulting_mapper;}
    }
    return NULL;
}

