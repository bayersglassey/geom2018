
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include <SDL2/SDL.h>


#include "util.h"
#include "lexer.h"
#include "font.h"



void font_cleanup(font_t *font){
    for(int i = 0; i < FONT_N_CHARS; i++){
        free(font->char_data[i]);
    }
}

int font_load(font_t *font, const char *filename){
    font->char_w = 0;
    font->char_h = 0;

    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    err = font_parse(font, &lexer);
    if(err)return err;

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

        if(char_c < 0){
            fprintf(stderr, "Char < 0: %i (%c)\n",
                char_c, char_c);
            return 2;
        }
        if(char_c >= FONT_N_CHARS){
            fprintf(stderr, "Char >= %i: %i (%c)\n",
                FONT_N_CHARS, char_c, char_c);
            return 2;
        }
        int char_i = char_c;

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
                int value;
                if(c == ' '){
                    value = 0;
                }else if(c >= '0' && c <= '2'){
                    value = c - '0' + 1;
                }else{
                    return fus_lexer_unexpected(lexer,
                        "'0', '1', '2', or ' '");
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

int font_printf(font_putc_callback_t *callback, void *callback_data,
    const char *msg, ...
){
    int err = 0;
    va_list vlist;
    va_start(vlist, msg);
    err = font_vprintf(callback, callback_data, msg, vlist);
    if(err)goto done;
done:
    va_end(vlist);
    return err;
}

int font_vprintf(font_putc_callback_t *callback, void *callback_data,
    const char *msg, va_list vlist
){
    int err;

    char c;
    while(c = *msg, c != '\0'){
        if(c == '%'){
            msg++;
            c = *msg;
            if(c != '%'){
                if(c == 'i'){
                    int i = va_arg(vlist, int);

                    /* 2^64 has 20 digits in base 10 */
                    static const int max_digits = 20;
                    int digits[max_digits];
                    int digit_i = 0;

                    /* in case i == 0 */
                    digits[0] = 0;

                    if(i < 0){
                        i = -i;
                        err = callback(callback_data, '-');
                        if(err)return err;
                    }

                    while(i > 0 && digit_i < max_digits){
                        digits[digit_i] = i % 10;
                        i /= 10;
                        digit_i++;
                    }

                    if(digit_i > 0)digit_i--;
                    while(digit_i >= 0){
                        char c = '0' + digits[digit_i];
                        err = callback(callback_data, c);
                        if(err)return err;
                        digit_i--;
                    }
                }else if(c == 'c'){
                    char c = va_arg(vlist, int);
                    err = callback(callback_data, c);
                    if(err)return err;
                }else if(c == 's'){
                    char *s = va_arg(vlist, char *);
                    char c;
                    while(c = *s, c != '\0'){
                        err = callback(callback_data, c);
                        if(err)return err;
                        s++;
                    }
                }else{
                    /* Unsupported percent+character, so just output them
                    back as-is */
                    err = callback(callback_data, '%');
                    if(err)return err;
                    err = callback(callback_data, c);
                    if(err)return err;
                }
                msg++;
                continue;
            }
        }

        err = callback(callback_data, c);
        if(err)return err;
        msg++;
    }

    return 0;
}
