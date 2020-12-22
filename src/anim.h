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


/* Need these declarations for state_cond_match, state_effect_apply,
collmsg_handler_apply */
struct hexgame;
struct body;
struct actor;


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
int collmsg_handler_apply(collmsg_handler_t *handler,
    struct hexgame *game, struct body *body, struct actor *actor,
    bool *continues_ptr);


/********
* STATE *
********/

typedef struct state {
    struct stateset *stateset;
    char *name;
    rendergraph_t *rgraph;
    hexcollmap_t *hitbox;
    bool safe;
    bool flying;
    ARRAY_DECL(char*, collmsgs)
    ARRAY_DECL(struct collmsg_handler, collmsg_handlers)
    ARRAY_DECL(struct state_rule*, rules)
} state_t;


/***********
* STATESET *
***********/

typedef struct stateset {
    char *filename;
    ARRAY_DECL(char*, collmsgs)
    ARRAY_DECL(struct collmsg_handler, collmsg_handlers)
    ARRAY_DECL(struct state*, states)
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

enum state_cond_expr_op {
    STATE_COND_EXPR_OP_EQ,
    STATE_COND_EXPR_OP_NE,
    STATE_COND_EXPR_OP_LT,
    STATE_COND_EXPR_OP_LE,
    STATE_COND_EXPR_OP_GT,
    STATE_COND_EXPR_OP_GE,
};

typedef struct state_cond {
    const char *type;
    union {
        struct {
            char *var_name;
            int op; /* enum state_cond_expr_op */
            int value;
        } expr;
        struct {
            int flags; /* ORed combination of enum anim_cond_flag values */
            char *collmsg;
                /* If flags^ANIM_COND_FLAGS_BODIES, collmsg specifies
                that only bodies with this collmsg should collide */
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

extern const char state_cond_type_false[];
extern const char state_cond_type_key[];
extern const char state_cond_type_coll[];
extern const char state_cond_type_chance[];
extern const char state_cond_type_any[];
extern const char state_cond_type_all[];
extern const char state_cond_type_not[];
extern const char state_cond_type_expr[];
extern const char state_cond_type_get_bool[];
extern const char *state_cond_types[];


/*********
* EFFECT *
*********/

typedef struct state_effect_goto {
    char *name;
    bool immediate;
} state_effect_goto_t;

typedef struct state_effect_spawn {
    char *stateset_filename;
    char *state_name;
    char *palmapper_name;
    hexgame_location_t loc;
} state_effect_spawn_t;

typedef struct state_effect {
    const char *type;
    union {
        char *msg;
        char *var_name;
        int delay;
        state_effect_goto_t gotto;
        int dead; /* enum body_dead */
        state_effect_spawn_t spawn;
        char *play_filename;
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
    } u;
} state_effect_t;

extern const char state_effect_type_print[];
extern const char state_effect_type_print_var[];
extern const char state_effect_type_print_vars[];
extern const char state_effect_type_move[];
extern const char state_effect_type_rot[];
extern const char state_effect_type_turn[];
extern const char state_effect_type_goto[];
extern const char state_effect_type_delay[];
extern const char state_effect_type_spawn[];
extern const char state_effect_type_play[];
extern const char state_effect_type_die[];
extern const char state_effect_type_zero[];
extern const char state_effect_type_inc[];
extern const char state_effect_type_continue[];
extern const char state_effect_type_confused[];
extern const char state_effect_type_key[];
extern const char state_effect_type_set[];
extern const char *state_effect_types[];


/*************
* PROTOTYPES *
*************/

void stateset_cleanup(stateset_t *stateset);
int stateset_init(stateset_t *stateset, char *filename);
void stateset_dump(stateset_t *stateset, FILE *file, int depth);
int stateset_load(stateset_t *stateset, char *filename, vars_t *vars,
    prismelrenderer_t *prend, vecspace_t *space);
int stateset_parse(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space);
state_t *stateset_get_state(stateset_t *stateset, const char *name);


void state_cleanup(state_t *state);
int state_init(state_t *state, stateset_t *stateset, char *name,
    rendergraph_t *rgraph);
void state_dump(state_t *state, FILE *file, int depth);

void state_rule_cleanup(state_rule_t *rule);
int state_rule_init(state_rule_t *rule, state_t *state);
void state_rule_dump(state_rule_t *rule, FILE *file, int depth);

void state_cond_cleanup(state_cond_t *cond);
void state_cond_dump(state_cond_t *cond, FILE *file, int depth);
int state_cond_match(state_cond_t *cond,
    struct hexgame *game, struct body *body, struct actor *actor,
    bool *matched_ptr);

void state_effect_cleanup(state_effect_t *effect);
void state_effect_dump(state_effect_t *effect, FILE *file, int depth);
int state_effect_apply(state_effect_t *effect,
    struct hexgame *game, struct body *body, struct actor *actor,
    state_effect_goto_t **gotto_ptr, bool *continues_ptr);

#endif