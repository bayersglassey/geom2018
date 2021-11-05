#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "font.h"


typedef struct console {
    int cols;
    int rows;

    char *text;
    int col;
    int row;

    char *input;
    int input_i;
    int input_len;
    int input_maxlen;
} console_t;



void console_clear(console_t *console);
void console_cleanup(console_t *console);
int console_init(console_t *console, int cols, int rows, int input_maxlen);
int console_get_index(console_t *console, int col, int row);
int console_get_text_i(console_t *console);
void console_move_up(console_t *console);
void console_move_down(console_t *console);
void console_move_left(console_t *console);
void console_move_right(console_t *console);
void console_newline(console_t *console);
void console_write_char(console_t *console, char c);
void console_write_msg(console_t *console, const char *msg);
void console_write_line(console_t *console, const char *msg);
void console_printf(console_t *console, const char *msg, ...);
void console_backspace(console_t *console);
void console_delete(console_t *console);
void console_input_char(console_t *console, char c);
void console_input_backspace(console_t *console);
void console_input_delete(console_t *console);
void console_input_clear(console_t *console);

int console_blit(console_t *console,
    font_putc_callback_t *callback, void *callback_data);

#endif