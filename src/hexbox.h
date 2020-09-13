#ifndef _HEXBOX_H_
#define _HEXBOX_H_

#include <stdio.h>
#include <stdbool.h>

#include "hexspace.h"

/*

    (x, y, z)
    ...where z = x - y


     (0 +1 -1) * - * (+1 +1 0)
              / \   \
   (-1 0 -1) * -( )- * (+1 0 +1)
              \   \ /
     (-1 -1 0) * - * (0 -1 +1)

-----------------------------------

                y=+1
               * - *
         z=-1 / \   \ x=+1
             * -( )- *
         x=-1 \   \ / z=+1
               * - *
                y=-1

    {
        -1, +1, // x
        -1, +1, // y
        -1, +1, // z
    }

-----------------------------------

               MAX_Y
               * - *
        MIN_Z / \   \ MAX_X
             * -( )- *
        MIN_X \   \ / MAX_Z
               * - *
               MIN_Y

    As a hexbox_t.values:
    {
        MIN_X, MAX_X,
        MIN_Y, MAX_Y,
        MIN_Z, MAX_Z,
    }

*/

#define HEXBOX_X    0
#define HEXBOX_Y    1
#define HEXBOX_Z    2
#define HEXBOX_DIMS 3

#define HEXBOX_MIN    0
#define HEXBOX_MAX    1
#define HEXBOX_BOUNDS 2

#define HEXBOX_INDEX(DIM, BOUND) ((DIM) * HEXBOX_BOUNDS + (BOUND))

#define HEXBOX_VALUES (HEXBOX_DIMS * HEXBOX_BOUNDS)


typedef struct hexbox {
    int values[HEXBOX_VALUES]; // lookup with HEXBOX_INDEX(dim, bound)
} hexbox_t;


void hexbox_set(hexbox_t *hexbox,
    int min_x, int max_x,
    int min_y, int max_y,
    int min_z, int max_z);
void hexbox_zero(hexbox_t *hexbox);
void hexbox_add(hexbox_t *hexbox, vec_t add);
void hexbox_rot1(hexbox_t *hexbox);
void hexbox_rot(hexbox_t *hexbox, rot_t rot);
void hexbox_flip1(hexbox_t *hexbox);
void hexbox_flip(hexbox_t *hexbox, flip_t flip);
void hexbox_apply(hexbox_t *hexbox, trf_t *trf);
void hexbox_point_union(hexbox_t *hexbox, int x, int y);
void hexbox_union(hexbox_t *hexbox1, hexbox_t *hexbox2);
bool hexbox_eq(hexbox_t *hexbox1, hexbox_t *hexbox2);
void hexbox_fprintf(FILE *f, hexbox_t *hexbox);
void hexbox_printf(hexbox_t *hexbox);
void hexbox_rot(hexbox_t *hexbox, rot_t rot);


#endif