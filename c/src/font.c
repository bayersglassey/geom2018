
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include <SDL2/SDL.h>


#include "util.h"
#include "lexer.h"
#include "font.h"


int font_load(font_t *font, const char *filename,
    SDL_Renderer *renderer
){
    font->char_w = 0;
    font->char_h = 0;
    font->surface = NULL;
    font->texture = NULL;

    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text);
    if(err)return err;

    err = font_parse(font, &lexer, renderer);
    if(err)return err;

    free(text);
    return 0;
}

void font_get_char_coords(font_t *font, char c, int *char_x, int *char_y){
    int char_index = (unsigned)c;
    *char_x = char_index % 16;
    *char_y = char_index / 16;
}

int font_parse(font_t *font, fus_lexer_t *lexer,
    SDL_Renderer *renderer
){
    int err;


    /*****************
     * GET CHAR W, H *
     *****************/

    int char_w, char_h;

    err = fus_lexer_expect(lexer, "char_w");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    err = fus_lexer_expect_int(lexer, &char_w);
    if(err)return err;
    err = fus_lexer_expect(lexer, ")");
    if(err)return err;

    err = fus_lexer_expect(lexer, "char_h");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    err = fus_lexer_expect_int(lexer, &char_h);
    if(err)return err;
    err = fus_lexer_expect(lexer, ")");
    if(err)return err;


    /******************
     * CREATE SURFACE *
     ******************/

    static const int n_chars_x = 16;
    static const int n_chars_y = 16;
    static const int bpp = 32;

    SDL_Surface *surface = surface_create(
        char_w * n_chars_x, char_h * n_chars_y, bpp);
    if(surface == NULL)return 2;

    SDL_LockSurface(surface);
    SDL_memset(surface->pixels, 0, surface->h * surface->pitch);

    Uint32 color_values[10] = {0};
    for(int i = 1; i < 10; i++){
        int c = 255 / 10 * i;
        color_values[i] = SDL_MapRGB(surface->format, c, c, c);
    }


    /************************
     * PARSE & RENDER CHARS *
     ************************/

    err = fus_lexer_expect(lexer, "chars");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;

    while(1){
        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, ")"))break;

        char *char_name;
        err = fus_lexer_get_str(lexer, &char_name);
        if(err)return err;
        if(strlen(char_name) < 1){
            fus_lexer_err_info(lexer);
            fprintf(stderr, "Empty char name.\n");
            return 2;}

        int char_x, char_y;
        font_get_char_coords(font, char_name[0], &char_x, &char_y);
        free(char_name);

        err = fus_lexer_expect(lexer, "(");
        if(err)return err;

        for(int y = 0; y < char_h; y++){
            char *line;
            err = fus_lexer_expect_str(lexer, &line);
            if(err)return err;
            int line_w = strlen(line);
            if(line_w != char_w){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Bad line width: %i, expected %i\n",
                    line_w, char_w);
                return 2;}

            Uint32 *p = surface_get_pixel_ptr(surface,
                char_x * char_w,
                char_y * char_h + y);

            for(int x = 0; x < char_w; x++){
                char c = line[x];
                if(c >= '0' && c <= '9'){
                    p[x] = color_values[c - '0'];
                }
            }

            free(line);
        }

        err = fus_lexer_expect(lexer, ")");
        if(err)return err;
    }


    /*************
     * FINISH UP *
     *************/

    SDL_UnlockSurface(surface);

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if(texture == NULL){
        fprintf(stderr, "SDL_CreateTextureFromSurface failed: %s\n",
            SDL_GetError());
        return 2;
    }

    font->char_w = char_w;
    font->char_h = char_h;
    font->surface = surface;
    font->texture = texture;
    return 0;
}

typedef struct font_blitter {
    font_t *font;
    SDL_Renderer *renderer;
    int x0;
    int y0;
    int x;
    int y;
} font_blitter_t;

void font_blitchar(font_blitter_t *blitter, char c){
    font_t *font = blitter->font;
    int char_w = font->char_w;
    int char_h = font->char_h;

    if(c == '\n'){
        blitter->x = blitter->x0;
        blitter->y += char_h;
        return;
    }

    int char_x, char_y;
    font_get_char_coords(font, toupper(c),
        &char_x, &char_y);

    SDL_Rect src_rect = {
        char_x * char_w,
        char_y * char_h,
        char_w, char_h
    };
    SDL_Rect dst_rect = {
        blitter->x, blitter->y,
        char_w, char_h
    };
    SDL_RenderCopy(blitter->renderer, font->texture,
        &src_rect, &dst_rect);

    blitter->x += char_w;
}

void font_blitmsg(font_t *font, SDL_Renderer *renderer, const char *msg, ...){
    int char_w = font->char_w;
    int char_h = font->char_h;
    font_blitter_t blitter = {font, renderer, 0, 0, 0, 0};

    va_list args;
    va_start(args, msg);

    char c;
    while(c = *msg, c != '\0'){
        if(c == '%'){
            msg++;
            c = *msg;
            if(c != '%'){
                if(c == 'i'){
                    int i = va_arg(args, int);
                    if(i == 0){
                        font_blitchar(&blitter, '0');
                    }else if(i < 0){
                        i = -i;
                        font_blitchar(&blitter, '-');
                    }
                    while(i != 0){
                        char c = '0' + (i % 10);
                        font_blitchar(&blitter, c);
                        i /= 10;
                    }
                }else if(c == 'c'){
                    char c = va_arg(args, int);
                    font_blitchar(&blitter, c);
                }else if(c == 's'){
                    char *s = va_arg(args, char *);
                    char c;
                    while(c = *s, c != '\0'){
                        font_blitchar(&blitter, c);
                        s++;
                    }
                }else{
                    fprintf(stderr,
                        "font_blitmsg: Unsupported format string: %%%c", c);
                    goto done;
                }
                msg++;
                continue;
            }
        }

        font_blitchar(&blitter, c);
        msg++;
    }

done:
    va_end(args);
}
