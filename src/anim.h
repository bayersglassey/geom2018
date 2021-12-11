#ifndef _ANIM_H_
#define _ANIM_H_

#include <stdio.h>

#include "array.h"
#include "geom.h"
#include "lexer.h"
#include "hexmap.h"
#include "valexpr.h"
#include "prismelrenderer.h"

#define ANIM_KEY_CS "xyudlrfb"
/*
Meaning of each character:
'x' -> action (e.g. spit)
'y' -> action2 (e.g. look up)
'u' -> up
'd' -> down
'l' -> left
'r' -> right
'f' -> forward
'b' -> back
*/


enum anim_cond_flag {
    ANIM_COND_FLAGS_ALL    = 1,
    ANIM_COND_FLAGS_YES    = 2,
    ANIM_COND_FLAGS_WATER  = 4,
    ANIM_COND_FLAGS_BODIES = 8,
};


/* One is "mostly" dead when one makes a jump and slams into a wall.
One is "all" dead when one is crushed by a bad guy or whatever.
The main difference between each kind of death is where you
will respawn: when mostly dead, you respawn from where you jumped,
when all dead, you respawn at last checkpoint. */
enum body_dead {
    BODY_NOT_DEAD,
    BODY_MOSTLY_DEAD,
    BODY_ALL_DEAD
};

enum effect_boolean {
    EFFECT_BOOLEAN_TRUE,
    EFFECT_BOOLEAN_FALSE,
    EFFECT_BOOLEAN_TOGGLE
};

enum effect_as {
    EFFECT_AS_YOU
};


/******************
* COLLMSG_HANDLER *
******************/

typedef struct collmsg_handler {
    /* When colliding with another body who is "sending" the given collmsg,
    goto the associated state (old behaviour) or apply the associated
    effects (new behaviour) */
    char *msg;
    ARRAY_DECL(struct state_effect*, effects)
} collmsg_handler_t;

void collmsg_handler_cleanup(collmsg_handler_t *handler);
void collmsg_handler_init(collmsg_handler_t *handler, char *msg);
struct state_effect_goto;


/****************
* STATESET_PROC *
*****************/

typedef struct stateset_proc {
    /* A "procedure" which can be "called" using STATE_EFFECT_TYPE_CALL */
    const char *name;
    ARRAY_DECL(struct state_effect*, effects)
} stateset_proc_t;

void stateset_proc_cleanup(stateset_proc_t *handler);
void stateset_proc_init(stateset_proc_t *handler, const char *msg);


/********
* STATE *
********/

typedef struct state {
    struct stateset *stateset;
    const char *name;
    rendergraph_t *rgraph;
    hexcollmap_t *own_hitbox;
        /* For if we own the hitbox, as opposed to
        just pointing to one from stateset->collmaps. */
    hexcollmap_t *hitbox;
        /* If own_hitbox != NULL, then hitbox == own_hitbox */
    bool safe;
    bool flying;
    ARRAY_DECL(char*, collmsgs)
    ARRAY_DECL(struct collmsg_handler, collmsg_handlers)
    ARRAY_DECL(struct state_rule*, rules)
} state_t;


/***********
* STATESET *
***********/

typedef struct stateset_collmap_entry {
    /* Key/value pair in the array stateset->collmaps. */
    const char *name;
    struct hexcollmap *collmap;
} stateset_collmap_entry_t;

typedef struct stateset {
    const char *filename;
    ARRAY_DECL(char*, collmsgs)
    ARRAY_DECL(struct collmsg_handler, collmsg_handlers)
    ARRAY_DECL(struct stateset_proc, procs)
    ARRAY_DECL(struct state*, states)
    ARRAY_DECL(stateset_collmap_entry_t*, collmaps)
    bool debug_collision;
    const char *default_state_name; /* stateset->states[i]->name */
} stateset_t;


/*******
* RULE *
*******/

typedef struct state_rule {
    struct state *state;
    ARRAY_DECL(struct state_cond*, conds)
    ARRAY_DECL(struct state_effect*, effects)
} state_rule_t;


/*******
* COND *
*******/

enum state_cond_type {
    STATE_COND_TYPE_FALSE,
    STATE_COND_TYPE_TRUE,
    STATE_COND_TYPE_KEY,
    STATE_COND_TYPE_COLL,
    STATE_COND_TYPE_CHANCE,
    STATE_COND_TYPE_ANY,
    STATE_COND_TYPE_ALL,
    STATE_COND_TYPE_NOT,
    STATE_COND_TYPE_EXPR,
    STATE_COND_TYPE_GET_BOOL,
    STATE_COND_TYPE_EXISTS,
    STATE_COND_TYPES
};

static const char *state_cond_type_name(int type){
    switch(type){
        case STATE_COND_TYPE_FALSE: return "false";
        case STATE_COND_TYPE_TRUE: return "true";
        case STATE_COND_TYPE_KEY: return "key";
        case STATE_COND_TYPE_COLL: return "coll";
        case STATE_COND_TYPE_CHANCE: return "chance";
        case STATE_COND_TYPE_ANY: return "any";
        case STATE_COND_TYPE_ALL: return "all";
        case STATE_COND_TYPE_NOT: return "not";
        case STATE_COND_TYPE_EXPR: return "expr";
        case STATE_COND_TYPE_GET_BOOL: return "get_bool";
        case STATE_COND_TYPE_EXISTS: return "exists";
        default: return "unknown";
    }
}

enum state_cond_expr_op {
    STATE_COND_EXPR_OP_EQ,
    STATE_COND_EXPR_OP_NE,
    STATE_COND_EXPR_OP_LT,
    STATE_COND_EXPR_OP_LE,
    STATE_COND_EXPR_OP_GT,
    STATE_COND_EXPR_OP_GE,
};

typedef struct state_cond {
    int type; /* enum state_cond_type */
    union {
        struct {
            int op; /* enum state_cond_expr_op */
            valexpr_t val1_expr;
            valexpr_t val2_expr;
        } expr;
        struct {
            int flags; /* ORed combination of enum anim_cond_flag values */
            char *collmsg;
                /* If flags^ANIM_COND_FLAGS_BODIES, collmsg specifies
                that only bodies with this collmsg should collide */
            hexcollmap_t *own_collmap;
                /* own_collmap is for if we own the collmap, as opposed to
                just pointing to one from stateset->collmaps. */
            hexcollmap_t *collmap;
                /* Cannot be NULL. If own_collmap != NULL, then
                collmap == own_collmap */
        } coll;
        struct {
            bool yes;
            int kstate;
                /*
                    0 -> isdown
                    1 -> wasdown
                    2 -> wentdown
                */
            char c; /* See: ANIM_KEY_CS */
        } key;
        struct {
            /* ratio: a / b */
            int a;
            int b;
        } ratio;
        struct {
            ARRAY_DECL(struct state_cond*, conds)
        } subconds;
        valexpr_t valexpr;
    } u;
} state_cond_t;


/*********
* EFFECT *
*********/

enum state_effect_type {
    STATE_EFFECT_TYPE_NOOP,
    STATE_EFFECT_TYPE_PRINT,
    STATE_EFFECT_TYPE_PRINT_VAR,
    STATE_EFFECT_TYPE_PRINT_VARS,
    STATE_EFFECT_TYPE_MOVE,
    STATE_EFFECT_TYPE_ROT,
    STATE_EFFECT_TYPE_TURN,
    STATE_EFFECT_TYPE_GOTO,
    STATE_EFFECT_TYPE_CALL,
    STATE_EFFECT_TYPE_DELAY,
    STATE_EFFECT_TYPE_SPAWN,
    STATE_EFFECT_TYPE_PLAY,
    STATE_EFFECT_TYPE_DIE,
    STATE_EFFECT_TYPE_INC,
    STATE_EFFECT_TYPE_DEC,
    STATE_EFFECT_TYPE_CONTINUE,
    STATE_EFFECT_TYPE_CONFUSED,
    STATE_EFFECT_TYPE_KEY,
    STATE_EFFECT_TYPE_SET,
    STATE_EFFECT_TYPE_UNSET,
    STATE_EFFECT_TYPE_SET_LABEL,
    STATE_EFFECT_TYPE_IF,
    STATE_EFFECT_TYPE_AS,
    STATE_EFFECT_TYPES
};

static const char *state_effect_type_name(int type){
    switch(type){
        case STATE_EFFECT_TYPE_NOOP: return "noop";
        case STATE_EFFECT_TYPE_PRINT: return "print";
        case STATE_EFFECT_TYPE_PRINT_VAR: return "print_var";
        case STATE_EFFECT_TYPE_PRINT_VARS: return "print_vars";
        case STATE_EFFECT_TYPE_MOVE: return "move";
        case STATE_EFFECT_TYPE_ROT: return "rot";
        case STATE_EFFECT_TYPE_TURN: return "turn";
        case STATE_EFFECT_TYPE_GOTO: return "goto";
        case STATE_EFFECT_TYPE_CALL: return "call";
        case STATE_EFFECT_TYPE_DELAY: return "delay";
        case STATE_EFFECT_TYPE_SPAWN: return "spawn";
        case STATE_EFFECT_TYPE_PLAY: return "play";
        case STATE_EFFECT_TYPE_DIE: return "die";
        case STATE_EFFECT_TYPE_INC: return "inc";
        case STATE_EFFECT_TYPE_DEC: return "dec";
        case STATE_EFFECT_TYPE_CONTINUE: return "continue";
        case STATE_EFFECT_TYPE_CONFUSED: return "confused";
        case STATE_EFFECT_TYPE_KEY: return "key";
        case STATE_EFFECT_TYPE_SET: return "set";
        case STATE_EFFECT_TYPE_UNSET: return "unset";
        case STATE_EFFECT_TYPE_SET_LABEL: return "set_label";
        case STATE_EFFECT_TYPE_IF: return "if";
        case STATE_EFFECT_TYPE_AS: return "as";
        default: return "unknown";
    }
}

typedef struct state_effect_goto {
    const char *name;
    bool immediate;
    bool delay;
} state_effect_goto_t;

typedef struct state_effect_call {
    const char *name;
} state_effect_call_t;

typedef struct state_effect_spawn {
    const char *stateset_filename;
    const char *state_name;
    const char *palmapper_name;
    hexgame_location_t loc;
    ARRAY_DECL(struct state_effect*, effects)
} state_effect_spawn_t;

typedef struct state_effect {
    int type; /* enum state_effect_type */
    union {
        char *msg;
        const char *var_name;
        int delay;
        state_effect_goto_t gotto;
        state_effect_call_t call;
        int dead; /* enum body_dead */
        state_effect_spawn_t spawn;
        const char *play_filename;
        vec_t vec;
        rot_t rot;
        int boolean; /* enum effect_boolean */
        int i;
        struct {
            int action;
                /*
                    0x0 -> none
                    0x1 -> keydown
                    0x2 -> keyup
                    0x3 -> keypress (down + up on same frame)
                */
            char c; /* See: ANIM_KEY_CS */
        } key;
        struct {
            valexpr_t var_expr;
            valexpr_t val_expr;
        } set;
        struct state_effect_ite *ite;
        struct {
            int type; /* enum effect_as */
            ARRAY_DECL(struct state_effect*, sub_effects)
        } as;
    } u;
} state_effect_t;

typedef struct state_effect_ite {
    /* ite: if-then-else */
    ARRAY_DECL(struct state_cond*, conds)
    ARRAY_DECL(struct state_effect*, then_effects)
    ARRAY_DECL(struct state_effect*, else_effects)
} state_effect_ite_t;



/*************
* PROTOTYPES *
*************/

void stateset_collmap_entry_cleanup(stateset_collmap_entry_t *entry);

void stateset_cleanup(stateset_t *stateset);
int stateset_init(stateset_t *stateset, const char *filename);
void stateset_dump(stateset_t *stateset, FILE *file, int depth);
hexcollmap_t *stateset_get_collmap(stateset_t *stateset, const char *name);
collmsg_handler_t *stateset_get_collmsg_handler(stateset_t *stateset,
    const char *msg);
stateset_proc_t *stateset_get_proc(stateset_t *stateset, const char *name);
int stateset_load(stateset_t *stateset, const char *filename, vars_t *vars,
    prismelrenderer_t *prend, vecspace_t *space);
int stateset_parse(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space);
state_t *stateset_get_state(stateset_t *stateset, const char *name);


void state_cleanup(state_t *state);
int state_init(state_t *state, stateset_t *stateset, const char *name);
void state_dump(state_t *state, FILE *file, int depth);

void state_rule_cleanup(state_rule_t *rule);
int state_rule_init(state_rule_t *rule, state_t *state);
void state_rule_dump(state_rule_t *rule, FILE *file, int depth);

void state_cond_cleanup(state_cond_t *cond);
void state_cond_dump(state_cond_t *cond, FILE *file, int depth);

void state_effect_cleanup(state_effect_t *effect);
void state_effect_dump(state_effect_t *effect, FILE *file, int depth);

#endif