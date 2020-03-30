

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



/************
 * STATESET *
 ************/

void stateset_cleanup(stateset_t *stateset){
    free(stateset->filename);
    ARRAY_FREE_PTR(state_t*, stateset->states, state_cleanup)
}

int stateset_init(stateset_t *stateset, char *filename){
    stateset->filename = filename;
    ARRAY_INIT(stateset->states)
    stateset->is_projectile = false;
    stateset->is_collectible = false;
    stateset->collided_state_name = NULL;
    return 0;
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
            flags ^= 4;
        }else if(GOT("bodies")){
            NEXT
            flags ^= 8;
        }

        if(GOT("all"))flags ^= 1;
        else if(GOT("any"))/* don't do nuthin */;
        else return UNEXPECTED("all or any");
        NEXT

        if(GOT("yes"))flags ^= 2;
        else if(GOT("no"))/* dinnae move a muscle */;
        else return UNEXPECTED("yes or no");
        NEXT

        hexcollmap_t *collmap = calloc(1, sizeof(*collmap));
        if(collmap == NULL)return 1;
        err = hexcollmap_init(collmap, space,
            strdup(lexer->filename));
        if(err)return err;
        err = hexcollmap_parse(collmap, lexer, true);
        if(err)return err;

        GET(")")

        cond->type = state_cond_type_coll;
        cond->u.coll.collmap = collmap;
        cond->u.coll.flags = flags;
    }else if(GOT("chance")){
        NEXT
        GET("(")
        int percent = 0;
        GET_INT(percent)
        GET("%")
        GET(")")
        cond->type = state_cond_type_chance;
        cond->u.percent = percent;
    }else if(GOT("any") || GOT("all")){
        cond->type = GOT("any")? state_cond_type_any: state_cond_type_all;
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
        char *msg;
        GET_STR(msg)
        effect->type = state_effect_type_print;
        effect->u.msg = msg;
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
        GET_INT(spawn.pos[0])
        GET_INT(spawn.pos[1])
        GET(")")
        GET_INT(spawn.rot)
        GET_BOOL(spawn.turn)

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
        if(GOT("crushes")){
            NEXT
            state->crushes = true;
        }
        if(GOT("hitbox")){
            NEXT
            GET("(")

            hexcollmap_t *collmap = calloc(1, sizeof(*collmap));
            if(collmap == NULL)return 1;
            err = hexcollmap_init(collmap, space,
                strdup(lexer->filename));
            if(err)return err;
            err = hexcollmap_parse(collmap, lexer, true);
            if(err)return err;
            state->hitbox = collmap;

            GET(")")
        }
        if(GOT("on_collided")){
            NEXT
            GET("(")
            char *collided_state_name;
            GET_NAME(collided_state_name)
            state->collided_state_name = collided_state_name;
            GET(")")
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

    if(GOT("projectile")){
        NEXT
        stateset->is_projectile = true;
    }

    if(GOT("collectible")){
        NEXT
        stateset->is_collectible = true;
    }

    if(GOT("on_collided")){
        NEXT
        GET("(")
        char *collided_state_name;
        GET_NAME(collided_state_name)
        stateset->collided_state_name = collided_state_name;
        GET(")")
    }

    return _stateset_parse(stateset, lexer, prend, space);
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
const char *state_cond_types[] = {
    state_cond_type_false,
    state_cond_type_key,
    state_cond_type_coll,
    state_cond_type_chance,
    state_cond_type_any,
    state_cond_type_all,
    NULL
};


const char state_effect_type_print[] = "print";
const char state_effect_type_move[] = "move";
const char state_effect_type_rot[] = "rot";
const char state_effect_type_turn[] = "turn";
const char state_effect_type_goto[] = "goto";
const char state_effect_type_delay[] = "delay";
const char state_effect_type_spawn[] = "spawn";
const char state_effect_type_play[] = "play";
const char state_effect_type_die[] = "die";
const char *state_effect_types[] = {
    state_effect_type_print,
    state_effect_type_move,
    state_effect_type_rot,
    state_effect_type_turn,
    state_effect_type_goto,
    state_effect_type_delay,
    state_effect_type_spawn,
    state_effect_type_play,
    state_effect_type_die,
    NULL
};


void state_cleanup(state_t *state){
    free(state->name);
    if(state->hitbox != NULL){
        hexcollmap_cleanup(state->hitbox);
        free(state->hitbox);
    }
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
    state->crushes = false;
    state->collided_state_name = NULL;
    ARRAY_INIT(state->rules)
    return 0;
}


static void state_cond_cleanup(state_cond_t *cond){
    if(cond->type == state_cond_type_coll){
        hexcollmap_t *collmap = cond->u.coll.collmap;
        if(collmap != NULL){
            hexcollmap_cleanup(collmap);
            free(collmap);
        }
    }else if(
        cond->type == state_cond_type_any ||
        cond->type == state_cond_type_all
    ){
        ARRAY_FREE_PTR(state_cond_t*, cond->u.subconds.conds, state_cond_cleanup)
    }
}

static void state_effect_cleanup(state_effect_t *effect){
    if(effect->type == state_effect_type_print){
        free(effect->u.msg);
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

