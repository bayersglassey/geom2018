

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "geom.h"
#include "hexgame_location.h"
#include "hexspace.h"
#include "lexer.h"
#include "geom_lexer_utils.h"
#include "vars.h"


static void _print_tabs(FILE *file, int indent){
    for(int i = 0; i < indent; i++)putc(' ', file);
}


rot_t hexgame_location_get_rot(hexgame_location_t *loc){
    /* Returns trf.rot, where trf is loc converted to a trf_t.

    * If !loc->turn, returns loc->rot.
    * If loc->turn, maps loc->rot to return value as follows:

        0 => 3
        1 => 2
        2 => 1
        3 => 0
        4 => 5
        5 => 4
    */

    rot_t rot = loc->rot;
    if(loc->turn){
        rot = rot_contain(HEXSPACE_ROT_MAX,
            HEXSPACE_ROT_MAX/2 - rot);
    }
    return rot;
}

void hexgame_location_zero(hexgame_location_t *loc){
    vec_zero(loc->pos);
    loc->rot = 0;
    loc->turn = false;
}

void hexgame_location_init_trf(hexgame_location_t *loc, trf_t *trf){
    /* Converts loc to a trf_t. */
    vec_cpy(HEXSPACE_DIMS, trf->add, loc->pos);
    trf->rot = hexgame_location_get_rot(loc);
    trf->flip = loc->turn;
}

void hexgame_location_from_trf(hexgame_location_t *loc, trf_t *trf){
    /* Converts a trf_t to loc. */
    vec_cpy(HEXSPACE_DIMS, loc->pos, trf->add);
    loc->rot = trf->flip? (HEXSPACE_ROT_MAX/2 - trf->rot): trf->rot;
    loc->turn = trf->flip;
}

void hexgame_location_apply(hexgame_location_t *loc, trf_t *trf){
    /* Applies a trf_t to loc. */
    trf_t loctrf;
    hexgame_location_init_trf(loc, &loctrf);
    trf_apply(&hexspace, &loctrf, trf);
    hexgame_location_from_trf(loc, &loctrf);
}


void hexgame_location_write(hexgame_location_t *loc, FILE *file, int indent){
    _print_tabs(stderr, indent);
    fprintf(stderr, "pos: (%i %i)\n",
        loc->pos[0], loc->pos[1]);
    _print_tabs(stderr, indent);
    fprintf(stderr, "rot: %i\n", loc->rot);
    _print_tabs(stderr, indent);
    fprintf(stderr, "turn: %c\n", loc->turn? 'y': 'n');
}

int hexgame_location_parse(hexgame_location_t *loc, fus_lexer_t *lexer){
    /* Example input: "(0 0) 0 n" */
    int err;
    err = fus_lexer_get_vec(lexer, &hexspace, loc->pos);
    if(err)return err;
    err = fus_lexer_get_int(lexer, &loc->rot);
    if(err)return err;
    err = fus_lexer_get_yn(lexer, &loc->turn);
    if(err)return err;
    return 0;
}

int hexgame_location_parse_string(hexgame_location_t *loc, const char *s){
    /* Example input: "(0 0) 0 n" */

    int x, y, r;
    char c;
    int ret = sscanf(s, " ( %i %i ) %i %c", &x, &y, &r, &c);
    if(ret < 4 || (c != 'y' && c != 'n')){
        fprintf(stderr, "Failed to parse location from string: %s\n", s);
        fprintf(stderr, "...expected: (x y) rot turn\n");
        fprintf(stderr, "...where turn is y or n\n");
        return 2;
    }

    loc->pos[0] = x;
    loc->pos[1] = y;
    loc->rot = r;
    loc->turn = c == 'y';
    return 0;
}

int hexgame_location_from_val(hexgame_location_t *loc, val_t *val){
    /* Initializes loc from a val of the form: arr(x y rot turn) */
    int err;
    if(val->type != VAL_TYPE_ARR){
        fprintf(stderr, "%s: Got %s, expected arr\n",
            __func__, val_type_name(val->type));
        return 2;
    }
    if(val->u.a.len != 4){
        fprintf(stderr, "%s: Got arr of len %i, expected 4\n",
            __func__, val->u.a.len);
        return 2;
    }
    loc->pos[0] = val_get_int(&val->u.a.vals[0]);
    loc->pos[1] = val_get_int(&val->u.a.vals[1]);
    loc->rot = val_get_int(&val->u.a.vals[2]);
    loc->turn = val_get_bool(&val->u.a.vals[3]);
    return 0;
}

int hexgame_location_to_val(hexgame_location_t *loc, val_t *val){
    /* Converts loc to a val of the form: arr(x y rot turn) */
    int err;
    err = val_set_arr(val, 4);
    if(err)return err;
    val_set_int(&val->u.a.vals[0], loc->pos[0]);
    val_set_int(&val->u.a.vals[1], loc->pos[1]);
    val_set_int(&val->u.a.vals[2], loc->rot);
    val_set_bool(&val->u.a.vals[3], loc->turn);
    return 0;
}
