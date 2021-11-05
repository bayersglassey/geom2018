

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "anim.h"
#include "array.h"
#include "hexmap.h"
#include "valexpr.h"
#include "util.h"
#include "prismelrenderer.h"
#include "lexer.h"
#include "lexer_macros.h"
#include "var_utils.h"


static void _print_tabs(FILE *file, int depth){
    for(int i = 0; i < depth; i++)fputs("  ", file);
}


static int _parse_trf(fus_lexer_t *lexer, vecspace_t *space, rot_t *rot_ptr){
    rot_t rot = 0;
    INIT
    while(1){
        if(GOT("^")){
            NEXT
            int addrot;
            GET_INT(addrot)
            rot = rot_contain(space->rot_max, rot + addrot);
        }else break;
    }
    *rot_ptr = rot;
    return 0;
}


static int _parse_collmap(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space,
    hexcollmap_t **own_collmap_ptr, hexcollmap_t **collmap_ptr
){
    int err;

    /* To be returned via own_collmap_ptr, collmap_ptr */
    hexcollmap_t *own_collmap = NULL;
    hexcollmap_t *collmap;

    if(GOT("collmap")){
        NEXT
        OPEN
        const char *name;
        GET_STR_CACHED(name, &prend->name_store)

        hexcollmap_t *found_collmap = stateset_get_collmap(
            stateset, name);
        if(!found_collmap){
            fus_lexer_err_info(lexer);
            fprintf(stderr, "Couldn't find collmap: %s\n", name);
            return 2;
        }
        CLOSE

        rot_t rot;
        err = _parse_trf(lexer, space, &rot);
        if(err)return err;

        if(rot != 0){
            own_collmap = calloc(1, sizeof(*own_collmap));
            if(!own_collmap)return 1;
            hexcollmap_init_clone(own_collmap, found_collmap,
                found_collmap->filename);
            int err = hexcollmap_clone(own_collmap, found_collmap, rot);
            if(err)return err;

            collmap = own_collmap;
        }else{
            collmap = found_collmap;
        }
    }else{
        own_collmap = calloc(1, sizeof(*collmap));
        if(own_collmap == NULL)return 1;
        err = hexcollmap_parse(own_collmap, lexer, space,
            lexer->filename, true,
            &prend->name_store, &prend->filename_store);
        if(err)return err;
        collmap = own_collmap;
    }

    *own_collmap_ptr = own_collmap;
    *collmap_ptr = collmap;
    return 0;
}


static int _parse_effect(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space,
    state_effect_t *effect);
static int _parse_collmsg_handler(stateset_t *stateset, fus_lexer_t *lexer,
    collmsg_handler_t *handler, prismelrenderer_t *prend, vecspace_t *space
){
    INIT

    char *msg;
    GET_STR(msg)

    collmsg_handler_init(handler, msg);

    OPEN
    while(true){
        if(GOT(")"))break;
        ARRAY_PUSH_NEW(state_effect_t*, handler->effects, effect)
        err = _parse_effect(stateset, lexer, prend, space, effect);
        if(err)return err;
    }
    NEXT

    return 0;
}


/******************
* COLLMSG_HANDLER *
******************/

void collmsg_handler_cleanup(collmsg_handler_t *handler){
    free(handler->msg);
    ARRAY_FREE_PTR(state_effect_t*, handler->effects, state_effect_cleanup)
}

void collmsg_handler_init(collmsg_handler_t *handler, char *msg){
    handler->msg = msg;
    ARRAY_INIT(handler->effects)
}


/************
 * STATESET *
 ************/

void stateset_collmap_entry_cleanup(stateset_collmap_entry_t *entry){
    hexcollmap_cleanup(entry->collmap);
    free(entry->collmap);
}

void stateset_cleanup(stateset_t *stateset){
    ARRAY_FREE_PTR(char*, stateset->collmsgs, (void))
    ARRAY_FREE(collmsg_handler_t, stateset->collmsg_handlers,
        collmsg_handler_cleanup)
    ARRAY_FREE_PTR(state_t*, stateset->states, state_cleanup)
    ARRAY_FREE_PTR(stateset_collmap_entry_t*, stateset->collmaps,
        stateset_collmap_entry_cleanup)
}

int stateset_init(stateset_t *stateset, const char *filename){
    stateset->filename = filename;
    ARRAY_INIT(stateset->collmsgs)
    ARRAY_INIT(stateset->collmsg_handlers)
    ARRAY_INIT(stateset->states)
    ARRAY_INIT(stateset->collmaps)
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

hexcollmap_t *stateset_get_collmap(stateset_t *stateset, const char *name){
    for(int i = 0; i < stateset->collmaps_len; i++){
        stateset_collmap_entry_t *entry = stateset->collmaps[i];
        if(!strcmp(entry->name, name))return entry->collmap;
    }
    return NULL;
}

int stateset_load(stateset_t *stateset, const char *filename, vars_t *vars,
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

static int _parse_cond(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space,
    state_cond_t *cond
){
    INIT
    if(GOT("false")){
        NEXT
        cond->type = STATE_COND_TYPE_FALSE;
    }else if(GOT("true")){
        NEXT
        cond->type = STATE_COND_TYPE_TRUE;
    }else if(GOT("key")){
        NEXT
        OPEN

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

        char c;
        GET_CHR(c)
        if(!strchr(ANIM_KEY_CS, c)){
            UNEXPECTED(
                "one of the characters: " ANIM_KEY_CS);
        }

        cond->type = STATE_COND_TYPE_KEY;
        cond->u.key.kstate = kstate;
        cond->u.key.c = c;
        cond->u.key.yes = yes;

        CLOSE
    }else if(GOT("coll")){
        NEXT
        OPEN

        int flags = 0;
        char *collmsg = NULL;

        if(GOT("water")){
            NEXT
            flags ^= ANIM_COND_FLAGS_WATER;
        }else if(GOT("bodies")){
            NEXT
            flags ^= ANIM_COND_FLAGS_BODIES;
            if(GOT_STR){
                GET_STR(collmsg)
            }
        }

        if(GOT("all"))flags ^= ANIM_COND_FLAGS_ALL;
        else if(GOT("any"))/* don't do nuthin */;
        else return UNEXPECTED("all or any");
        NEXT

        if(GOT("yes"))flags ^= ANIM_COND_FLAGS_YES;
        else if(GOT("no"))/* dinnae move a muscle */;
        else return UNEXPECTED("yes or no");
        NEXT

        hexcollmap_t *own_collmap;
        hexcollmap_t *collmap;
        err = _parse_collmap(stateset, lexer, prend, space,
            &own_collmap, &collmap);
        if(err)return err;

        CLOSE

        cond->type = STATE_COND_TYPE_COLL;
        cond->u.coll.own_collmap = own_collmap;
        cond->u.coll.collmap = collmap;
        cond->u.coll.flags = flags;
        cond->u.coll.collmsg = collmsg;
    }else if(GOT("chance")){
        NEXT
        OPEN
        int a;
        int b = 100;
        GET_INT(a)
        if(GOT("%")){
            NEXT
        }else{
            GET("/")
            GET_INT(b)
        }
        CLOSE
        cond->type = STATE_COND_TYPE_CHANCE;
        cond->u.ratio.a = a;
        cond->u.ratio.b = b;
    }else if(GOT("any") || GOT("all") || GOT("not")){
        cond->type =
            GOT("any")? STATE_COND_TYPE_ANY:
            GOT("all")? STATE_COND_TYPE_ALL:
            STATE_COND_TYPE_NOT;
        ARRAY_INIT(cond->u.subconds.conds)

        NEXT
        OPEN
        while(!GOT(")")){
            ARRAY_PUSH_NEW(state_cond_t*, cond->u.subconds.conds, subcond)
            err = _parse_cond(stateset, lexer, prend, space, subcond);
            if(err)return err;
        }
        NEXT
    }else if(GOT("expr")){
        cond->type = STATE_COND_TYPE_EXPR;
        NEXT
        OPEN
        if(GOT("=="))cond->u.expr.op = STATE_COND_EXPR_OP_EQ;
        else if(GOT("!="))cond->u.expr.op = STATE_COND_EXPR_OP_NE;
        else if(GOT( "<"))cond->u.expr.op = STATE_COND_EXPR_OP_LT;
        else if(GOT("<="))cond->u.expr.op = STATE_COND_EXPR_OP_LE;
        else if(GOT( ">"))cond->u.expr.op = STATE_COND_EXPR_OP_GT;
        else if(GOT(">="))cond->u.expr.op = STATE_COND_EXPR_OP_GE;
        else return UNEXPECTED("== or != or < or <= or > or >=");
        NEXT
        err = valexpr_parse(&cond->u.expr.val1_expr, lexer);
        if(err)return err;
        err = valexpr_parse(&cond->u.expr.val2_expr, lexer);
        if(err)return err;
        CLOSE
    }else if(GOT("get_bool")){
        cond->type = STATE_COND_TYPE_GET_BOOL;
        NEXT
        err = valexpr_parse(&cond->u.valexpr, lexer);
        if(err)return err;
    }else if(GOT("exists")){
        cond->type = STATE_COND_TYPE_EXISTS;
        NEXT
        err = valexpr_parse(&cond->u.valexpr, lexer);
        if(err)return err;
    }else{
        return UNEXPECTED(NULL);
    }
    return 0;
}

static int _parse_effect(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space,
    state_effect_t *effect
){
    INIT
    if(GOT("noop")){
        NEXT
        effect->type = STATE_EFFECT_TYPE_NOOP;
    }else if(GOT("print")){
        NEXT
        OPEN
        effect->type = STATE_EFFECT_TYPE_PRINT;
        GET_STR(effect->u.msg)
        CLOSE
    }else if(GOT("print_var")){
        NEXT
        OPEN
        effect->type = STATE_EFFECT_TYPE_PRINT_VAR;
        GET_NAME_CACHED(effect->u.var_name, &prend->name_store)
        CLOSE
    }else if(GOT("print_vars")){
        NEXT
        effect->type = STATE_EFFECT_TYPE_PRINT_VARS;
    }else if(GOT("move")){
        NEXT
        OPEN
        effect->type = STATE_EFFECT_TYPE_MOVE;
        for(int i = 0; i < space->dims; i++){
            GET_INT(effect->u.vec[i]);
        }
        CLOSE
    }else if(GOT("rot")){
        NEXT
        OPEN
        int rot;
        GET_INT(rot)
        effect->type = STATE_EFFECT_TYPE_ROT;
        effect->u.rot = rot_contain(space->rot_max, rot);
        CLOSE
    }else if(GOT("turn")){
        NEXT
        effect->type = STATE_EFFECT_TYPE_TURN;
    }else if(GOT("goto")){
        NEXT

        bool immediate = false;
        if(GOT("immediate")){
            NEXT
            immediate = true;
        }

        bool delay = false;
        if(GOT("delay")){
            NEXT
            delay = true;
        }

        OPEN

        const char *goto_name;
        GET_NAME_CACHED(goto_name, &prend->name_store)

        effect->type = STATE_EFFECT_TYPE_GOTO;
        effect->u.gotto.name = goto_name;
        effect->u.gotto.immediate = immediate;
        effect->u.gotto.delay = delay;

        CLOSE
    }else if(GOT("delay")){
        NEXT
        OPEN

        int delay;
        GET_INT(delay)

        effect->type = STATE_EFFECT_TYPE_DELAY;
        effect->u.delay = delay;

        CLOSE
    }else if(GOT("spawn")){
        NEXT
        OPEN

        state_effect_spawn_t spawn = {0};
        GET_STR_CACHED(spawn.stateset_filename, &prend->filename_store)
        GET_STR_CACHED(spawn.state_name, &prend->name_store)
        OPEN
        GET_INT(spawn.loc.pos[0])
        GET_INT(spawn.loc.pos[1])
        CLOSE
        GET_INT(spawn.loc.rot)
        GET_BOOL(spawn.loc.turn)

        effect->type = STATE_EFFECT_TYPE_SPAWN;
        effect->u.spawn = spawn;

        CLOSE
    }else if(GOT("die")){
        NEXT
        effect->type = STATE_EFFECT_TYPE_DIE;
        if(GOT("mostly")){
            NEXT
            effect->u.dead = BODY_MOSTLY_DEAD;
        }else effect->u.dead = BODY_ALL_DEAD;
    }else if(GOT("inc") || GOT("dec")){
        effect->type = GOT("dec")?
            STATE_EFFECT_TYPE_DEC: STATE_EFFECT_TYPE_INC;
        NEXT

        err = valexpr_parse(&effect->u.set.var_expr, lexer);
        if(err)return err;

        if(GOT("(")){
            NEXT
            err = valexpr_parse(&effect->u.set.val_expr, lexer);
            if(err)return err;
            CLOSE
        }else{
            /* Default: increment by 1 */
            valexpr_set_literal_int(&effect->u.set.val_expr, 1);
        }
    }else if(GOT("continue")){
        effect->type = STATE_EFFECT_TYPE_CONTINUE;
        NEXT
    }else if(GOT("confused")){
        NEXT
        OPEN
        int boolean;
        if(GOT("yes"))boolean = EFFECT_BOOLEAN_TRUE;
        else if(GOT("no"))boolean = EFFECT_BOOLEAN_FALSE;
        else if(GOT("toggle"))boolean = EFFECT_BOOLEAN_TOGGLE;
        else {
            return UNEXPECTED("yes or no or toggle");
        }
        NEXT
        effect->type = STATE_EFFECT_TYPE_CONFUSED;
        effect->u.boolean = boolean;
        CLOSE
    }else if(GOT("key")){
        NEXT
        OPEN

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

        char c;
        GET_CHR(c)
        if(!strchr(ANIM_KEY_CS, c)){
            UNEXPECTED(
                "one of the characters: " ANIM_KEY_CS);
        }

        effect->type = STATE_EFFECT_TYPE_KEY;
        effect->u.key.action = action;
        effect->u.key.c = c;

        CLOSE
    }else if(GOT("set")){
        NEXT

        effect->type = STATE_EFFECT_TYPE_SET;

        err = valexpr_parse(&effect->u.set.var_expr, lexer);
        if(err)return err;

        OPEN
        err = valexpr_parse(&effect->u.set.val_expr, lexer);
        if(err)return err;
        CLOSE
    }else if(GOT("if")){
        NEXT
        effect->type = STATE_EFFECT_TYPE_IF;

        state_effect_ite_t *ite = calloc(1, sizeof(*ite));
        if(!ite)return 1;
        effect->u.ite = ite;

        ARRAY_INIT(ite->conds)
        ARRAY_INIT(ite->then_effects)
        ARRAY_INIT(ite->else_effects)

        OPEN
        while(!GOT(")")){
            ARRAY_PUSH_NEW(state_cond_t*, ite->conds, cond)
            err = _parse_cond(stateset, lexer, prend, space, cond);
            if(err)return err;
        }
        NEXT

        GET("then")
        OPEN
        while(!GOT(")")){
            ARRAY_PUSH_NEW(state_effect_t*, ite->then_effects, effect)
            err = _parse_effect(stateset, lexer, prend, space, effect);
            if(err)return err;
        }
        NEXT

        if(GOT("else")){
            NEXT
            OPEN
            while(!GOT(")")){
                ARRAY_PUSH_NEW(state_effect_t*, ite->else_effects, effect)
                err = _parse_effect(stateset, lexer, prend, space, effect);
                if(err)return err;
            }
            NEXT
        }
    }else if(GOT("as")){
        NEXT
        effect->type = STATE_EFFECT_TYPE_AS;

        if(GOT("you")){
            NEXT
            effect->u.as.type = EFFECT_AS_YOU;
        }else{
            return UNEXPECTED("you");
        }

        ARRAY_INIT(effect->u.as.sub_effects)

        OPEN
        while(!GOT(")")){
            ARRAY_PUSH_NEW(state_effect_t*, effect->u.as.sub_effects,
                sub_effect)
            err = _parse_effect(stateset, lexer, prend, space, sub_effect);
            if(err)return err;
        }
        NEXT
    }else if(GOT("play")){
        NEXT
        OPEN

        const char *play_filename;
        GET_STR_CACHED(play_filename, &prend->filename_store)

        effect->type = STATE_EFFECT_TYPE_PLAY;
        effect->u.play_filename = play_filename;

        CLOSE
    }else{
        return UNEXPECTED(NULL);
    }
    return 0;
}

static int _state_parse_rule(state_t *state, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space
){
    INIT

    stateset_t *stateset = state->stateset;

    ARRAY_PUSH_NEW(state_rule_t*, state->rules, rule)
    err = state_rule_init(rule, state);
    if(err)return err;

    GET("if")
    OPEN
    while(1){
        if(GOT(")"))break;
        ARRAY_PUSH_NEW(state_cond_t*, rule->conds, cond)
        err = _parse_cond(stateset, lexer, prend, space, cond);
        if(err)return err;
    }
    NEXT

    GET("then")
    OPEN
    while(1){
        if(GOT(")"))break;
        ARRAY_PUSH_NEW(state_effect_t*, rule->effects, effect)
        err = _parse_effect(stateset, lexer, prend, space, effect);
        if(err)return err;
    }
    NEXT

    return 0;
}

static int _stateset_parse(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space
){
    /* NOTE: This function may be called recursively! (If an "import"
    token is encountered.) */

    INIT

    while(true){
        if(GOT("debug_collision")){
            NEXT
            stateset->debug_collision = true;
            continue;
        }
        if(GOT("collmsgs")){
            NEXT
            OPEN
            while(!GOT(")")){
                char *msg;
                GET_STR(msg)
                ARRAY_PUSH(char*, stateset->collmsgs, msg)
            }
            NEXT
            continue;
        }
        if(GOT("on")){
            NEXT
            collmsg_handler_t handler;
            err = _parse_collmsg_handler(stateset, lexer, &handler,
                prend, space);
            if(err)return err;
            ARRAY_PUSH(collmsg_handler_t, stateset->collmsg_handlers, handler)
            continue;
        }
        if(GOT("collmap")){
            NEXT
            const char *name;
            GET_STR_CACHED(name, &prend->name_store)
            OPEN

            hexcollmap_t *collmap;
            if(GOT("collmap")){
                NEXT
                OPEN
                const char *name;
                GET_STR_CACHED(name, &prend->name_store)

                hexcollmap_t *found_collmap = stateset_get_collmap(
                    stateset, name);
                if(!found_collmap){
                    fus_lexer_err_info(lexer);
                    fprintf(stderr, "Couldn't find collmap: %s\n", name);
                    return 2;
                }
                CLOSE

                rot_t rot;
                err = _parse_trf(lexer, space, &rot);
                if(err)return err;

                collmap = calloc(1, sizeof(*collmap));
                if(!collmap)return 1;
                hexcollmap_init_clone(collmap, found_collmap,
                    found_collmap->filename);
                int err = hexcollmap_clone(collmap, found_collmap, rot);
                if(err)return err;
            }else{
                collmap = calloc(1, sizeof(*collmap));
                if(!collmap)return 1;
                err = hexcollmap_parse(collmap, lexer, space,
                    lexer->filename, true,
                    &prend->name_store, &prend->filename_store);
                if(err)return err;
            }
            CLOSE

            ARRAY_PUSH_NEW(stateset_collmap_entry_t*, stateset->collmaps,
                entry)
            entry->collmap = collmap;
            entry->name = name;
            continue;
        }
        if(GOT("end_headers")){
            /* This weird thing is just so, theoretically, you could use a
            "reserved keyword" like "collmsgs" or "on" as a state name.
            Although that would be an awful idea. Please don't do that. */
            NEXT
            break;
        }
        break;
    }

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
            free(text);
            continue;
        }

        const char *name;
        GET_NAME_CACHED(name, &prend->name_store)
        OPEN

        ARRAY_PUSH_NEW(state_t*, stateset->states, state)
        err = state_init(state, stateset, name);
        if(err)return err;

        /* Parse state properties */
        while(1){
            if(GOT("rgraph")){
                const char *rgraph_name;
                NEXT
                OPEN
                GET_STR_CACHED(rgraph_name, &prend->name_store)
                CLOSE
                state->rgraph = prismelrenderer_get_rendergraph(
                    prend, rgraph_name);
                if(state->rgraph == NULL){
                    fus_lexer_err_info(lexer);
                    fprintf(stderr, "Couldn't find shape: %s\n", rgraph_name);
                    return 2;}
                continue;
            }
            if(GOT("unsafe")){
                NEXT
                state->safe = false;
                continue;
            }
            if(GOT("flying")){
                NEXT
                state->flying = true;
                continue;
            }
            if(GOT("collmsgs")){
                NEXT
                OPEN
                while(!GOT(")")){
                    char *msg;
                    GET_STR(msg)
                    ARRAY_PUSH(char*, state->collmsgs, msg)
                }
                NEXT
                continue;
            }
            if(GOT("hitbox")){
                NEXT
                OPEN

                hexcollmap_t *own_collmap;
                hexcollmap_t *collmap;
                err = _parse_collmap(stateset, lexer, prend, space,
                    &own_collmap, &collmap);
                if(err)return err;

                state->own_hitbox = own_collmap;
                state->hitbox = collmap;

                CLOSE
                continue;
            }
            if(GOT("on")){
                NEXT
                collmsg_handler_t handler;
                err = _parse_collmsg_handler(stateset, lexer, &handler,
                    prend, space);
                if(err)return err;
                ARRAY_PUSH(collmsg_handler_t, state->collmsg_handlers, handler)
                continue;
            }
            break;
        }

        /* Parse rules */
        while(1){
            if(GOT(")"))break;

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

                while(!fus_lexer_done(&sublexer)){
                    err = _state_parse_rule(state, &sublexer, prend, space);
                    if(err)return err;
                }

                /* We now call fus_lexer_next manually, see call to _fus_lexer_get_str
                above */
                err = fus_lexer_next(lexer);
                if(err)return err;

                fus_lexer_cleanup(&sublexer);
                free(filename);
                free(text);
                continue;
            }

            err = _state_parse_rule(state, lexer, prend, space);
            if(err)return err;
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

    err = _stateset_parse(stateset, lexer, prend, space);
    if(err)return err;

    stateset->default_state_name = stateset->states[0]->name;

    return 0;
}

state_t *stateset_get_state(stateset_t *stateset, const char *name){
    for(int i = 0; i < stateset->states_len; i++){
        state_t *state = stateset->states[i];
        if(state->name == name || !strcmp(state->name, name))return state;
    }
    return NULL;
}



/*********
 * STATE *
 *********/

void state_cleanup(state_t *state){
    if(state->own_hitbox != NULL){
        hexcollmap_cleanup(state->own_hitbox);
        free(state->own_hitbox);
    }
    ARRAY_FREE_PTR(char*, state->collmsgs, (void))
    ARRAY_FREE(collmsg_handler_t, state->collmsg_handlers,
        collmsg_handler_cleanup)
    ARRAY_FREE_PTR(state_rule_t*, state->rules, state_rule_cleanup)
}

int state_init(state_t *state, stateset_t *stateset, const char *name){
    state->stateset = stateset;
    state->name = name;
    state->rgraph = NULL;
    state->own_hitbox = NULL;
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


/*******
* COND *
*******/

void state_cond_cleanup(state_cond_t *cond){
    switch(cond->type){
        case STATE_COND_TYPE_COLL: {
            free(cond->u.coll.collmsg);
            hexcollmap_t *collmap = cond->u.coll.own_collmap;
            if(collmap != NULL){
                hexcollmap_cleanup(collmap);
                free(collmap);
            }
            break;
        }
        case STATE_COND_TYPE_ANY:
        case STATE_COND_TYPE_ALL:
        case STATE_COND_TYPE_NOT:
            ARRAY_FREE_PTR(state_cond_t*, cond->u.subconds.conds,
                state_cond_cleanup)
            break;
        case STATE_COND_TYPE_EXPR:
            valexpr_cleanup(&cond->u.expr.val1_expr);
            valexpr_cleanup(&cond->u.expr.val2_expr);
            break;
        case STATE_COND_TYPE_GET_BOOL:
        case STATE_COND_TYPE_EXISTS:
            valexpr_cleanup(&cond->u.valexpr);
            break;
        default: break;
    }
}

void state_cond_dump(state_cond_t *cond, FILE *file, int depth){
    _print_tabs(file, depth);
    fprintf(file, "%s\n", state_cond_type_name(cond->type));
    if(
        cond->type == STATE_COND_TYPE_ANY ||
        cond->type == STATE_COND_TYPE_ALL ||
        cond->type == STATE_COND_TYPE_NOT
    ){
        for(int i = 0; i < cond->u.subconds.conds_len; i++){
            state_cond_t *subcond = cond->u.subconds.conds[i];
            state_cond_dump(subcond, file, depth+1);
        }
    }
}


/*********
* EFFECT *
*********/

void state_effect_cleanup(state_effect_t *effect){
    switch(effect->type){
        case STATE_EFFECT_TYPE_PRINT:
            free(effect->u.msg);
            break;
        case STATE_EFFECT_TYPE_INC:
        case STATE_EFFECT_TYPE_DEC:
        case STATE_EFFECT_TYPE_SET:
            valexpr_cleanup(&effect->u.set.var_expr);
            valexpr_cleanup(&effect->u.set.val_expr);
            break;
        case STATE_EFFECT_TYPE_IF: {
            state_effect_ite_t *ite = effect->u.ite;
            if(ite){
                ARRAY_FREE_PTR(state_cond_t*, ite->conds,
                    state_cond_cleanup)
                ARRAY_FREE_PTR(state_effect_t*, ite->then_effects,
                    state_effect_cleanup)
                ARRAY_FREE_PTR(state_effect_t*, ite->else_effects,
                    state_effect_cleanup)
                free(ite);
            }
            break;
        }
        case STATE_EFFECT_TYPE_AS:
            ARRAY_FREE_PTR(state_effect_t*, effect->u.as.sub_effects,
                state_effect_cleanup)
            break;
        default: break;
    }
}

void state_effect_dump(state_effect_t *effect, FILE *file, int depth){
    _print_tabs(file, depth);
    fprintf(file, "%s\n", state_effect_type_name(effect->type));
}


/*******
* RULE *
*******/

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

