
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL2/SDL.h>


#include "util.h"
#include "font.h"
#include "sdlfont.h"


static void get_char_coords(char c, int *char_x, int *char_y){
    int char_index = c;
    *char_x = char_index % 16;
    *char_y = char_index / 16;
}


/**********
* SDLFONT *
**********/

void sdlfont_cleanup(sdlfont_t *sdlfont){
    SDL_FreeSurface(sdlfont->surface);
}

int sdlfont_init(sdlfont_t *sdlfont, font_t *font, SDL_Palette *pal){
    int err = 0;

    sdlfont->font = font;
    sdlfont->autoupper = false;

    int char_w = font->char_w;
    int char_h = font->char_h;

    /******************
     * CREATE SURFACE *
     ******************/

    /* 16 * 8 = 128 = FONT_N_CHARS */
    SDL_Surface *surface = surface8_create(
        char_w * 16, char_h * 8, true, true, pal);
    if(surface == NULL)return 2;

    SDL_LockSurface(surface);
    SDL_memset(surface->pixels, 0, surface->h * surface->pitch);

    /* "palette" indexed by font's char_data "pixel values", 0 is
    actually transparent though */
    Uint8 color_values[FONT_N_COLOR_VALUES] = {0, 1+8, 1+7, 1+15};

    for(int i = 0; i < FONT_N_CHARS; i++){
        unsigned char *char_data = font->char_data[i];
        if(!char_data)continue;

        int char_x, char_y;
        get_char_coords(i, &char_x, &char_y);

        for(int y = 0; y < char_h; y++){
            Uint8 *p = surface8_get_pixel_ptr(surface,
                char_x * char_w,
                char_y * char_h + y);
            for(int x = 0; x < char_w; x++){
                int color_i = char_data[y * char_h + x];
                p[x] = color_values[color_i];
            }
        }
    }

    SDL_UnlockSurface(surface);

    sdlfont->surface = surface;
    return 0;
}

static void sdlfont_blitter_newline(sdlfont_blitter_t *blitter){
    blitter->col = 0;
    blitter->row++;
}

static void sdlfont_blitter_move_right(sdlfont_blitter_t *blitter){
    blitter->col++;
}

void sdlfont_putc(sdlfont_t *sdlfont, SDL_Surface *render_surface,
    int x, int y, char c
){
    font_t *font = sdlfont->font;
    int char_w = font->char_w;
    int char_h = font->char_h;

    int char_x, char_y;
    if(sdlfont->autoupper)c = toupper(c);
    get_char_coords(c, &char_x, &char_y);

    SDL_Rect src_rect = {
        char_x * char_w,
        char_y * char_h,
        char_w, char_h
    };
    SDL_Rect dst_rect = {
        x, y,
        char_w, char_h
    };
    SDL_BlitSurface(sdlfont->surface, &src_rect,
        render_surface, &dst_rect);
}

int sdlfont_putc_callback(void *data, char c){
    sdlfont_blitter_putc(data, c);
    return 0;
}

int sdlfont_printf(sdlfont_t *sdlfont, SDL_Surface *render_surface,
    int x0, int y0, const char *msg, ...
){
    va_list vlist;
    sdlfont_blitter_t blitter;
    sdlfont_blitter_init(&blitter, sdlfont, render_surface, x0, y0);
    return font_vprintf(&sdlfont_putc_callback, &blitter, msg, vlist);
}


/******************
* SDLFONT_BLITTER *
******************/

void sdlfont_blitter_init(sdlfont_blitter_t *blitter, sdlfont_t *sdlfont,
    SDL_Surface *render_surface, int x0, int y0
){
    blitter->sdlfont = sdlfont;
    blitter->render_surface = render_surface;
    blitter->x0 = x0;
    blitter->y0 = y0;
    blitter->row = 0;
    blitter->col = 0;
}

void sdlfont_blitter_putc(sdlfont_blitter_t *blitter, char c){
    sdlfont_t *sdlfont = blitter->sdlfont;
    font_t *font = sdlfont->font;

    if(c == '\n'){
        sdlfont_blitter_newline(blitter);
        return;
    }

    sdlfont_putc(sdlfont, blitter->render_surface,
        blitter->x0 + blitter->col * font->char_w,
        blitter->y0 + blitter->row * font->char_h,
        c);
    sdlfont_blitter_move_right(blitter);
}
