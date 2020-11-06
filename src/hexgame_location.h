#ifndef _HEXGAME_LOCATION_H_
#define _HEXGAME_LOCATION_H_

#include <stdbool.h>

#include "geom.h"


/************
 * LOCATION *
 ************/

typedef struct hexgame_location {
    /* Describes the same set of transformations as trf_t, but uses
    a different set of variables, better suited to video game character
    movement (e.g. rotating up/down, turning left/right).

    Applied in the following order: rot, turn, pos
    (Whereas trf_t is applied in the order: flip, rot, add)

    A trf_t represents the following transformation:

        F^flip * R^rot + add

    Note that "*" is not commutative for R and F:

        R^rot * F^flip = F^flip * R^(rot * -1^flip)

        Proof of simple case:

            R * F = F * R^-1

            R:     R * F:    F:     F * R^-1:
               +
              /
            (+)     (+)     (+)- +    (+)
                      \                 \
                       +                 +

    ...now, a hexgame_location_t represents the following transformation:

        NOTE: a "turn" is a flip followed by a 180 degree rotation,
        that is, F * R^3

        R^rot * (F * R^3)^turn + pos
        = R^rot * F^turn * R^3^turn + pos
        = F^turn * R^(rot * -1^turn) * R^3^turn + pos
        = F^turn * R^(rot * -1^turn + 3*turn) + pos

    So, in order to convert from hexgame_location_t to trf_t, we have:

        trf.flip = loc.turn
        trf.rot = loc.rot * -1^loc.turn + 3*loc.turn
        trf.add = loc.pos

    The formula for trf.rot can be expressed in C as:

        // Used by hexgame_location_init_trf
        // (And literally *is* hexgame_location_get_rot)
        trf.rot = loc.turn? (3 - loc.rot): loc.rot;

    The reverse formula (from trf to loc) is:

        // Used by hexgame_location_from_trf
        loc.rot = trf.flip? (3 - trf.rot): trf.rot;
    */
    vec_t pos;
    rot_t rot;
    bool turn;
} hexgame_location_t;

rot_t hexgame_location_get_rot(hexgame_location_t *loc);
void hexgame_location_init_trf(hexgame_location_t *loc, trf_t *trf);
void hexgame_location_from_trf(hexgame_location_t *loc, trf_t *trf);
void hexgame_location_apply(hexgame_location_t *loc, trf_t *trf);


/****************
 * SAVELOCATION *
 ****************/

typedef struct hexgame_savelocation {
    hexgame_location_t loc;
    char *map_filename;
    char *stateset_filename;
    char *state_name;
} hexgame_savelocation_t;

void hexgame_savelocation_init(hexgame_savelocation_t *location);
void hexgame_savelocation_cleanup(hexgame_savelocation_t *location);
void hexgame_savelocation_set(hexgame_savelocation_t *location, vecspace_t *space,
    vec_t pos, rot_t rot, bool turn, char *map_filename,
    char *stateset_filename, char *state_name);
int hexgame_savelocation_save(const char *filename, hexgame_savelocation_t *location);
int hexgame_savelocation_load(const char *filename, hexgame_savelocation_t *location);

#endif