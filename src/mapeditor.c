
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "mapeditor.h"
#include "rawinput.h"


/* ANSI sequences */
#define ANSI_CLS "\e[2J"



int mapeditor(const char *collmap_filename, hexcollmap_write_options_t *opts,
    hexcollmap_t *collmap, hexcollmap_part_t **parts, int parts_len
){
    int err;

    err = enable_raw_mode();
    if(err)return err;

    int x = 0;
    int y = 0;
    bool quit = false;
    while(!quit){
        printf(ANSI_CLS);
        hexcollmap_write_with_parts(collmap, stdout, opts, parts, parts_len);

        int ch = raw_getch();
        if(ch < 0){
            fprintf(stderr, "Failed to get a key code from stdin\n");
            return 1;
        }

        switch(ch){
            case ESC: case 'q': quit = true; break;
            case ARROW_UP:    y--; break;
            case ARROW_DOWN:  y++; break;
            case ARROW_LEFT:  x--; break;
            case ARROW_RIGHT: x++; break;
            default: break;
        }
    }

    err = disable_raw_mode();
    if(err)return err;

    fprintf(stderr, "Editor exiting OK!\n");
    return 0;
}
