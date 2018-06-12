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

typedef struct rule_effect {
    const char *type;
    union {
        vec_t vec;
        rot_t rot;
    } u;
} state_effect_t;


void state_cleanup(state_t *state);
int state_init(state_t *state, char *name);
void state_dump(state_t *state, FILE *f);
int state_parse(fus_lexer_t *lexer, state_t **state_ptr);

void state_rule_cleanup(state_rule_t *rule);
int state_rule_init(state_rule_t *rule);
void state_rule_dump(state_rule_t *rule, FILE *f);

#endif