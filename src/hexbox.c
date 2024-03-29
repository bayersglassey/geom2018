
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "hexspace.h"
#include "hexbox.h"
#include "mathutil.h"



#define HEXBOX_GET_MIN(HEXBOX, DIM) \
    ((HEXBOX)->values[HEXBOX_INDEX((DIM), HEXBOX_MIN)])

#define HEXBOX_GET_MAX(HEXBOX, DIM) \
    ((HEXBOX)->values[HEXBOX_INDEX((DIM), HEXBOX_MAX)])

#define HEXBOX_SET_MIN(HEXBOX, DIM, VALUE) { \
    int *_i = &HEXBOX_GET_MIN(HEXBOX, DIM); \
    *_i = _min(*_i, (VALUE)); \
}

#define HEXBOX_SET_MAX(HEXBOX, DIM, VALUE) { \
    int *_i = &HEXBOX_GET_MAX(HEXBOX, DIM); \
    *_i = _max(*_i, (VALUE)); \
}


void hexbox_set(hexbox_t *hexbox,
    int min_x, int max_x,
    int min_y, int max_y,
    int min_z, int max_z
){
    hexbox->values[0] = min_x;
    hexbox->values[1] = max_x;
    hexbox->values[2] = min_y;
    hexbox->values[3] = max_y;
    hexbox->values[4] = min_z;
    hexbox->values[5] = max_z;
}

void hexbox_zero(hexbox_t *hexbox){
    memset(hexbox, 0, sizeof(*hexbox));
}

void hexbox_add(hexbox_t *hexbox, vec_t add){
    int x = add[0];
    int y = add[1];
    int z = x - y;
    HEXBOX_GET_MIN(hexbox, HEXBOX_X) += x;
    HEXBOX_GET_MAX(hexbox, HEXBOX_X) += x;
    HEXBOX_GET_MIN(hexbox, HEXBOX_Y) += y;
    HEXBOX_GET_MAX(hexbox, HEXBOX_Y) += y;
    HEXBOX_GET_MIN(hexbox, HEXBOX_Z) += z;
    HEXBOX_GET_MAX(hexbox, HEXBOX_Z) += z;
}

void hexbox_rot1(hexbox_t *hexbox){
    /*
               MAX_Y
               * - *
        MIN_Z / \   \ MAX_X
             * -( )- *
        MIN_X \   \ / MAX_Z
               * - *
               MIN_Y
    */
    hexbox_t original = *hexbox;
    HEXBOX_GET_MAX(hexbox, HEXBOX_X) = HEXBOX_GET_MAX(&original, HEXBOX_Z);
    HEXBOX_GET_MAX(hexbox, HEXBOX_Y) = HEXBOX_GET_MAX(&original, HEXBOX_X);
    HEXBOX_GET_MIN(hexbox, HEXBOX_Z) = -HEXBOX_GET_MAX(&original, HEXBOX_Y);
    HEXBOX_GET_MIN(hexbox, HEXBOX_X) = HEXBOX_GET_MIN(&original, HEXBOX_Z);
    HEXBOX_GET_MIN(hexbox, HEXBOX_Y) = HEXBOX_GET_MIN(&original, HEXBOX_X);
    HEXBOX_GET_MAX(hexbox, HEXBOX_Z) = -HEXBOX_GET_MIN(&original, HEXBOX_Y);
}

void hexbox_rot(hexbox_t *hexbox, rot_t rot){
    rot = rot_contain(HEXSPACE_ROT_MAX, rot);
    for(int i = 0; i < rot; i++){
        hexbox_rot1(hexbox);
    }
}

void hexbox_flip1(hexbox_t *hexbox){
    /*
               MAX_Y
               * - *
        MIN_Z / \   \ MAX_X
             * -( )- *
        MIN_X \   \ / MAX_Z
               * - *
               MIN_Y
    */
#define HEXBOX_FLIP1_SWAP(_X, _Y, _OP) { \
    int _temp = _X; \
    _X = _OP _Y; \
    _Y = _OP _temp; \
}
    HEXBOX_FLIP1_SWAP(
        HEXBOX_GET_MAX(hexbox, HEXBOX_X),
        HEXBOX_GET_MAX(hexbox, HEXBOX_Z), +)
    HEXBOX_FLIP1_SWAP(
        HEXBOX_GET_MAX(hexbox, HEXBOX_Y),
        HEXBOX_GET_MIN(hexbox, HEXBOX_Y), -)
    HEXBOX_FLIP1_SWAP(
        HEXBOX_GET_MIN(hexbox, HEXBOX_Z),
        HEXBOX_GET_MIN(hexbox, HEXBOX_X), +)
#undef HEXBOX_FLIP1_SWAP
}

void hexbox_flip(hexbox_t *hexbox, flip_t flip){
    if(flip)hexbox_flip1(hexbox);
}

void hexbox_apply(hexbox_t *hexbox, trf_t *trf){
    hexbox_flip(hexbox, trf->flip);
    hexbox_rot(hexbox, trf->rot);
    hexbox_add(hexbox, trf->add);
}

void hexbox_point_union(hexbox_t *hexbox, int x, int y){
    int z = x - y;
    HEXBOX_SET_MIN(hexbox, HEXBOX_X, x)
    HEXBOX_SET_MAX(hexbox, HEXBOX_X, x)
    HEXBOX_SET_MIN(hexbox, HEXBOX_Y, y)
    HEXBOX_SET_MAX(hexbox, HEXBOX_Y, y)
    HEXBOX_SET_MIN(hexbox, HEXBOX_Z, z)
    HEXBOX_SET_MAX(hexbox, HEXBOX_Z, z)
}

void hexbox_union(hexbox_t *hexbox1, hexbox_t *hexbox2){
    for(int dim = 0; dim < HEXBOX_DIMS; dim++){
        HEXBOX_SET_MIN(hexbox1, dim, HEXBOX_GET_MIN(hexbox2, dim))
        HEXBOX_SET_MAX(hexbox1, dim, HEXBOX_GET_MAX(hexbox2, dim))
    }
}

bool hexbox_eq(hexbox_t *hexbox1, hexbox_t *hexbox2){
    for(int i = 0; i < HEXBOX_VALUES; i++){
        if(hexbox1->values[i] != hexbox2->values[i])return false;
    }
    return true;
}

void hexbox_fprintf(FILE *f, hexbox_t *hexbox){
    fputc('{', f);
    for(int i = 0; i < HEXBOX_VALUES; i++){
        fprintf(f, "% 4i", hexbox->values[i]);
    }
    fputc('}', f);
}

void hexbox_printf(hexbox_t *hexbox){
    hexbox_fprintf(stdout, hexbox);
}
