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


struct stateset;


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
****************/

enum stateset_proc_type {
    STATESET_PROC_TYPE_NORMAL,
    STATESET_PROC_TYPE_ONSTATESETCHANGE,
    STATESET_PROC_TYPE_ONMAPCHANGE,
    STATESET_PROC_TYPES
};

static const char *stateset_proc_type_msg(int type){
    switch(type){
        case STATESET_PROC_TYPE_NORMAL: return "normal";
        case STATESET_PROC_TYPE_ONSTATESETCHANGE: return "onstatesetchange";
        case STATESET_PROC_TYPE_ONMAPCHANGE: return "onmapchange";
        default: return "unknown";
    }
}

typedef struct stateset_proc {
    /* A "procedure" which can be "called" using STATE_EFFECT_TYPE_CALL */
    int type; /* enum stateset_proc_type */
    const char *name;
    ARRAY_DECL(struct state_effect*, effects)

    /* Weakrefs: */
    struct stateset *stateset;
} stateset_proc_t;

void stateset_proc_cleanup(stateset_proc_t *handler);
void stateset_proc_init(stateset_proc_t *proc, struct stateset *stateset,
    int type, const char *name);


/****************
* STATE_CONTEXT *
****************/

typedef struct state_context_collmap_entry {
    /* Key/value pair */
    const char *name;
    struct hexcollmap *collmap;
} state_context_collmap_entry_t;

void state_context_collmap_entry_cleanup(state_context_collmap_entry_t *entry);

typedef struct state_context {
    /* Stuff which can be used by states within a stateset */
    ARRAY_DECL(char*, collmsgs)
    ARRAY_DECL(struct collmsg_handler, collmsg_handlers)
    ARRAY_DECL(struct stateset_proc, procs)
    ARRAY_DECL(state_context_collmap_entry_t*, collmaps)

    /* Weakrefs: */
    struct stateset *stateset;
    struct state_context *parent;
} state_context_t;

void state_context_cleanup(state_context_t *context);
void state_context_init(state_context_t *context, struct stateset *stateset,
    state_context_t *parent);

hexcollmap_t *state_context_get_collmap(state_context_t *context,
    const char *name);
stateset_proc_t *state_context_get_proc(state_context_t *context,
    const char *name);


/********
* STATE *
********/

typedef struct state {
    const char *name;
    hexcollmap_t *own_hitbox;
        /* For if we own the hitbox, as opposed to
        just pointing to one from context->collmaps. */
    bool safe;
    bool flying;
    ARRAY_DECL(struct state_rule*, rules)

    /* Weakrefs: */
    struct stateset *stateset;
    rendergraph_t *rgraph;
    state_context_t *context;
    hexcollmap_t *hitbox;
        /* If own_hitbox != NULL, then hitbox == own_hitbox */
} state_t;


/***********
* STATESET *
***********/

typedef struct stateset {
    const char *filename;
    ARRAY_DECL(struct state*, states)
    ARRAY_DECL(state_context_t*, contexts)
    vars_t vars;
    bool debug_collision;
    const char *default_state_name; /* stateset->states[i]->name */

    /* Weakrefs: */
    state_context_t *root_context;
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
        case STATE_COND_TYPE_EXISTS: return "exists";
        default: return "unknown";
    }
}

typedef struct state_cond {
    int type; /* enum state_cond_type */
    union {
        struct {
            int flags; /* ORed combination of enum anim_cond_flag values */
            valexpr_t collmsg_expr;
                /* If flags^ANIM_COND_FLAGS_BODIES, collmsg_expr specifies
                that only bodies with that collmsg should collide
                (or, and this is a bit of a hack, collmsg_expr can specify
                a stateset filename instead) */
            hexcollmap_t *own_collmap;
                /* own_collmap is for if we own the collmap, as opposed to
                just pointing to one from context->collmaps. */
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
    STATE_EFFECT_TYPE_NO_KEY_RESET,
    STATE_EFFECT_TYPE_PRINT,
    STATE_EFFECT_TYPE_MOVE,
    STATE_EFFECT_TYPE_ROT,
    STATE_EFFECT_TYPE_TURN,
    STATE_EFFECT_TYPE_RELOCATE,
    STATE_EFFECT_TYPE_GOTO,
    STATE_EFFECT_TYPE_CALL,
    STATE_EFFECT_TYPE_DELAY,
    STATE_EFFECT_TYPE_ADD_DELAY,
    STATE_EFFECT_TYPE_SPAWN,
    STATE_EFFECT_TYPE_PLAY,
    STATE_EFFECT_TYPE_DIE,
    STATE_EFFECT_TYPE_REMOVE,
    STATE_EFFECT_TYPE_INC,
    STATE_EFFECT_TYPE_DEC,
    STATE_EFFECT_TYPE_CONTINUE,
    STATE_EFFECT_TYPE_CONFUSED,
    STATE_EFFECT_TYPE_KEY,
    STATE_EFFECT_TYPE_SET,
    STATE_EFFECT_TYPE_UNSET,
    STATE_EFFECT_TYPE_IF,
    STATE_EFFECT_TYPE_AS,
    STATE_EFFECT_TYPES
};

static const char *state_effect_type_name(int type){
    switch(type){
        case STATE_EFFECT_TYPE_NOOP: return "noop";
        case STATE_EFFECT_TYPE_NO_KEY_RESET: return "no_key_reset";
        case STATE_EFFECT_TYPE_PRINT: return "print";
        case STATE_EFFECT_TYPE_MOVE: return "move";
        case STATE_EFFECT_TYPE_ROT: return "rot";
        case STATE_EFFECT_TYPE_TURN: return "turn";
        case STATE_EFFECT_TYPE_RELOCATE: return "relocate";
        case STATE_EFFECT_TYPE_GOTO: return "goto";
        case STATE_EFFECT_TYPE_CALL: return "call";
        case STATE_EFFECT_TYPE_DELAY: return "delay";
        case STATE_EFFECT_TYPE_ADD_DELAY: return "add_delay";
        case STATE_EFFECT_TYPE_SPAWN: return "spawn";
        case STATE_EFFECT_TYPE_PLAY: return "play";
        case STATE_EFFECT_TYPE_DIE: return "die";
        case STATE_EFFECT_TYPE_REMOVE: return "remove";
        case STATE_EFFECT_TYPE_INC: return "inc";
        case STATE_EFFECT_TYPE_DEC: return "dec";
        case STATE_EFFECT_TYPE_CONTINUE: return "continue";
        case STATE_EFFECT_TYPE_CONFUSED: return "confused";
        case STATE_EFFECT_TYPE_KEY: return "key";
        case STATE_EFFECT_TYPE_SET: return "set";
        case STATE_EFFECT_TYPE_UNSET: return "unset";
        case STATE_EFFECT_TYPE_IF: return "if";
        case STATE_EFFECT_TYPE_AS: return "as";
        default: return "unknown";
    }
}

typedef struct state_effect_goto {
    const char *name;
    bool immediate;
    bool delay;
    int add_delay;
} state_effect_goto_t;

typedef struct state_effect_call {
    state_context_t *state_context;
    const char *name;
} state_effect_call_t;

typedef struct state_effect_spawn {
    valexpr_t stateset_filename_expr;
    valexpr_t state_name_expr;
    const char *palmapper_name;
    hexgame_location_t loc;
    ARRAY_DECL(struct state_effect*, effects)
} state_effect_spawn_t;

typedef struct state_effect {
    int type; /* enum state_effect_type */
    union {
        const char *var_name;
        state_effect_goto_t gotto;
        state_effect_call_t call;
        int dead; /* enum body_dead */
        state_effect_spawn_t spawn;
        const char *play_filename;
        vec_t vec;
        rot_t rot;
        int boolean; /* enum effect_boolean */
        int i;
        valexpr_t expr;
        struct {
            valexpr_t loc_expr;
            valexpr_t map_filename_expr;
            valexpr_t stateset_filename_expr;
            valexpr_t state_name_expr;
        } relocate;
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

void stateset_cleanup(stateset_t *stateset);
int stateset_init(stateset_t *stateset, const char *filename);
void stateset_dump(stateset_t *stateset, FILE *file, int depth);
int stateset_load(stateset_t *stateset, const char *filename,
    prismelrenderer_t *prend, vecspace_t *space);
int stateset_parse(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space);
state_t *stateset_get_state(stateset_t *stateset, const char *name);

void state_cleanup(state_t *state);
int state_init(state_t *state, stateset_t *stateset, const char *name,
    state_context_t *parent_context);
void state_dump(state_t *state, FILE *file, int depth);

void state_rule_cleanup(state_rule_t *rule);
int state_rule_init(state_rule_t *rule, state_t *state);
void state_rule_dump(state_rule_t *rule, FILE *file, int depth);

void state_cond_cleanup(state_cond_t *cond);
void state_cond_dump(state_cond_t *cond, FILE *file, int depth);

void state_effect_cleanup(state_effect_t *effect);
void state_effect_dump(state_effect_t *effect, FILE *file, int depth);

#endif