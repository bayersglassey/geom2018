
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

#include "console.h"
#include "font.h"



void console_clear(console_t *console){
    int text_len = console->cols * console->rows;
    for(int i = 0; i < text_len; i++)console->text[i] = ' ';
    console->col = 0;
    console->row = 0;
}

void console_clear_line(console_t *console){
    int cols = console->cols;
    char *text = console->text + console->row * cols;
    for(int i = 0; i < cols; i++)text[i] = ' ';
    console->col = 0;
}

int console_init(console_t *console, int cols, int rows, int input_maxlen){
    int text_len = cols * rows;

    char *text = calloc(text_len + 1, sizeof(*text));
    if(text == NULL)return 1;
    for(int i = 0; i < text_len; i++)text[i] = ' ';

    char *input = calloc(input_maxlen + 1, sizeof(*text));
    if(input == NULL)return 1;

    console->cols = cols;
    console->rows = rows;

    console->text = text;
    console->col = 0;
    console->row = 0;

    console->input = input;
    console->input_i = 0;
    console->input_len = 0;
    console->input_maxlen = input_maxlen;
    return 0;
}

int console_get_index(console_t *console, int col, int row){
    return row * console->cols + col;
}

int console_get_text_i(console_t *console){
    return console_get_index(console, console->col, console->row);
}

void console_move_up(console_t *console){
    if(console->row < 1)console->row = console->rows - 1;
    else console->row--;
}

void console_move_down(console_t *console){
    if(console->row >= console->rows - 1)console->row = 0;
    else console->row++;
    console_clear_line(console);
}

void console_move_left(console_t *console){
    if(console->col < 1){
        console->col = console->cols - 1;
        console_move_up(console);
    }else{
        console->col--;
    }
}

void console_move_right(console_t *console){
    if(console->col >= console->cols - 1){
        console->col = 0;
        console_move_down(console);
    }else{
        console->col++;
    }
}

void console_newline(console_t *console){
    console->col = 0;
    console_move_down(console);
}

void console_write_char(console_t *console, char c){
    if(c == '\n'){
        console_newline(console);
    }else if(c == '\0'){
        /* Don't show EOF */
    }else{
        int text_i = console_get_text_i(console);
        console->text[text_i] = c;
        console_move_right(console);
    }
}

void console_write_msg(console_t *console, const char *msg){
    while(*msg != '\0'){
        console_write_char(console, *msg);
        msg++;
    }
}

void console_backspace(console_t *console){
    console_move_left(console);
    int text_i = console_get_text_i(console);
    console->text[text_i] = ' ';
}

void console_delete(console_t *console){
    if(console->input_i >= console->input_len - 1)return;
    int text_i = console_get_text_i(console);
    console->text[text_i] = ' ';
    console_move_left(console);
}

void console_input_char(console_t *console, char c){
    if(console->input_len >= console->input_maxlen)return;
    for(int i = console->input_len; i > console->input_i; i--){
        console->input[i] = console->input[i - 1];
    }
    console->input[console->input_i] = c;
    console->input_i++;
    console->input_len++;
    console_write_char(console, c);
}

void console_input_backspace(console_t *console){
    if(console->input_len < 1)return;
    for(int i = console->input_i - 1; i < console->input_len; i++){
        console->input[i] = console->input[i + 1];
    }
    console->input_i--;
    console->input_len--;
    console_backspace(console);
}

void console_input_delete(console_t *console){
    if(console->input_len < 1)return;
    for(int i = console->input_i; i < console->input_len; i++){
        console->input[i] = console->input[i + 1];
    }
    console->input_i--;
    console->input_len--;
    console_delete(console);
}

void console_input_clear(console_t *console){
    int input_len = console->input_len;
    for(int i = 0; i < input_len; i++)console->input[i] = '\0';
    console->input_i = 0;
    console->input_len = 0;
}



void console_blit(console_t *console, font_t *font, SDL_Surface *render_surface,
    int x0, int y0
){
    int char_w = font->char_w;
    int char_h = font->char_h;

    int cols = console->cols;
    int rows = console->rows;
    char *text = console->text + (console->row + 1) * cols;
    char *text_end = console->text + cols * rows;

    int y = y0;
    for(int row = 0; row < rows; row++){
        int x = x0;
        if(text >= text_end)text = console->text;
        for(int col = 0; col < cols; col++){
            font_blitchar(font, render_surface, x, y, *text);
            text++;
            x += char_w;
        }
        y += char_h;
    }
}

