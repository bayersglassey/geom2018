
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include <SDL2/SDL.h>


#include "util.h"
#include "lexer.h"
#include "font.h"



static int _get_color(char c){
    if(c == ' ')return 0;
    if(c >= '0' && c <= '9')return c - '0' + 1;
    if(c >= 'A' && c <= 'Z')return c - 'A' + 10 + 1;
    if(c >= 'a' && c <= 'z')return c - 'a' + 10 + 1;
    return -1;
}

static char _get_color_char(int color){
    if(color == 0)return ' ';
    if(color >= 1 && color <= 10)return '0' + (color - 1);
    if(color >= 11 && color <= 16)return 'A' + (color - 1);
    return '?';
}


void font_cleanup(font_t *font){
    for(int i = 0; i < FONT_N_CHARS; i++){
        free(font->char_data[i]);
    }
}

int font_load(font_t *font, const char *filename, vars_t *vars){
    font->filename = filename;
    font->autoupper = false;
    font->char_w = 0;
    font->char_h = 0;

    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init_with_vars(&lexer, text, filename, vars);
    if(err)return err;

    err = font_parse(font, &lexer);
    if(err)return err;

    fus_lexer_cleanup(&lexer);

    free(text);
    return 0;
}

static void font_clear_all_char_data(font_t *font){
    for(int i = 0; i < FONT_N_CHARS; i++){
        font->char_data[i] = NULL;
    }
}

int font_parse(font_t *font, fus_lexer_t *lexer){
    int err;

    if(fus_lexer_got(lexer, "autoupper")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        font->autoupper = true;
    }

    /*****************
     * GET CHAR W, H *
     *****************/

    int char_w, char_h;

    err = fus_lexer_get(lexer, "char_w");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_get_int(lexer, &char_w);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    err = fus_lexer_get(lexer, "char_h");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_get_int(lexer, &char_h);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    font->char_w = char_w;
    font->char_h = char_h;


    /***************
     * PARSE CHARS *
     ***************/

    font_clear_all_char_data(font);

    err = fus_lexer_get(lexer, "chars");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;

    while(1){
        if(fus_lexer_got(lexer, ")"))break;

        char char_c;
        err = fus_lexer_get_chr(lexer, &char_c);
        if(err)return err;

        int char_i = char_c;
        if(char_i < 0){
            fprintf(stderr, "Char < 0: %i (%c)\n",
                char_i, char_c);
            return 2;
        }
        if(char_i >= FONT_N_CHARS){
            fprintf(stderr, "Char >= %i: %i (%c)\n",
                FONT_N_CHARS, char_i, char_c);
            return 2;
        }

        unsigned char *char_data = font->char_data[char_i] = calloc(
            char_w * char_h, sizeof(*char_data));
        if(!char_data)return 1;

        err = fus_lexer_get(lexer, "(");
        if(err)return err;

        for(int y = 0; y < char_h; y++){
            char *line;
            err = fus_lexer_get_str(lexer, &line);
            if(err)return err;
            int line_w = strlen(line);
            if(line_w != char_w){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Bad line width: %i, expected %i\n",
                    line_w, char_w);
                return 2;}

            for(int x = 0; x < char_w; x++){
                char c = line[x];
                int value = _get_color(c);
                if(value < 0){
                    return fus_lexer_unexpected(lexer,
                        "' ' or a hexadecimal digit");
                }
                char_data[y * char_w + x] = value;
            }

            free(line);
        }

        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }
    err = fus_lexer_next(lexer);
    if(err)return err;

    return 0;
}
