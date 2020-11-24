

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "anim.h"
#include "array.h"
#include "hexmap.h"
#include "util.h"
#include "prismelrenderer.h"
#include "lexer.h"
#include "lexer_macros.h"


static void _print_tabs(FILE *file, int depth){
    for(int i = 0; i < depth; i++)fputs("  ", file);
}


void collmsg_handler_cleanup(collmsg_handler_t *handler){
    free(handler->msg);
    free(handler->state_name);
    ARRAY_FREE_PTR(state_effect_t*, handler->effects, state_effect_cleanup)
}

void collmsg_handler_init(collmsg_handler_t *handler,
    char *msg, char *state_name
){
    handler->msg = msg;
    handler->state_name = state_name;
    ARRAY_INIT(handler->effects)
}

static int _parse_collmsg_handler(fus_lexer_t *lexer,
    collmsg_handler_t *handler, prismelrenderer_t *prend, vecspace_t *space
){
    INIT
    char *msg;
    char *state_name;

    GET_STR(msg)

    GET("(")
    GET("goto")
    GET_NAME(state_name)
    /*
    while(true){
        if(GOT(")"))break;
        ARRAY_PUSH_NEW(state_effect_t*, handler->effects, effect)
        err = _parse_effect(lexer, prend, space, effect);
        if(err)return err;
    }
    */
    GET(")")

    collmsg_handler_init(handler, msg, state_name);
    return 0;
}



/************
 * STATESET *
 ************/

void stateset_cleanup(stateset_t *stateset){
    free(stateset->filename);
    ARRAY_FREE_PTR(char*, stateset->collmsgs, (void))
    ARRAY_FREE(collmsg_handler_t, stateset->collmsg_handlers,
        collmsg_handler_cleanup)
    ARRAY_FREE_PTR(state_t*, stateset->states, state_cleanup)
}

int stateset_init(stateset_t *stateset, char *filename){
    stateset->filename = filename;
    ARRAY_INIT(stateset->collmsgs)
    ARRAY_INIT(stateset->collmsg_handlers)
    ARRAY_INIT(stateset->states)
    stateset->debug_collision = false;
    return 0;
}

void stateset_dump(stateset_t *stateset, FILE *file, int depth){
    _print_tabs(file, depth);
    fprintf(file, "%s:\n", stateset->filename);
    for(int i = 0; i < stateset->states_len; i++){
        state_t *state = stateset->states[i];
        state_dump(state, file, depth+1);
    }
}

int stateset_load(stateset_t *stateset, char *filename, vars_t *vars,
    prismelrenderer_t *prend, vecspace_t *space
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init_with_vars(&lexer, text, filename, vars);
    if(err)return err;

    err = stateset_init(stateset, filename);
    if(err)return err;

    err = stateset_parse(stateset, &lexer, prend, space);
    if(err)return err;

    fus_lexer_cleanup(&lexer);

    free(text);
    return 0;
}

static int _parse_cond(fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space,
    state_cond_t *cond
){
    INIT
    if(GOT("false")){
        NEXT
        cond->type = state_cond_type_false;
    }else if(GOT("key")){
        NEXT
        GET("(")

        bool yes = true;
        if(GOT("not")){
            NEXT
            yes = false;
        }

        int kstate;
        if(GOT("isdown")){
            kstate = 0;
        }else if(GOT("wasdown")){
            kstate = 1;
        }else if(GOT("wentdown")){
            kstate = 2;
        }else{
            return UNEXPECTED(
                "isdown or wasdown or wentdown");
        }
        NEXT

        char *name;
        GET_NAME(name)

        char c = name[0];
        if(strlen(name) != 1 || !strchr(ANIM_KEY_CS, c)){
            UNEXPECTED(
                "one of the characters: " ANIM_KEY_CS);
        }

        cond->type = state_cond_type_key;
        cond->u.key.kstate = kstate;
        cond->u.key.c = c;
        cond->u.key.yes = yes;
        free(name);

        GET(")")
    }else if(GOT("coll")){
        NEXT
        GET("(")

        int flags = 0;

        if(GOT("water")){
            NEXT
            flags ^= ANIM_COND_FLAGS_WATER;
        }else if(GOT("bodies")){
            NEXT
            flags ^= ANIM_COND_FLAGS_BODIES;
        }

        if(GOT("all"))flags ^= ANIM_COND_FLAGS_ALL;
        else if(GOT("any"))/* don't do nuthin */;
        else return UNEXPECTED("all or any");
        NEXT

        if(GOT("yes"))flags ^= ANIM_COND_FLAGS_YES;
        else if(GOT("no"))/* dinnae move a muscle */;
        else return UNEXPECTED("yes or no");
        NEXT

        hexcollmap_t *collmap = calloc(1, sizeof(*collmap));
        if(collmap == NULL)return 1;
        hexcollmap_init(collmap, space,
            strdup(lexer->filename));
        err = hexcollmap_parse(collmap, lexer, true);
        if(err)return err;

        GET(")")

        cond->type = state_cond_type_coll;
        cond->u.coll.collmap = collmap;
        cond->u.coll.flags = flags;
    }else if(GOT("chance")){
        NEXT
        GET("(")
        int a;
        int b = 100;
        GET_INT(a)
        if(GOT("%")){
            NEXT
        }else{
            GET("/")
            GET_INT(b)
        }
        GET(")")
        cond->type = state_cond_type_chance;
        cond->u.ratio.a = a;
        cond->u.ratio.b = b;
    }else if(GOT("any") || GOT("all") || GOT("not")){
        cond->type =
            GOT("any")? state_cond_type_any:
            GOT("all")? state_cond_type_all:
            state_cond_type_not;
        ARRAY_INIT(cond->u.subconds.conds)

        NEXT
        GET("(")
        while(1){
            if(GOT(")"))break;
            ARRAY_PUSH_NEW(state_cond_t*, cond->u.subconds.conds, subcond)
            err = _parse_cond(lexer, prend, space, subcond);
            if(err)return err;
        }
        NEXT
    }else if(GOT("expr")){
        cond->type = state_cond_type_expr;
        cond->u.expr.var_name = NULL;
        NEXT
        GET("(")
        if(GOT("=="))cond->u.expr.op = STATE_COND_EXPR_OP_EQ;
        else if(GOT("!="))cond->u.expr.op = STATE_COND_EXPR_OP_NE;
        else if(GOT( "<"))cond->u.expr.op = STATE_COND_EXPR_OP_LT;
        else if(GOT("<="))cond->u.expr.op = STATE_COND_EXPR_OP_LE;
        else if(GOT( ">"))cond->u.expr.op = STATE_COND_EXPR_OP_GT;
        else if(GOT(">="))cond->u.expr.op = STATE_COND_EXPR_OP_GE;
        else return UNEXPECTED("== or != or < or <= or > or >=");
        NEXT
        GET_NAME(cond->u.expr.var_name)
        GET_INT(cond->u.expr.value)
        GET(")")
    }else{
        return UNEXPECTED(NULL);
    }
    return 0;
}

static int _parse_effect(fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space,
    state_effect_t *effect
){
    INIT
    if(GOT("print")){
        NEXT
        GET("(")
        effect->type = state_effect_type_print;
        GET_STR(effect->u.msg)
        GET(")")
    }else if(GOT("print_int")){
        NEXT
        GET("(")
        effect->type = state_effect_type_print_int;
        GET_NAME(effect->u.var_name)
        GET(")")
    }else if(GOT("move")){
        NEXT
        GET("(")
        effect->type = state_effect_type_move;
        for(int i = 0; i < space->dims; i++){
            GET_INT(effect->u.vec[i]);
        }
        GET(")")
    }else if(GOT("rot")){
        NEXT
        GET("(")
        int rot;
        GET_INT(rot)
        effect->type = state_effect_type_rot;
        effect->u.rot = rot_contain(space->rot_max, rot);
        GET(")")
    }else if(GOT("turn")){
        NEXT
        effect->type = state_effect_type_turn;
    }else if(GOT("goto")){
        NEXT

        bool immediate = false;
        if(GOT("immediate")){
            NEXT
            immediate = true;
        }

        GET("(")

        char *goto_name;
        GET_NAME(goto_name)

        effect->type = state_effect_type_goto;
        effect->u.gotto.name = goto_name;
        effect->u.gotto.immediate = immediate;

        GET(")")
    }else if(GOT("delay")){
        NEXT
        GET("(")

        int delay;
        GET_INT(delay)

        effect->type = state_effect_type_delay;
        effect->u.delay = delay;

        GET(")")
    }else if(GOT("spawn")){
        NEXT
        GET("(")

        state_effect_spawn_t spawn = {0};
        GET_STR(spawn.stateset_filename)
        GET_STR(spawn.state_name)
        GET("(")
        GET_INT(spawn.loc.pos[0])
        GET_INT(spawn.loc.pos[1])
        GET(")")
        GET_INT(spawn.loc.rot)
        GET_BOOL(spawn.loc.turn)

        effect->type = state_effect_type_spawn;
        effect->u.spawn = spawn;

        GET(")")
    }else if(GOT("die")){
        NEXT
        effect->type = state_effect_type_die;
        if(GOT("mostly")){
            NEXT
            effect->u.dead = BODY_MOSTLY_DEAD;
        }else effect->u.dead = BODY_ALL_DEAD;
    }else if(GOT("zero")){
        effect->type = state_effect_type_zero;
        effect->u.var_name = NULL;
        NEXT
        GET("(")
        GET_NAME(effect->u.var_name)
        GET(")")
    }else if(GOT("inc")){
        effect->type = state_effect_type_inc;
        effect->u.var_name = NULL;
        NEXT
        GET("(")
        GET_NAME(effect->u.var_name)
        GET(")")
    }else if(GOT("continue")){
        effect->type = state_effect_type_continue;
        NEXT
    }else if(GOT("confused")){
        NEXT
        GET("(")
        int boolean;
        if(GOT("yes"))boolean = EFFECT_BOOLEAN_TRUE;
        else if(GOT("no"))boolean = EFFECT_BOOLEAN_FALSE;
        else if(GOT("toggle"))boolean = EFFECT_BOOLEAN_TOGGLE;
        else {
            return UNEXPECTED("yes or no or toggle");
        }
        NEXT
        effect->type = state_effect_type_confused;
        effect->u.boolean = boolean;
        GET(")")
    }else if(GOT("key")){
        NEXT
        GET("(")

        int action;
        if(GOT("down")){
            action = 0x1;
        }else if(GOT("up")){
            action = 0x2;
        }else if(GOT("press")){
            action = 0x3;
        }else{
            return UNEXPECTED(
                "down or up or press");
        }
        NEXT

        char *name;
        GET_NAME(name)

        char c = name[0];
        if(strlen(name) != 1 || !strchr(ANIM_KEY_CS, c)){
            UNEXPECTED(
                "one of the characters: " ANIM_KEY_CS);
        }

        effect->type = state_effect_type_key;
        effect->u.key.action = action;
        effect->u.key.c = c;
        free(name);

        GET(")")
    }else if(GOT("play")){
        NEXT
        GET("(")

        char *play_filename;
        GET_STR(play_filename)

        effect->type = state_effect_type_play;
        effect->u.play_filename = play_filename;

        GET(")")
    }else{
        return UNEXPECTED(NULL);
    }
    return 0;
}

static int _stateset_parse(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space
){
    INIT
    while(1){
        if(DONE)break;

        if(GOT("import")){
            NEXT

            /* We use _fus_lexer_get_str to avoid calling fus_lexer_next until after
            the call to fus_lexer_init_with_vars is done, to make sure we don't modify
            lexer->vars first */
            char *filename;
            err = _fus_lexer_get_str(lexer, &filename);
            if(err)return err;

            char *text = load_file(filename);
            if(text == NULL)return 1;

            fus_lexer_t sublexer;
            err = fus_lexer_init_with_vars(&sublexer, text, filename,
                lexer->vars);
            if(err)return err;

            err = _stateset_parse(stateset, &sublexer, prend, space);
            if(err)return err;

            /* We now call fus_lexer_next manually, see call to _fus_lexer_get_str
            above */
            err = fus_lexer_next(lexer);
            if(err)return err;

            fus_lexer_cleanup(&sublexer);
            free(filename);
            continue;
        }

        char *name;
        GET_NAME(name)
        GET("(")

        rendergraph_t *rgraph = NULL;
        if(GOT("rgraph")){
            char *rgraph_name;
            NEXT
            GET("(")
            GET_STR(rgraph_name)
            GET(")")
            rgraph = prismelrenderer_get_rendergraph(
                prend, rgraph_name);
            if(rgraph == NULL){
                fprintf(stderr, "Couldn't find shape: %s\n", rgraph_name);
                free(rgraph_name); return 2;}
            free(rgraph_name);
        }

        ARRAY_PUSH_NEW(state_t*, stateset->states, state)
        err = state_init(state, stateset, name, rgraph);
        if(err)return err;

        if(GOT("unsafe")){
            NEXT
            state->safe = false;
        }
        if(GOT("flying")){
            NEXT
            state->flying = true;
        }
        if(GOT("collmsgs")){
            NEXT
            GET("(")
            while(!GOT(")")){
                char *msg;
                GET_STR(msg)
                ARRAY_PUSH(char*, state->collmsgs, msg)
            }
            NEXT
        }
        if(GOT("hitbox")){
            NEXT
            GET("(")

            hexcollmap_t *collmap = calloc(1, sizeof(*collmap));
            if(collmap == NULL)return 1;
            hexcollmap_init(collmap, space,
                strdup(lexer->filename));
            err = hexcollmap_parse(collmap, lexer, true);
            if(err)return err;
            state->hitbox = collmap;

            GET(")")
        }
        while(GOT("on")){
            NEXT
            collmsg_handler_t handler;
            err = _parse_collmsg_handler(lexer, &handler, prend, space);
            if(err)return err;
            ARRAY_PUSH(collmsg_handler_t, state->collmsg_handlers, handler)
        }

        while(1){
            if(GOT(")"))break;

            ARRAY_PUSH_NEW(state_rule_t*, state->rules, rule)
            err = state_rule_init(rule, state);
            if(err)return err;

            GET("if")
            GET("(")
            while(1){
                if(GOT(")"))break;
                ARRAY_PUSH_NEW(state_cond_t*, rule->conds, cond)
                err = _parse_cond(lexer, prend, space, cond);
                if(err)return err;
            }
            NEXT

            GET("then")
            GET("(")
            while(1){
                if(GOT(")"))break;
                ARRAY_PUSH_NEW(state_effect_t*, rule->effects, effect)
                err = _parse_effect(lexer, prend, space, effect);
                if(err)return err;
            }
            NEXT
        }
        NEXT
    }
    NEXT
    return 0;
}

int stateset_parse(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space
){
    INIT

    if(GOT("debug_collision")){
        NEXT
        stateset->debug_collision = true;
    }
    if(GOT("collmsgs")){
        NEXT
        GET("(")
        while(!GOT(")")){
            char *msg;
            GET_STR(msg)
            ARRAY_PUSH(char*, stateset->collmsgs, msg)
        }
        NEXT
    }
    while(GOT("on")){
        NEXT
        collmsg_handler_t handler;
        err = _parse_collmsg_handler(lexer, &handler, prend, space);
        if(err)return err;
        ARRAY_PUSH(collmsg_handler_t, stateset->collmsg_handlers, handler)
    }

    err = _stateset_parse(stateset, lexer, prend, space);
    if(err)return err;

    stateset->default_state_name = stateset->states[0]->name;

    return 0;
}

state_t *stateset_get_state(stateset_t *stateset, const char *name){
    for(int i = 0; i < stateset->states_len; i++){
        state_t *state = stateset->states[i];
        if(!strcmp(state->name, name))return state;
    }
    return NULL;
}



/*********
 * STATE *
 *********/


const char state_cond_type_false[] = "false";
const char state_cond_type_key[] = "key";
const char state_cond_type_coll[] = "coll";
const char state_cond_type_chance[] = "chance";
const char state_cond_type_any[] = "any";
const char state_cond_type_all[] = "all";
const char state_cond_type_not[] = "not";
const char state_cond_type_expr[] = "expr";
const char *state_cond_types[] = {
    state_cond_type_false,
    state_cond_type_key,
    state_cond_type_coll,
    state_cond_type_chance,
    state_cond_type_any,
    state_cond_type_all,
    state_cond_type_not,
    state_cond_type_expr,
    NULL
};


const char state_effect_type_print[] = "print";
const char state_effect_type_print_int[] = "print_int";
const char state_effect_type_move[] = "move";
const char state_effect_type_rot[] = "rot";
const char state_effect_type_turn[] = "turn";
const char state_effect_type_goto[] = "goto";
const char state_effect_type_delay[] = "delay";
const char state_effect_type_spawn[] = "spawn";
const char state_effect_type_play[] = "play";
const char state_effect_type_die[] = "die";
const char state_effect_type_zero[] = "zero";
const char state_effect_type_inc[] = "inc";
const char state_effect_type_continue[] = "continue";
const char state_effect_type_confused[] = "confused";
const char state_effect_type_key[] = "key";
const char *state_effect_types[] = {
    state_effect_type_print,
    state_effect_type_print_int,
    state_effect_type_move,
    state_effect_type_rot,
    state_effect_type_turn,
    state_effect_type_goto,
    state_effect_type_delay,
    state_effect_type_spawn,
    state_effect_type_play,
    state_effect_type_die,
    state_effect_type_zero,
    state_effect_type_inc,
    state_effect_type_continue,
    state_effect_type_confused,
    state_effect_type_key,
    NULL
};


void state_cleanup(state_t *state){
    free(state->name);
    if(state->hitbox != NULL){
        hexcollmap_cleanup(state->hitbox);
        free(state->hitbox);
    }
    ARRAY_FREE_PTR(char*, state->collmsgs, (void))
    ARRAY_FREE(collmsg_handler_t, state->collmsg_handlers, collmsg_handler_cleanup)
    ARRAY_FREE_PTR(state_rule_t*, state->rules, state_rule_cleanup)
}

int state_init(state_t *state, stateset_t *stateset, char *name,
    rendergraph_t *rgraph
){
    state->stateset = stateset;
    state->name = name;
    state->rgraph = rgraph;
    state->hitbox = NULL;
    state->safe = true;
    state->flying = false;
    ARRAY_INIT(state->collmsgs)
    ARRAY_INIT(state->collmsg_handlers)
    ARRAY_INIT(state->rules)
    return 0;
}

void state_dump(state_t *state, FILE *file, int depth){
    _print_tabs(file, depth);
    fprintf(file, "%s:\n", state->name);
    if(state->rgraph){
        _print_tabs(file, depth+1);
        fprintf(file, "rgraph: %s\n", state->rgraph->name);
    }
    for(int i = 0; i < state->rules_len; i++){
        state_rule_t *rule = state->rules[i];
        state_rule_dump(rule, file, depth+1);
    }
}


void state_cond_cleanup(state_cond_t *cond){
    if(cond->type == state_cond_type_coll){
        hexcollmap_t *collmap = cond->u.coll.collmap;
        if(collmap != NULL){
            hexcollmap_cleanup(collmap);
            free(collmap);
        }
    }else if(
        cond->type == state_cond_type_any ||
        cond->type == state_cond_type_all ||
        cond->type == state_cond_type_not
    ){
        ARRAY_FREE_PTR(state_cond_t*, cond->u.subconds.conds, state_cond_cleanup)
    }else if(cond->type == state_cond_type_expr){
        free(cond->u.expr.var_name);
    }
}

void state_cond_dump(state_cond_t *cond, FILE *file, int depth){
    _print_tabs(file, depth);
    fprintf(file, "%s\n", cond->type);
    if(
        cond->type == state_cond_type_any ||
        cond->type == state_cond_type_all ||
        cond->type == state_cond_type_not
    ){
        for(int i = 0; i < cond->u.subconds.conds_len; i++){
            state_cond_t *subcond = cond->u.subconds.conds[i];
            state_cond_dump(subcond, file, depth+1);
        }
    }
}

void state_effect_cleanup(state_effect_t *effect){
    if(effect->type == state_effect_type_print){
        free(effect->u.msg);
    }else if(
        effect->type == state_effect_type_print_int ||
        effect->type == state_effect_type_zero ||
        effect->type == state_effect_type_inc
    ){
        free(effect->u.var_name);
    }else if(effect->type == state_effect_type_goto){
        free(effect->u.gotto.name);
    }else if(effect->type == state_effect_type_spawn){
        free(effect->u.spawn.stateset_filename);
        free(effect->u.spawn.state_name);
        free(effect->u.spawn.palmapper_name);
    }else if(effect->type == state_effect_type_play){
        free(effect->u.play_filename);
    }
}

void state_effect_dump(state_effect_t *effect, FILE *file, int depth){
    _print_tabs(file, depth);
    fprintf(file, "%s\n", effect->type);
}

void state_rule_cleanup(state_rule_t *rule){
    ARRAY_FREE_PTR(state_cond_t*, rule->conds, state_cond_cleanup)
    ARRAY_FREE_PTR(state_effect_t*, rule->effects, state_effect_cleanup)
}

int state_rule_init(state_rule_t *rule, state_t *state){
    rule->state = state;
    ARRAY_INIT(rule->conds)
    ARRAY_INIT(rule->effects)
    return 0;
}

void state_rule_dump(state_rule_t *rule, FILE *file, int depth){
    _print_tabs(file, depth);
    fprintf(file, "if:\n");
    for(int i = 0; i < rule->conds_len; i++){
        state_cond_t *cond = rule->conds[i];
        state_cond_dump(cond, file, depth+1);
    }

    _print_tabs(file, depth);
    fprintf(file, "then:\n");
    for(int i = 0; i < rule->effects_len; i++){
        state_effect_t *effect = rule->effects[i];
        state_effect_dump(effect, file, depth+1);
    }
}

