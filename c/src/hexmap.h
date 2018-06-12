#ifndef _HEXMAP_H_
#define _HEXMAP_H_


typedef struct hex_coord {
    int x;
    int y;

    int i;
    /*
        0: vert (0..0)
        1: edge (0..2)
        2: face (0..1)
    */
} hex_coord_t;

void hex_coord_cpy(hex_coord_t *c1, hex_coord_t *c2);
void hex_coord_add(hex_coord_t *coord, int x, int y);
void hex_coord_rot(hex_coord_t *coord, int n);
void hex_coord_flip(hex_coord_t *coord, bool flip);


#endif