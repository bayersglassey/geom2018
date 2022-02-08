/* Based on the "Low level terminal handling" section of antirez's
awesome "kilo" editor:

    https://github.com/antirez/kilo/blob/master/kilo.c

...that file's copyright is reproduced here:

     Copyright (C) 2016 Salvatore Sanfilippo <antirez at gmail dot com>

     All rights reserved.

     Redistribution and use in source and binary forms, with or without
     modification, are permitted provided that the following conditions are
     met:

      *  Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      *  Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
     "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
     LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
     A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
     HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
     SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
     LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
     DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
     THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
     (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
     OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "rawinput.h"


bool raw_mode = false;
struct termios original_termios;

int disable_raw_mode(){
    if(raw_mode){
        if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios) < 0){
            perror("tcsetattr");
            return 1;
        }
        raw_mode = false;
    }
    return 0;
}

static void reset_termios(){
    (void)disable_raw_mode();
}

int enable_raw_mode(){

    if(!isatty(STDIN_FILENO)){
        fprintf(stderr, "Stdin is not a tty!\n");
        return 1;
    }

    if(tcgetattr(STDIN_FILENO, &original_termios) < 0){
        perror("tcgetattr");
        return 1;
    }

    raw_mode = true;
    atexit(&reset_termios);

    struct termios t = original_termios;
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    t.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    /* t.c_oflag &= ~(OPOST); */
    /* control modes - set 8 bit chars */
    t.c_cflag |= (CS8);
    /* local modes - echoing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    t.c_lflag &= ~(ECHO | ICANON | IEXTEN /*| ISIG*/);
    /* control chars - set return condition: min number of bytes and timer. */
    t.c_cc[VMIN] = 0; /* Return each byte, or zero for timeout. */
    t.c_cc[VTIME] = 1; /* 100 ms timeout (unit is tens of second). */

    /* put terminal in raw mode after flushing */
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &t) < 0){
        perror("tcsetattr");
        return 1;
    }

    return 0;
}


/* Read a key from the terminal put in raw mode, trying to handle
escape sequences.
Returns < 0 on failure, otherwise a key code, i.e. a char or an extended
key code (see enum KEY_ACTION). */
int raw_getch() {
    int nread;
    char c, seq[3];
    while ((nread = read(STDIN_FILENO,&c,1)) == 0);
    if (nread == -1) return -1;

    while(1) {
        switch(c) {
        case ESC:    /* escape sequence */
            /* If this is just an ESC, we'll timeout here. */
            if (read(STDIN_FILENO,seq,1) == 0) return ESC;
            if (read(STDIN_FILENO,seq+1,1) == 0) return ESC;

            /* ESC [ sequences. */
            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    /* Extended escape, read additional byte. */
                    if (read(STDIN_FILENO,seq+2,1) == 0) return ESC;
                    if (seq[2] == '~') {
                        switch(seq[1]) {
                        case '3': return DEL_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        }
                    }
                } else {
                    switch(seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                    }
                }
            }

            /* ESC O sequences. */
            else if (seq[0] == 'O') {
                switch(seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
                }
            }
            break;
        default:
            return c;
        }
    }
}
