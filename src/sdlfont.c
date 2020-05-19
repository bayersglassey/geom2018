
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include <SDL2/SDL.h>


#include "util.h"
#include "font.h"
#include "sdlfont.h"
#include "generic_printf.h"


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
    Uint8 colors[FONT_N_COLOR_VALUES] = {0, 1+8, 1+7, 1+15};

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
                p[x] = colors[color_i];
            }
        }
    }

    SDL_UnlockSurface(surface);

    sdlfont->surface = surface;
    return 0;
}

int sdlfont_printf(sdlfont_t *sdlfont, SDL_Surface *render_surface,
    int x0, int y0, const char *msg, ...
){
    int err = 0;
    va_list vlist;
    va_start(vlist, msg);

    sdlfont_blitter_t blitter;
    sdlfont_blitter_init(&blitter, sdlfont, render_surface, x0, y0);
    err = generic_vprintf(&sdlfont_blitter_putc_callback, &blitter,
        msg, vlist);

    va_end(vlist);
    return err;
}

static int sdlfont_putc(sdlfont_t *sdlfont, SDL_Surface *render_surface,
    int x, int y, char c
){
    int err;

    font_t *font = sdlfont->font;
    int char_w = font->char_w;
    int char_h = font->char_h;

    if(sdlfont->autoupper)c = toupper(c);
    int char_i = c;
    if(char_i < 0 || char_i >= FONT_N_CHARS){
        fprintf(stderr, "%s: Char outside 0..%i: %i (%c)\n",
            __func__, FONT_N_CHARS - 1, char_i, c);
        return 2;
    }

    int char_x, char_y;
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
    RET_IF_SDL_NZ(SDL_BlitSurface(sdlfont->surface, &src_rect,
        render_surface, &dst_rect));
    return 0;
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

static void sdlfont_blitter_newline(sdlfont_blitter_t *blitter){
    blitter->col = 0;
    blitter->row++;
}

static void sdlfont_blitter_move_right(sdlfont_blitter_t *blitter){
    blitter->col++;
}

int sdlfont_blitter_putc(sdlfont_blitter_t *blitter, char c){
    int err;

    sdlfont_t *sdlfont = blitter->sdlfont;
    font_t *font = sdlfont->font;

    if(c == '\n'){
        sdlfont_blitter_newline(blitter);
        return 0;
    }

    err = sdlfont_putc(sdlfont, blitter->render_surface,
        blitter->x0 + blitter->col * font->char_w,
        blitter->y0 + blitter->row * font->char_h,
        c);
    if(err)return err;

    sdlfont_blitter_move_right(blitter);
    return 0;
}

int sdlfont_blitter_putc_callback(void *data, char c){
    /* Callback for use with generic_printf, console_blit, etc */
    return sdlfont_blitter_putc(data, c);
}
