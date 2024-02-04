
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "mapeditor.h"
#include "rawinput.h"


/* ANSI sequences */
#define ANSI_CLS "\e[2J"


#define SWAP_C(_A, _B) {char _c = _A; _A = _B; _B = _c;}


static void print_help(FILE *file, const char *collmap_filename) {
    fprintf(file,
        "Editor commands:\n"
        "\n"
        "  Q           Quit editor\n"
        "  W           Save (to %s)\n"
        "  ?           Display help\n"
        "\n"
        "  Left/Right  Rotate left/right\n"
        "  Up/Down     Move forward/backward\n"
        "  PgUp/PgDown Flip\n"
        "  0           Return to map's origin\n"
        "  1           Return to map's spawn point\n"
        "  Enter       Show/hide marker\n"
        "\n"
        "  a/s/d       Set vert/edge/face primary char\n"
        "  A/S/D       Set vert/edge/face secondary char\n"
        "  f           Swap all primary & secondary chars\n"
        "  z/x/c       Draw vert/edge/face (using primary char)\n"
        "  Z/X/C       Draw vert/edge/face (using secondary char)\n"
        "  !           Set spawn point\n"
        , collmap_filename
    );
}


static void cls() {
    printf(ANSI_CLS);
}


static void press_any_key() {
    fprintf(stdout, "\n...Press any key to continue...\n");
    raw_getch();
}


int mapeditor(const char *collmap_filename, hexcollmap_write_options_t *opts,
    hexcollmap_t *collmap, hexcollmap_part_t **parts, int parts_len
){
    int err;

    vecspace_t *space = collmap->space;

    err = enable_raw_mode();
    if(err)return err;

    trf_t _marker = {
        .add = {collmap->ox, -collmap->oy},
    };
    trf_t *marker=&_marker;

    bool marker_visible = true;

    /* FOR DEBUGGING KEY CODES: */
    //while(1)printf("Got: %i\n", raw_getch());

    char vert_c = '0';
    char edge_c = '0';
    char face_c = '0';
    char vert_c2 = '@';
    char edge_c2 = '=';
    char face_c2 = 'o';

    bool quit = false;
    while(!quit){
        cls();
        opts->marker = marker_visible? marker: NULL;
        hexcollmap_write_with_parts(collmap, stdout, opts, parts, parts_len);
        printf("Vert: [%c][%c] | Edge: [%c][%c] | Face: [%c][%c]\n",
            vert_c, vert_c2, edge_c, edge_c2, face_c, face_c2);
        printf("Origin: (%i %i) | Marker: (%i %i) %i %c (%s)\n",
            collmap->ox,
            -collmap->oy,
            marker->add[0],
            marker->add[1],
            marker->rot,
            marker->flip? 'y': 'n',
            marker_visible? "Visible": "Hidden");
        printf("Press '?' for help!\n");

        int ch = raw_getch();
        if(ch < 0){
            fprintf(stderr, "Failed to get a key code from stdin\n");
            return 1;
        }

        switch(ch){
            case 'Q': quit = true; break;
            case 'W': {
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

                printf("\n*** Saved to: %s\n", collmap_filename);
                press_any_key();
            } break;
            case '?': {
                cls();
                print_help(stdout, collmap_filename);
                press_any_key();
            } break;

            case ARROW_LEFT: {
                marker_visible = true;
                marker->rot = rot_contain(space->rot_max, marker->rot + 1);
            } break;
            case ARROW_RIGHT: {
                marker_visible = true;
                marker->rot = rot_contain(space->rot_max, marker->rot - 1);
            } break;
            case ARROW_UP: {
                marker_visible = true;
                vec_t unit = {1, 0};
                space->vec_rot(unit, marker->rot);
                vec_add(space->dims, marker->add, unit);
            } break;
            case ARROW_DOWN: {
                marker_visible = true;
                vec_t unit = {1, 0};
                space->vec_rot(unit, marker->rot);
                vec_sub(space->dims, marker->add, unit);
            } break;
            case PAGE_UP:
            case PAGE_DOWN: {
                marker_visible = true;
                marker->flip = !marker->flip;
            } break;
            case '0': {
                /* Go to origin */
                marker_visible = true;
                marker->add[0] = collmap->ox;
                marker->add[1] = -collmap->oy;
                marker->rot = 0;
                marker->flip = false;
            } break;
            case '1': {
                /* Go to spawn point */
                marker_visible = true;
                hexgame_location_init_trf(&collmap->spawn, marker);
                marker->add[0] += collmap->ox;
                marker->add[1] -= collmap->oy;
            } break;
            case ENTER: marker_visible = !marker_visible; break;

            case 'a':
            case 's':
            case 'd':
            case 'A':
            case 'S':
            case 'D': {
                int vef; /* 0 = vert, 1 = edge, 2 = face */
                bool primary;
                char *c_ptr;
                if(ch == 'a'){ vef = 0; primary = true; c_ptr = &vert_c; }
                else if(ch == 's'){ vef = 1; primary = true; c_ptr = &edge_c; }
                else if(ch == 'd'){ vef = 2; primary = true; c_ptr = &face_c; }
                else if(ch == 'A'){ vef = 0; primary = false; c_ptr = &vert_c2; }
                else if(ch == 'S'){ vef = 1; primary = false; c_ptr = &edge_c2; }
                else{ vef = 2; primary = false; c_ptr = &face_c2; }
                printf("\nSet %s %s char (currently [%c])\n",
                    primary? "primary": "secondary",
                    vef == 0? "vert": vef == 1? "edge": "face",
                    *c_ptr);
                *c_ptr = raw_getch();
            } break;
            case 'f': {
                SWAP_C(vert_c, vert_c2);
                SWAP_C(edge_c, edge_c2);
                SWAP_C(face_c, face_c2);
            } break;
            case 'z':
            case 'x':
            case 'c':
            case 'Z':
            case 'X':
            case 'C': {
                /* Draw vert/edge/face */
                marker_visible = false;
                trf_t index = *marker;
                index.add[0] -= collmap->ox;
                index.add[1] = -index.add[1] - collmap->oy;
                char c;
                hexcollmap_elem_t *elem;
                if(ch == 'z' || ch == 'Z'){
                    c = ch == 'Z'? vert_c2: vert_c;
                    hexcollmap_normalize_vert(&index);
                    elem = hexcollmap_get_vert(collmap, &index);
                }else if(ch == 'x' || ch == 'X'){
                    c = ch == 'X'? edge_c2: edge_c;
                    hexcollmap_normalize_edge(&index);
                    elem = hexcollmap_get_edge(collmap, &index);
                }else{
                    c = ch == 'C'? face_c2: face_c;
                    hexcollmap_normalize_face(&index);
                    elem = hexcollmap_get_face(collmap, &index);
                }
                if(elem){
                    elem->tile_c = elem->tile_c == c? ' ': c;
                }
            } break;
            case '!': {
                /* Set spawn point */
                hexgame_location_from_trf(&collmap->spawn, marker);
                collmap->spawn.pos[0] -= collmap->ox;
                collmap->spawn.pos[1] += collmap->oy;

                printf("\n*** Set spawn point!\n");
                press_any_key();
            } break;
            default: break;
        }
    }

    err = disable_raw_mode();
    if(err)return err;

    fprintf(stderr, "Editor exiting OK!\n");
    return 0;
}
