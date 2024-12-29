

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
#include "hexgame_vars_props.h"


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


static int _parse_collmap(state_context_t *context, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space,
    hexcollmap_t **own_collmap_ptr, hexcollmap_t **collmap_ptr
){
    int err;

    /* To be returned via own_collmap_ptr, collmap_ptr */
    hexcollmap_t *own_collmap = NULL;
    hexcollmap_t *collmap;

    stateset_t *stateset = context->stateset;

    if(GOT("collmap")){
        NEXT
        OPEN
        const char *name;
        GET_STR_CACHED(name, &prend->name_store)

        hexcollmap_t *found_collmap = state_context_get_collmap(
            context, name);
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


static int _parse_effect(state_context_t *context, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space,
    state_effect_t *effect);
static int _parse_collmsg_handler(state_context_t *context, fus_lexer_t *lexer,
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
        err = _parse_effect(context, lexer, prend, space, effect);
        if(err)return err;
    }
    NEXT

    return 0;
}
static int _parse_stateset_proc(state_context_t *context, fus_lexer_t *lexer,
    stateset_proc_t *proc, prismelrenderer_t *prend, vecspace_t *space
){
    INIT

    stateset_t *stateset = context->stateset;

    int type = STATESET_PROC_TYPE_NORMAL;
    bool toplevel = false;
    while(true){
        if(GOT("toplevel")){
            NEXT
            toplevel = true;
            continue;
        }
        if(GOT("onstatesetchange")){
            NEXT
            type = STATESET_PROC_TYPE_ONSTATESETCHANGE;
            continue;
        }
        if(GOT("onmapchange")){
            NEXT
            type = STATESET_PROC_TYPE_ONMAPCHANGE;
            continue;
        }
        break;
    }

    if(toplevel){
        /* Just puts this proc in the root context for this stateset, which
        is particularly useful for bodies making calls to each others procs */
        context = stateset->root_context;
    }

    const char *name;
    GET_STR_CACHED(name, &prend->name_store)

    stateset_proc_t *found_proc = state_context_get_proc(context, name);
    if(found_proc){
        fprintf(stderr, "Can't redefine proc \"%s\" in stateset \"%s\"\n",
            name, stateset->filename);
        return 2;
    }

    stateset_proc_init(proc, stateset, type, name);

    OPEN
    while(true){
        if(GOT(")"))break;
        ARRAY_PUSH_NEW(state_effect_t*, proc->effects, effect)
        err = _parse_effect(context, lexer, prend, space, effect);
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


/****************
* STATESET_PROC *
*****************/

void stateset_proc_cleanup(stateset_proc_t *proc){
    ARRAY_FREE_PTR(state_effect_t*, proc->effects, state_effect_cleanup)
}

void stateset_proc_init(stateset_proc_t *proc, stateset_t *stateset,
    int type, const char *name
){
    proc->stateset = stateset;
    proc->type = type;
    proc->name = name;
    ARRAY_INIT(proc->effects)
}


/****************
* STATE_CONTEXT *
****************/

void state_context_collmap_entry_cleanup(state_context_collmap_entry_t *entry){
    hexcollmap_cleanup(entry->collmap);
    free(entry->collmap);
}

void state_context_cleanup(state_context_t *context){
    ARRAY_FREE_PTR(char*, context->collmsgs, (void))
    ARRAY_FREE(collmsg_handler_t, context->collmsg_handlers,
        collmsg_handler_cleanup)
    ARRAY_FREE(stateset_proc_t, context->procs,
        stateset_proc_cleanup)
    ARRAY_FREE_PTR(state_context_collmap_entry_t*, context->collmaps,
        state_context_collmap_entry_cleanup)
}

void state_context_init(state_context_t *context, stateset_t *stateset,
    state_context_t *parent
){
    ARRAY_INIT(context->collmsgs)
    ARRAY_INIT(context->collmsg_handlers)
    ARRAY_INIT(context->procs)
    ARRAY_INIT(context->collmaps)
    context->stateset = stateset;
    context->parent = parent;
}

hexcollmap_t *state_context_get_collmap(state_context_t *context,
    const char *name
){
    while(context){
        for(int i = 0; i < context->collmaps_len; i++){
            state_context_collmap_entry_t *entry = context->collmaps[i];
            if(!strcmp(entry->name, name))return entry->collmap;
        }
        context = context->parent;
    }
    return NULL;
}

stateset_proc_t *state_context_get_proc(state_context_t *context,
    const char *name
){
    while(context){
        for(int i = 0; i < context->procs_len; i++){
            stateset_proc_t *proc = &context->procs[i];
            if(!strcmp(proc->name, name))return proc;
        }
        context = context->parent;
    }
    return NULL;
}


/************
 * STATESET *
 ************/

void stateset_cleanup(stateset_t *stateset){
    ARRAY_FREE_PTR(state_t*, stateset->states, state_cleanup)
    ARRAY_FREE_PTR(state_context_t*, stateset->contexts,
        state_context_cleanup)
    vars_cleanup(&stateset->vars);
}

int stateset_init(stateset_t *stateset, const char *filename){
    stateset->filename = filename;
    ARRAY_INIT(stateset->states)
    ARRAY_INIT(stateset->contexts)
    vars_init_with_props(&stateset->vars, hexgame_vars_prop_names);
    stateset->debug_collision = false;

    /* Create root context */
    ARRAY_PUSH_NEW(state_context_t*, stateset->contexts, context)
    state_context_init(context, stateset, NULL);
    stateset->root_context = context;

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

int stateset_load(stateset_t *stateset, const char *filename,
    prismelrenderer_t *prend, vecspace_t *space
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init_with_vars(&lexer, text, filename, NULL);
    if(err)return err;

    err = stateset_init(stateset, filename);
    if(err)return err;

    err = stateset_parse(stateset, &lexer, prend, space);
    if(err)return err;

    fus_lexer_cleanup(&lexer);

    free(text);
    return 0;
}

static int _parse_cond(state_context_t *context, fus_lexer_t *lexer,
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
        valexpr_t collmsg_expr;
        valexpr_set_literal_null(&collmsg_expr);

        if(GOT("water")){
            NEXT
            flags ^= ANIM_COND_FLAGS_WATER;
        }else if(GOT("bodies")){
            NEXT
            flags ^= ANIM_COND_FLAGS_BODIES;
            if(GOT("(")){
                NEXT
                err = valexpr_parse(&collmsg_expr, lexer);
                if(err)return err;
                CLOSE
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
        err = _parse_collmap(context, lexer, prend, space,
            &own_collmap, &collmap);
        if(err)return err;

        CLOSE

        cond->type = STATE_COND_TYPE_COLL;
        cond->u.coll.own_collmap = own_collmap;
        cond->u.coll.collmap = collmap;
        cond->u.coll.flags = flags;
        cond->u.coll.collmsg_expr = collmsg_expr;
    }else if(GOT("dead")){
        NEXT
        OPEN
        int dead = -1; /* See: body_is_done_for */
        if(GOT("mostly")){
            NEXT
            dead = BODY_MOSTLY_DEAD;
        }else if(GOT("all")){
            NEXT
            dead = BODY_ALL_DEAD;
        }
        CLOSE
        cond->type = STATE_COND_TYPE_DEAD;
        cond->u.dead = dead;
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
            err = _parse_cond(context, lexer, prend, space, subcond);
            if(err)return err;
        }
        NEXT
    }else if(GOT("expr")){
        cond->type = STATE_COND_TYPE_EXPR;
        NEXT
        OPEN
        err = valexpr_parse(&cond->u.valexpr, lexer);
        if(err)return err;
        CLOSE
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

static int _parse_effect(state_context_t *context, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space,
    state_effect_t *effect
){
    INIT
    if(GOT("noop")){
        NEXT
        effect->type = STATE_EFFECT_TYPE_NOOP;
    }else if(GOT("no_key_reset")){
        NEXT
        effect->type = STATE_EFFECT_TYPE_NO_KEY_RESET;
    }else if(GOT("print")){
        NEXT
        OPEN
        effect->type = STATE_EFFECT_TYPE_PRINT;
        err = valexpr_parse(&effect->u.expr, lexer);
        if(err)return err;
        CLOSE
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
    }else if(GOT("relocate")){
        NEXT
        effect->type = STATE_EFFECT_TYPE_RELOCATE;
        OPEN
        if(!GOT(")")){
            err = valexpr_parse(&effect->u.relocate.loc_expr, lexer);
            if(err)return err;
        }
        if(!GOT(")")){
            err = valexpr_parse(&effect->u.relocate.map_filename_expr, lexer);
            if(err)return err;
        }
        if(!GOT(")")){
            err = valexpr_parse(&effect->u.relocate.stateset_filename_expr, lexer);
            if(err)return err;
        }
        if(!GOT(")")){
            err = valexpr_parse(&effect->u.relocate.state_name_expr, lexer);
            if(err)return err;
        }
        CLOSE
    }else if(GOT("goto")){
        NEXT

        bool immediate = false;
        if(GOT("immediate")){
            NEXT
            immediate = true;
        }

        bool delay = false;
        int add_delay = 0;
        if(GOT("delay")){
            NEXT
            delay = true;
            if(GOT_INT){
                GET_INT(add_delay)
            }
        }

        OPEN

        const char *goto_name;
        GET_STR_CACHED(goto_name, &prend->name_store)

        effect->type = STATE_EFFECT_TYPE_GOTO;
        effect->u.gotto.name = goto_name;
        effect->u.gotto.immediate = immediate;
        effect->u.gotto.delay = delay;
        effect->u.gotto.add_delay = add_delay;

        CLOSE
    }else if(GOT("call")){
        NEXT

        OPEN

        const char *name;
        GET_STR_CACHED(name, &prend->name_store)

        effect->type = STATE_EFFECT_TYPE_CALL;
        effect->u.call.state_context = context;
        effect->u.call.name = name;

        CLOSE
    }else if(GOT("delay")){
        NEXT
        OPEN
        effect->type = STATE_EFFECT_TYPE_DELAY;
        err = valexpr_parse(&effect->u.expr, lexer);
        if(err)return err;
        CLOSE
    }else if(GOT("add_delay")){
        NEXT
        OPEN
        effect->type = STATE_EFFECT_TYPE_ADD_DELAY;
        err = valexpr_parse(&effect->u.expr, lexer);
        if(err)return err;
        CLOSE
    }else if(GOT("spawn")){
        NEXT
        OPEN

        state_effect_spawn_t spawn = {0};
        err = valexpr_parse(&spawn.stateset_filename_expr, lexer);
        if(err)return err;
        err = valexpr_parse(&spawn.state_name_expr, lexer);
        if(err)return err;
        OPEN
        GET_INT(spawn.loc.pos[0])
        GET_INT(spawn.loc.pos[1])
        CLOSE
        GET_INT(spawn.loc.rot)
        GET_BOOL(spawn.loc.turn)

        ARRAY_INIT(spawn.effects)
        if(GOT("do")){
            NEXT
            OPEN
            while(!GOT(")")){
                ARRAY_PUSH_NEW(state_effect_t*, spawn.effects, effect)
                err = _parse_effect(context, lexer, prend, space, effect);
                if(err)return err;
            }
            NEXT
        }

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
    }else if(GOT("remove")){
        NEXT
        effect->type = STATE_EFFECT_TYPE_REMOVE;
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
        effect->type = STATE_EFFECT_TYPE_SET;
        NEXT

        err = valexpr_parse(&effect->u.set.var_expr, lexer);
        if(err)return err;

        OPEN
        err = valexpr_parse(&effect->u.set.val_expr, lexer);
        if(err)return err;
        CLOSE
    }else if(GOT("unset")){
        effect->type = STATE_EFFECT_TYPE_UNSET;
        NEXT

        err = valexpr_parse(&effect->u.set.var_expr, lexer);
        if(err)return err;

        valexpr_set_literal_null(&effect->u.set.val_expr);
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
            err = _parse_cond(context, lexer, prend, space, cond);
            if(err)return err;
        }
        NEXT

        GET("then")
        OPEN
        while(!GOT(")")){
            ARRAY_PUSH_NEW(state_effect_t*, ite->then_effects, effect)
            err = _parse_effect(context, lexer, prend, space, effect);
            if(err)return err;
        }
        NEXT

        if(GOT("else")){
            NEXT
            OPEN
            while(!GOT(")")){
                ARRAY_PUSH_NEW(state_effect_t*, ite->else_effects, effect)
                err = _parse_effect(context, lexer, prend, space, effect);
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
            err = _parse_effect(context, lexer, prend, space, sub_effect);
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
    state_context_t *context = state->context;

    const char *name = NULL;
    if(GOT_STR){
        GET_STR_CACHED(name, &prend->name_store)
        for(int i = 0; i < state->rules_len; i++){
            state_rule_t *rule = state->rules[i];
            if(rule->name != NULL && !strcmp(rule->name, name)){
                fprintf(stderr, "State \"%s\": attempted to define duplicate rule: \"%s\"\n",
                    state->name, name);
                return 2;}
        }
    }

    ARRAY_PUSH_NEW(state_rule_t*, state->rules, rule)
    state_rule_init(rule, state, name);

    GET("if")
    OPEN
    while(1){
        if(GOT(")"))break;
        ARRAY_PUSH_NEW(state_cond_t*, rule->conds, cond)
        err = _parse_cond(context, lexer, prend, space, cond);
        if(err)return err;
    }
    NEXT

    GET("then")
    OPEN
    while(1){
        if(GOT(")"))break;
        ARRAY_PUSH_NEW(state_effect_t*, rule->effects, effect)
        err = _parse_effect(context, lexer, prend, space, effect);
        if(err)return err;
    }
    NEXT

    return 0;
}

static int parse_rgraph_reference(fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space, rendergraph_t **rgraph_ptr
){
    int err;

    rendergraph_t *rgraph;
    if(GOT("collmap")){
        NEXT
        hexcollmap_t _collmap, *collmap=&_collmap;

        /* Get tileset */
        const char *tileset_name;
        GET_STR_CACHED(tileset_name, &prend->name_store)
        tileset_t *tileset;
        err = prismelrenderer_get_or_create_tileset(prend, tileset_name,
            &tileset);
        if(err)return err;

        /* Parse collmap */
        OPEN
        err = hexcollmap_parse(collmap, lexer, space,
            "<inline>" /* filename */,
            true /* just_coll */,
            &prend->name_store, &prend->filename_store);
        if(err)return err;
        CLOSE

        /* TODO: generate a random name or something */
        const char *rgraph_name = "...collmap...";

        /* Create rgraph */
        ARRAY_PUSH_NEW(rendergraph_t*, prend->rendergraphs, _rgraph)
        rgraph = _rgraph;
        err = rendergraph_init(rgraph, rgraph_name, prend, NULL,
            rendergraph_animation_type_cycle,
            HEXMAP_SUBMAP_RGRAPH_N_FRAMES /* HACK */);
        if(err)return err;

        /* Populate rgraph's children */
        err = rendergraph_add_rgraphs_from_collmap(
            rgraph, collmap, tileset,
            false /* add_collmap_rendergraphs */);
        if(err)return err;

        hexcollmap_cleanup(collmap);
    }else{
        const char *rgraph_name;
        GET_STR_CACHED(rgraph_name, &prend->name_store)
        rgraph = prismelrenderer_get_rendergraph(prend, rgraph_name);
        if(rgraph == NULL){
            fus_lexer_err_info(lexer);
            fprintf(stderr, "Couldn't find shape: %s\n", rgraph_name);
            return 2;}
    }

    *rgraph_ptr = rgraph;
    return 0;
}

static int _state_parse(state_t *state, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space
){
    int err;

    stateset_t *stateset = state->stateset;
    state_context_t *context = state->context;

    /* Parse state properties */
    while(1){
        if(GOT("rgraph")){
            NEXT
            if(state->rgraph){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Can't redefine rgraph.\n");
                return 2;
            }
            OPEN
            err = parse_rgraph_reference(lexer, prend, space, &state->rgraph);
            if(err)return err;
            CLOSE
            continue;
        }
        if(GOT("extra_rgraph")){
            NEXT
            OPEN
            rendergraph_t *rgraph;
            err = parse_rgraph_reference(lexer, prend, space, &rgraph);
            if(err)return err;
            CLOSE
            ARRAY_PUSH(rendergraph_t*, state->extra_rgraphs, rgraph)
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
                ARRAY_PUSH(char*, state->context->collmsgs, msg)
            }
            NEXT
            continue;
        }
        if(GOT("hitbox")){
            NEXT
            OPEN

            hexcollmap_t *own_collmap;
            hexcollmap_t *collmap;
            err = _parse_collmap(context, lexer, prend, space,
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
            err = _parse_collmsg_handler(context, lexer, &handler,
                prend, space);
            if(err)return err;
            ARRAY_PUSH(collmsg_handler_t, state->context->collmsg_handlers, handler)
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
                if(err){
                    fus_lexer_err_info(lexer);
                    fprintf(stderr, "...while importing here.\n");
                    return err;
                }
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

    return 0;
}

static int _stateset_parse(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space, state_context_t *context
){
    /* NOTE: This function may be called recursively! (If an "import"
    token is encountered.) */

    INIT

    while(!DONE){
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
                ARRAY_PUSH(char*, context->collmsgs, msg)
            }
            NEXT
            continue;
        }
        if(GOT("on")){
            NEXT
            collmsg_handler_t handler;
            err = _parse_collmsg_handler(context, lexer, &handler,
                prend, space);
            if(err)return err;
            ARRAY_PUSH(collmsg_handler_t, context->collmsg_handlers, handler)
            continue;
        }
        if(GOT("proc")){
            NEXT
            stateset_proc_t proc;
            err = _parse_stateset_proc(context, lexer, &proc,
                prend, space);
            if(err)return err;
            ARRAY_PUSH(stateset_proc_t, context->procs, proc)
            continue;
        }
        if(GOT("vars")){
            NEXT
            OPEN
            err = vars_parse(&stateset->vars, lexer);
            if(err)return err;
            CLOSE
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

                hexcollmap_t *found_collmap = state_context_get_collmap(
                    context, name);
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

            ARRAY_PUSH_NEW(state_context_collmap_entry_t*, context->collmaps,
                entry)
            entry->collmap = collmap;
            entry->name = name;
            continue;
        }
        if(GOT("import")){
            NEXT

            bool scoped = false;
            if(GOT("scoped")){
                NEXT
                scoped = true;
            }

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

            state_context_t *import_context = context;
            if(scoped){
                ARRAY_PUSH_NEW(state_context_t*, stateset->contexts, sub_context)
                state_context_init(sub_context, stateset, context);
                import_context = sub_context;
            }

            err = _stateset_parse(stateset, &sublexer, prend, space,
                import_context);
            if(err){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "...while importing here.\n");
                return err;
            }

            if(!fus_lexer_done(&sublexer)){
                err = fus_lexer_unexpected(&sublexer, "end of file");
                fus_lexer_err_info(lexer);
                fprintf(stderr, "...while importing here.\n");
                return err;
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
        if(GOT("state")){
            NEXT
            const char *name;
            GET_STR_CACHED(name, &prend->name_store)
            OPEN

            ARRAY_PUSH_NEW(state_context_t*, stateset->contexts, sub_context)
            state_context_init(sub_context, stateset, context);

            ARRAY_PUSH_NEW(state_t*, stateset->states, state)
            state_init(state, stateset, name, sub_context);

            err = _state_parse(state, lexer, prend, space);
            if(err)return err;
            continue;
        }
        return UNEXPECTED(NULL);
    }
    return 0;
}

int stateset_parse(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space
){
    INIT

    err = _stateset_parse(stateset, lexer, prend, space,
        stateset->root_context);
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
    ARRAY_FREE(rendergraph_t*, state->extra_rgraphs, (void))
    ARRAY_FREE_PTR(state_rule_t*, state->rules, state_rule_cleanup)
}

void state_init(state_t *state, stateset_t *stateset, const char *name,
    state_context_t *context
){
    state->stateset = stateset;
    state->name = name;
    state->rgraph = NULL;
    ARRAY_INIT(state->extra_rgraphs)
    state->own_hitbox = NULL;
    state->hitbox = NULL;
    state->safe = true;
    state->flying = false;
    ARRAY_INIT(state->rules)
    state->context = context;
}

void state_dump(state_t *state, FILE *file, int depth){
    _print_tabs(file, depth);
    fprintf(file, "%s:\n", state->name);
    if(state->rgraph){
        _print_tabs(file, depth+1);
        fprintf(file, "rgraph: %s\n", state->rgraph->name);
    }
    for(int i = 0; i < state->extra_rgraphs_len; i++){
        rendergraph_t *rgraph = state->extra_rgraphs[i];
        _print_tabs(file, depth+1);
        fprintf(file, "extra_rgraph: %s\n", rgraph->name);
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
            valexpr_cleanup(&cond->u.coll.collmsg_expr);
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
        case STATE_EFFECT_TYPE_DELAY:
        case STATE_EFFECT_TYPE_ADD_DELAY:
            valexpr_cleanup(&effect->u.expr);
            break;
        case STATE_EFFECT_TYPE_RELOCATE:
            valexpr_cleanup(&effect->u.relocate.loc_expr);
            valexpr_cleanup(&effect->u.relocate.map_filename_expr);
            valexpr_cleanup(&effect->u.relocate.stateset_filename_expr);
            valexpr_cleanup(&effect->u.relocate.state_name_expr);
            break;
        case STATE_EFFECT_TYPE_INC:
        case STATE_EFFECT_TYPE_DEC:
        case STATE_EFFECT_TYPE_SET:
        case STATE_EFFECT_TYPE_UNSET:
            valexpr_cleanup(&effect->u.set.var_expr);
            valexpr_cleanup(&effect->u.set.val_expr);
            break;
        case STATE_EFFECT_TYPE_SPAWN:
            valexpr_cleanup(&effect->u.spawn.stateset_filename_expr);
            valexpr_cleanup(&effect->u.spawn.state_name_expr);
            ARRAY_FREE_PTR(state_effect_t*, effect->u.spawn.effects,
                state_effect_cleanup)
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

void state_rule_init(state_rule_t *rule, state_t *state, const char *name){
    rule->state = state;
    ARRAY_INIT(rule->conds)
    ARRAY_INIT(rule->effects)
    rule->name = name;
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

