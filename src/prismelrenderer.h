#ifndef _PRISMELRENDERER_H_
#define _PRISMELRENDERER_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "geom.h"
#include "lexer.h"
#include "bounds.h"
#include "array.h"
#include "rendergraph.h"


/***********
 * GENERAL *
 ***********/

char *generate_mapped_name(char *mapper_name, char *mappee_name);
char *generate_palmapped_name(char *mapper_name, char *mappee_name);
char *generate_indexed_name(char *base_name, int i);
bool get_animated_frame_visible(int n_frames,
    int frame_start, int frame_len, int frame_i);
int get_animated_frame_i(const char *animation_type,
    int n_frames, int frame_i);
int get_n_bitmaps(vecspace_t *space, int n_frames);
int get_bitmap_i(vecspace_t *space, rot_t rot, flip_t flip,
    int n_frames, int frame_i);


/***********
 * PALETTE *
 ***********/

typedef struct palette_entry_keyframe {
    SDL_Color color;
    int n_frames;
} palette_entry_keyframe_t;

typedef struct palette_entry {
    int frame_i;
    int n_frames;
    int frame_offset;
    ARRAY_DECL(palette_entry_keyframe_t*, keyframes)
} palette_entry_t;

typedef struct palette {
    char *name;
    palette_entry_t entries[256];
} palette_t;

int palette_init(palette_t *pal, char *name);
void palette_cleanup(palette_t *pal);
int palette_reset(palette_t *pal);
int palette_step(palette_t *pal);
int palette_update_colors(palette_t *pal, SDL_Color *colors,
    int t, int t_max);
int update_sdl_palette(SDL_Palette *sdl_pal, SDL_Color *colors);
int palette_update_sdl_palette(palette_t *pal, SDL_Palette *sdl_pal);
int palette_load(palette_t *pal, const char *filename);


/***********
 * PRISMEL *
 ***********/

typedef struct prismel_image_line {
    int x, y, w;
} prismel_image_line_t;

typedef struct prismel_image {
    ARRAY_DECL(struct prismel_image_line*, lines)
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
    int n_textures;
    bool cache_bitmaps;
    vecspace_t *space;
    ARRAY_DECL(struct palettemapper*, palmappers)
    ARRAY_DECL(struct prismel*, prismels)
    ARRAY_DECL(struct rendergraph*, rendergraphs)
    ARRAY_DECL(struct prismelmapper*, mappers)
} prismelrenderer_t;



int prismelrenderer_init(prismelrenderer_t *renderer, vecspace_t *space);
void prismelrenderer_cleanup(prismelrenderer_t *renderer);
void prismelrenderer_dump(prismelrenderer_t *renderer, FILE *f,
    int dump_bitmaps);
void prismelrenderer_dump_stats(prismelrenderer_t *renderer, FILE *f);
int prismelrenderer_push_prismel(prismelrenderer_t *renderer, char *name,
    prismel_t **prismel_ptr);
struct prismel *prismelrenderer_get_prismel(prismelrenderer_t *prend,
    const char *name);
struct rendergraph *prismelrenderer_get_rendergraph(prismelrenderer_t *prend,
    const char *name);
struct prismelmapper *prismelrenderer_get_mapper(prismelrenderer_t *prend,
    const char *name);
struct palettemapper *prismelrenderer_get_palmapper(prismelrenderer_t *prend,
    const char *name);
struct palettemapper;
int prismelrenderer_get_solid_palettemapper(prismelrenderer_t *prend,
    int color, struct palettemapper **palmapper_ptr);
int prismelrenderer_parse(prismelrenderer_t *prend, fus_lexer_t *lexer);
int prismelrenderer_load(prismelrenderer_t *prend, const char *filename);
int prismelrenderer_save(prismelrenderer_t *prend, const char *filename);
int prismelrenderer_write(prismelrenderer_t *prend, FILE *f);
int prismelrenderer_render_all_bitmaps(prismelrenderer_t *prend,
    SDL_Palette *pal);



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
    bool solid;
    ARRAY_DECL(prismelmapper_entry_t*, entries)
    ARRAY_DECL(prismelmapper_application_t*, applications)
    ARRAY_DECL(prismelmapper_mapplication_t*, mapplications)
} prismelmapper_t;


void prismelmapper_cleanup(prismelmapper_t *mapper);
int prismelmapper_init(prismelmapper_t *mapper, char *name,
    vecspace_t *space, bool solid);
void prismelmapper_dump(prismelmapper_t *mapper, FILE *f, int n_spaces);
int prismelmapper_push_entry(prismelmapper_t *mapper,
    prismel_t *prismel, rendergraph_t *rendergraph);
int prismelmapper_apply_to_rendergraph(prismelmapper_t *mapper,
    prismelrenderer_t *prend,
    rendergraph_t *mapped_rgraph,
    char *name, vecspace_t *space, Uint8 *table,
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


/******************
 * PALETTE MAPPER *
 ******************/

typedef struct palettemapper {
    char *name;
    Uint8 table[256];
    ARRAY_DECL(struct palettemapper_pmapplication*, pmapplications)
    ARRAY_DECL(struct palettemapper_application*, applications)
} palettemapper_t;

/* TODO: Rename {prismel,palette}mapper_application to just
mapper_application, etc.
And then we don't have to have separate structs for prismelmapper
and palettemapper applications, pmapplications, etc.
Ja knowmean? */
typedef struct palettemapper_application {
    rendergraph_t *mapped_rgraph;
    rendergraph_t *resulting_rgraph;
} palettemapper_application_t;

typedef struct palettemapper_pmapplication {
    struct palettemapper *mapped_mapper;
    struct palettemapper *resulting_mapper;
} palettemapper_pmapplication_t;

int palettemapper_init(palettemapper_t *palmapper, char *name, int color);
void palettemapper_cleanup(palettemapper_t *palmapper);
Uint8 palettemapper_apply_to_color(palettemapper_t *palmapper, Uint8 c);
void palettemapper_apply_to_table(palettemapper_t *palmapper, Uint8 *table);
int palettemapper_apply_to_rendergraph(palettemapper_t *mapper,
    prismelrenderer_t *prend,
    rendergraph_t *mapped_rgraph,
    char *name, vecspace_t *space,
    rendergraph_t **rgraph_ptr);
int palettemapper_apply_to_palettemapper(palettemapper_t *palmapper,
    prismelrenderer_t *prend, palettemapper_t *mapped_palmapper,
    char *name, palettemapper_t **palmapper_ptr);
int palettemapper_push_application(palettemapper_t *mapper,
    rendergraph_t *mapped_rgraph, rendergraph_t *resulting_rgraph);
rendergraph_t *palettemapper_get_application(palettemapper_t *mapper,
    rendergraph_t *mapped_rgraph);
int palettemapper_push_pmapplication(palettemapper_t *mapper,
    palettemapper_t *mapped_mapper, palettemapper_t *resulting_mapper);
palettemapper_t *palettemapper_get_pmapplication(palettemapper_t *mapper,
    palettemapper_t *mapped_mapper);


/*************
 * FUS_LEXER *
 *************/

int fus_lexer_get_palettemapper(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, palettemapper_t **palmapper_ptr);
int fus_lexer_get_prismel(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, prismel_t **prismel_ptr);
int fus_lexer_get_rendergraph(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, rendergraph_t **rgraph_ptr);
int fus_lexer_get_mapper(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, prismelmapper_t **mapper_ptr);


#endif