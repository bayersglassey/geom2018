
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "../anim.h"
#include "../vec4.h"
#include "../prismelrenderer.h"
#include "../defaults.h"


typedef struct opts{
    bool output_handlers;
    bool output_procs;
    FILE *file;
} opts_t;
static void opts_init(opts_t *opts){
    opts->output_handlers = true;
    opts->output_procs = true;
    opts->file = stdout;
}


static void print_usage(FILE *file){
    fprintf(stderr,
        "Usage: animtool [OPTION ...] [--] FILENAME"
        "\n"
        "Reads FILENAME, parsing it as a stateset, and outputs it as a graph\n"
        "(expressed in the Graphviz \"dot\" language).\n"
        "\n"
        "Options:\n"
        "   -h --help            Print this message and exit\n"
        "   -H --no-handlers     Don't output collmsg handlers\n"
        "   -P --no-procs        Don't output procs\n"
        "      --prend FILENAME  Prismelrenderer (default: %s)\n"
    , DEFAULT_PREND_FILENAME);
}


static int dump_effect(stateset_t *stateset, state_effect_t *effect,
    state_context_t *context, void *parent, opts_t *opts
) {
    int err;
    switch(effect->type){
        case STATE_EFFECT_TYPE_CALL: if(opts->output_procs){
            state_effect_call_t *call = &effect->u.call;
            stateset_proc_t *proc = state_context_get_proc(context, call->name);
            if(!proc){
                fprintf(stderr, "Couldn't find proc \"%s\"\n", call->name);
                return 2;
            }
            fprintf(opts->file, "    \"%p\" -> \"%p\";\n", parent, proc);
            break;
        }
        case STATE_EFFECT_TYPE_SPAWN: {
            state_effect_spawn_t *spawn = &effect->u.spawn;
            for(int i = 0; i < spawn->effects_len; i++){
                state_effect_t *effect = spawn->effects[i];
                err = dump_effect(stateset, effect, context, parent, opts);
                if(err)return err;
            }
            break;
        }
        case STATE_EFFECT_TYPE_GOTO: {
            state_effect_goto_t *gotto = &effect->u.gotto;
            state_t *state = stateset_get_state(stateset, gotto->name);
            if(!state){
                fprintf(stderr, "Couldn't find state \"%s\"\n", gotto->name);
                return 2;
            }
            fprintf(opts->file, "    \"%p\" -> \"%p\";\n", parent, state);
            break;
        }
        case STATE_EFFECT_TYPE_IF: {
            state_effect_ite_t *ite = effect->u.ite;
            for(int i = 0; i < ite->then_effects_len; i++){
                state_effect_t *sub_effect = ite->then_effects[i];
                err = dump_effect(stateset, sub_effect, context, parent,
                    opts);
            }
            for(int i = 0; i < ite->else_effects_len; i++){
                state_effect_t *sub_effect = ite->else_effects[i];
                err = dump_effect(stateset, sub_effect, context, parent,
                    opts);
            }
            break;
        }
        default: break;
    }
    return 0;
}

static int dump_handler(stateset_t *stateset, collmsg_handler_t *handler,
    state_context_t *context, opts_t *opts
){
    int err;

    fprintf(opts->file, "    \"%p\" [label=\"Handler: %s\"];\n", handler, handler->msg);

    for(int i = 0; i < handler->effects_len; i++){
        state_effect_t *effect = handler->effects[i];
        err = dump_effect(stateset, effect, context, handler, opts);
        if(err)return err;
    }
    return 0;
}

static int dump_proc(stateset_t *stateset, stateset_proc_t *proc,
    state_context_t *context, opts_t *opts
){
    int err;

    fprintf(opts->file, "    \"%p\" [label=\"Proc: %s\"];\n", proc, proc->name);

    for(int i = 0; i < proc->effects_len; i++){
        state_effect_t *effect = proc->effects[i];
        err = dump_effect(stateset, effect, context, proc, opts);
        if(err)return err;
    }
    return 0;
}

static int dump_state(stateset_t *stateset, state_t *state, opts_t *opts){
    int err;

    state_context_t *context = state->context;

    fprintf(opts->file, "    \"%p\" [label=\"State: %s\"];\n", state, state->name);

    if(opts->output_handlers){
        for(
            state_context_t *context = state->context;
            context; context = context->parent
        ){
            for(int i = 0; i < context->collmsg_handlers_len; i++){
                collmsg_handler_t *handler = &context->collmsg_handlers[i];
                fprintf(opts->file, "    \"%p\" -> \"%p\";\n", state, handler);
            }
        }
    }

    for(int i = 0; i < state->rules_len; i++){
        state_rule_t *rule = state->rules[i];
        for(int j = 0; j < rule->effects_len; j++){
            state_effect_t *effect = rule->effects[j];
            err = dump_effect(stateset, effect, context, state, opts);
            if(err)return err;
        }
    }

    return 0;
}

static int dump_context(stateset_t *stateset, state_context_t *context,
    opts_t *opts
){
    int err;

    if(opts->output_handlers){
        for(int i = 0; i < context->collmsg_handlers_len; i++){
            collmsg_handler_t *handler = &context->collmsg_handlers[i];
            err = dump_handler(stateset, handler, context, opts);
            if(err)return err;
        }
    }

    if(opts->output_procs){
        for(int i = 0; i < context->procs_len; i++){
            stateset_proc_t *proc = &context->procs[i];
            err = dump_proc(stateset, proc, context, opts);
            if(err)return err;
        }
    }

    return 0;
}

static int dump_stateset(stateset_t *stateset, opts_t *opts){
    int err;

    fprintf(opts->file, "digraph {\n");
    fprintf(opts->file, "    label = \"Stateset: %s\";\n", stateset->filename);
    fprintf(opts->file, "    fontsize = 24;\n");

    for(int i = 0; i < stateset->contexts_len; i++){
        state_context_t *context = stateset->contexts[i];
        dump_context(stateset, context, opts);
    }

    for(int i = 0; i < stateset->states_len; i++){
        state_t *state = stateset->states[i];
        err = dump_state(stateset, state, opts);
        if(err)return err;
    }

    fprintf(opts->file, "}\n");

    return 0;
}


int main(int n_args, char **args){
    const char *prend_filename = DEFAULT_PREND_FILENAME;

    opts_t opts;
    opts_init(&opts);

    int arg_i = 1;
    for(; arg_i < n_args; arg_i++){
        char *arg = args[arg_i];
        if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            print_usage(stdout);
            return 0;
        }else if(!strcmp(arg, "-H") || !strcmp(arg, "--no-handlers")){
            opts.output_handlers = false;
        }else if(!strcmp(arg, "-P") || !strcmp(arg, "--no-procs")){
            opts.output_procs = false;
        }else if(!strcmp(arg, "--prend")){
            if(n_args - arg_i < 2){
                fprintf(stderr, "Missing value for option: %s\n", arg);
                return 2;
            }
            prend_filename = args[arg_i+1];
            arg_i++;
        }else if(!strcmp(arg, "--")){
            arg_i++;
            break;
        }else{
            break;
        }
    }

    int remaining_args = n_args - arg_i;
    if(remaining_args != 1){
        print_usage(stderr);
        return 2;
    }

    const char *stateset_filename = args[arg_i];

    /* Load the stateset */
    {
        int err;

        fprintf(stderr, "Loading prismelrenderer from file: %s\n",
            prend_filename);
        prismelrenderer_t prend;
        err = prismelrenderer_init(&prend, &vec4);
        if(err)return err;
        err = prismelrenderer_load(&prend, prend_filename, NULL, NULL);
        if(err)return err;

        fprintf(stderr, "Loading stateset from file: %s\n",
            stateset_filename);
        stateset_t stateset;
        stateset_init(&stateset, stateset_filename);
        err = stateset_load(&stateset, stateset_filename,
            &prend, &hexspace);
        if(err)return err;

        /* What we came here for... */
        err = dump_stateset(&stateset, &opts);
        if(err)return err;

        stateset_cleanup(&stateset);
        prismelrenderer_cleanup(&prend);
    }
    return 0;
}
