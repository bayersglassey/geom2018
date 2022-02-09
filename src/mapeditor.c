
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

    trf_t _marker = {
        .add = {collmap->ox, -collmap->oy},
    };
    trf_t *marker=&_marker;
    opts->marker = marker;

    bool quit = false;
    while(!quit){
        printf(ANSI_CLS);
        hexcollmap_write_with_parts(collmap, stdout, opts, parts, parts_len);
        printf("Origin: (%i %i)\n", collmap->ox, collmap->oy);
        printf("Marker: (%i %i)\n", marker->add[0], marker->add[1]);

        int ch = raw_getch();
        if(ch < 0){
            fprintf(stderr, "Failed to get a key code from stdin\n");
            return 1;
        }

        switch(ch){
            case 'q': quit = true; break;
            case 's': {
                if(!strcmp(collmap_filename, "<stdin>")){
                    fprintf(stderr, "Can't save to stdin!\n");
                    return 2;
                }
                FILE *file = fopen(collmap_filename, "w");
                if(!file){
                    perror("fopen");
                    return 1;
                }
                opts->marker = NULL;
                hexcollmap_write_with_parts(collmap, file, opts,
                    parts, parts_len);
                opts->marker = marker;
                if(fclose(file)){
                    perror("fclose");
                    return 1;
                }
                printf("Saved to: %s\n", collmap_filename);
                break;
            }
            case ARROW_UP:    marker->add[1]++; break;
            case ARROW_DOWN:  marker->add[1]--; break;
            case ARROW_LEFT:  marker->add[0]--; break;
            case ARROW_RIGHT: marker->add[0]++; break;
            default: break;
        }
    }

    err = disable_raw_mode();
    if(err)return err;

    fprintf(stderr, "Editor exiting OK!\n");
    return 0;
}
