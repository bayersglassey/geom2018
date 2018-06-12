#ifndef _ANIM_H_
#define _ANIM_H_

#include <stdio.h>

#include "array.h"
#include "geom.h"
#include "hexmap.h"
#include "lexer.h"

typedef struct hexcolltile {
    bool vert[1];
    bool edge[3];
    bool face[2];
} hexcolltile_t;

typedef struct hexcollmap {
    char *name;
    int w;
    int h;
    hexcolltile_t *data;
} hexcollmap_t;



typedef struct state {
    char *name;
    ARRAY_DECL(struct state_rule, rules)
} state_t;

typedef struct stateset {
    ARRAY_DECL(struct state, states)
} stateset_t;

typedef struct state_rule {
    ARRAY_DECL(struct state_cond, conds)
    ARRAY_DECL(struct state_effect, effects)
} state_rule_t;

typedef struct state_cond {
    const char *type;
    union {
        hexcollmap_t *collmap;
        char key; /* 'f' 'b' 'u' 'd' ... */
    } u;
} state_cond_t;

typedef struct state_effect {
    const char *type;
    union {
        vec_t vec;
        rot_t rot;
    } u;
} state_effect_t;




void stateset_cleanup(stateset_t *stateset);
int stateset_init(stateset_t *stateset);
void stateset_dump(stateset_t *stateset, FILE *f);
int stateset_load(stateset_t *stateset, const char *filename);
int stateset_parse(stateset_t *stateset, fus_lexer_t *lexer);


void state_cleanup(state_t *state);
int state_init(state_t *state, char *name);
void state_dump(state_t *state, FILE *f, int n_spaces);

#endif