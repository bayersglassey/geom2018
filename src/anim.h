#ifndef _ANIM_H_
#define _ANIM_H_

#include <stdio.h>

#include "array.h"
#include "geom.h"
#include "lexer.h"
#include "hexmap.h"
#include "prismelrenderer.h"

#define ANIM_KEY_CS "xudlrfb"


typedef struct state {
    struct stateset *stateset;
    char *name;
    rendergraph_t *rgraph;
    hexcollmap_t *hitbox;
    hexcollmap_t *crushbox;
    ARRAY_DECL(struct state_rule*, rules)
} state_t;

typedef struct stateset {
    char *filename;
    ARRAY_DECL(struct state*, states)
} stateset_t;

typedef struct state_rule {
    struct state *state;
    ARRAY_DECL(struct state_cond*, conds)
    ARRAY_DECL(struct state_effect*, effects)
} state_rule_t;



typedef struct state_cond {
    const char *type;
    union {
        struct {
            int flags;
                /*
                    flags & 1: 0 -> any, 1 -> all
                    flags & 2: 0 -> no,  1 -> yes
                */
            hexcollmap_t *collmap;
        } coll;
        struct {
            bool yes;
            int kstate;
                /*
                    0 -> isdown
                    1 -> wasdown
                    2 -> wentdown
                */
            char c;
                /*
                    'f' -> forward
                    'b' -> back
                    'u' -> up
                    'd' -> down
                    'x' -> action
                */
        } key;
        int percent;
    } u;
} state_cond_t;

extern const char state_cond_type_false[];
extern const char state_cond_type_key[];
extern const char state_cond_type_coll[];
extern const char state_cond_type_chance[];
extern const char *state_cond_types[];



typedef struct state_effect {
    const char *type;
    union {
        char *msg;
        int delay;
        char *goto_name;
        vec_t vec;
        rot_t rot;
    } u;
} state_effect_t;

extern const char state_effect_type_print[];
extern const char state_effect_type_move[];
extern const char state_effect_type_rot[];
extern const char state_effect_type_turn[];
extern const char state_effect_type_goto[];
extern const char state_effect_type_delay[];
extern const char *state_effect_types[];




void stateset_cleanup(stateset_t *stateset);
int stateset_init(stateset_t *stateset, char *filename);
void stateset_dump(stateset_t *stateset, FILE *f);
int stateset_load(stateset_t *stateset, char *filename,
    prismelrenderer_t *prend, vecspace_t *space);
int stateset_parse(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space);
state_t *stateset_get_state(stateset_t *stateset, const char *name);


void state_cleanup(state_t *state);
int state_init(state_t *state, stateset_t *stateset, char *name,
    rendergraph_t *rgraph);
void state_dump(state_t *state, FILE *f, int n_spaces);

void state_rule_cleanup(state_rule_t *rule);
int state_rule_init(state_rule_t *rule, state_t *state);

#endif