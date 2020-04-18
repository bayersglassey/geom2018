#ifndef _ANIM_H_
#define _ANIM_H_

#include <stdio.h>

#include "array.h"
#include "geom.h"
#include "lexer.h"
#include "hexmap.h"
#include "prismelrenderer.h"

#define ANIM_KEY_CS "xyudlrfb"


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


typedef struct collmsg_handler {
    /* When colliding with another body who is "sending" the given collmsg,
    goto the associated state */
    char *msg;
    char *state_name;
} collmsg_handler_t;

void collmsg_handler_cleanup(collmsg_handler_t *handler);



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

typedef struct stateset {
    char *filename;
    ARRAY_DECL(char*, collmsgs)
    ARRAY_DECL(struct collmsg_handler, collmsg_handlers)
    ARRAY_DECL(struct state*, states)
    bool debug_collision;
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
                    flags & 4: 1 -> water
                    flags & 8: 0 -> against map, 1 -> against bodies
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
        struct {
            ARRAY_DECL(struct state_cond*, conds)
        } subconds;
    } u;
} state_cond_t;

extern const char state_cond_type_false[];
extern const char state_cond_type_key[];
extern const char state_cond_type_coll[];
extern const char state_cond_type_chance[];
extern const char state_cond_type_any[];
extern const char state_cond_type_all[];
extern const char *state_cond_types[];



typedef struct state_effect_goto {
    char *name;
    bool immediate;
} state_effect_goto_t;

typedef struct state_effect_spawn {
    char *stateset_filename;
    char *state_name;
    char *palmapper_name;
    vec_t pos;
    rot_t rot;
    bool turn;
} state_effect_spawn_t;

typedef struct state_effect {
    const char *type;
    union {
        char *msg;
        int delay;
        state_effect_goto_t gotto;
        int dead; /* enum body_dead */
        state_effect_spawn_t spawn;
        char *play_filename;
        vec_t vec;
        rot_t rot;
        int boolean; /* enum effect_boolean */
    } u;
} state_effect_t;

extern const char state_effect_type_print[];
extern const char state_effect_type_move[];
extern const char state_effect_type_rot[];
extern const char state_effect_type_turn[];
extern const char state_effect_type_goto[];
extern const char state_effect_type_delay[];
extern const char state_effect_type_spawn[];
extern const char state_effect_type_play[];
extern const char state_effect_type_die[];
extern const char state_effect_type_confused[];
extern const char *state_effect_types[];




void stateset_cleanup(stateset_t *stateset);
int stateset_init(stateset_t *stateset, char *filename);
int stateset_load(stateset_t *stateset, char *filename, vars_t *vars,
    prismelrenderer_t *prend, vecspace_t *space);
int stateset_parse(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space);
state_t *stateset_get_state(stateset_t *stateset, const char *name);


void state_cleanup(state_t *state);
int state_init(state_t *state, stateset_t *stateset, char *name,
    rendergraph_t *rgraph);

void state_rule_cleanup(state_rule_t *rule);
int state_rule_init(state_rule_t *rule, state_t *state);

#endif