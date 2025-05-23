
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "../anim.h"
#include "../vec4.h"
#include "../prismelrenderer.h"
#include "../file_utils.h"
#include "../lexer.h"
#include "../lexer_macros.h"
#include "../hexgame.h"
#include "../hexgame_state.h"
#include "../defaults.h"


typedef struct test_case {
    stateset_t stateset;
    hexcollmap_t *collmap;
    int steps;
    ARRAY_DECL(state_effect_t*, before_effects)
    ARRAY_DECL(state_effect_t*, after_effects)
    /* Weakrefs: */
    const char *name;
    const char *state_name;
    state_context_t *context;
} test_case_t;
typedef struct test_suite {
    const char *filename;
    ARRAY_DECL(test_case_t*, test_cases)
} test_suite_t;


static void test_case_cleanup(test_case_t *test_case){
    stateset_cleanup(&test_case->stateset);
    if(test_case->collmap){
        hexcollmap_cleanup(test_case->collmap);
        free(test_case->collmap);
    }
    ARRAY_FREE_PTR(state_effect_t*, test_case->before_effects,
        state_effect_cleanup)
    ARRAY_FREE_PTR(state_effect_t*, test_case->after_effects,
        state_effect_cleanup)
}
static void test_case_init(test_case_t *test_case, const char *name){
    test_case->name = name;
    test_case->state_name = NULL;
    ARRAY_INIT(test_case->before_effects)
    ARRAY_INIT(test_case->after_effects)
}
static int _run_effects(state_effect_t *effects[], int effects_len,
    const char *effect_kind, hexgame_state_context_t *context
){
    int err;
    if(effects_len <= 0)return 0;
    fprintf(stderr, "Running %i \"%s\" effects...\n", effects_len, effect_kind);
    state_effect_goto_t *gotto = NULL;
    for(int i = 0; i < effects_len; i++){
        state_effect_t *effect = effects[i];
        err = state_effect_apply(effect, context, &gotto, NULL);
        if(!err && gotto){
            fprintf(stderr, "Can't use \"goto\"");
            err = 2;
        }
        if(err){
            if(err == 2){
                fprintf(stderr, "...in \"%s\" effect %i/%i\n",
                    effect_kind, i + 1, effects_len);
                if(context->body){
                    fprintf(stderr, "...with body:\n");
                    body_dump(context->body, 1);
                }
            }
            return err;
        }
    }
    return 0;
}
static int test_case_run(test_case_t *test_case, hexgame_t *game){
    int err;

    /* Use the test case's stateset
    NOTE: we need to remove it at the end of the test! */
    err = hexgame_swap_stateset(game, &test_case->stateset, NULL);
    if(err)return err;

    /* Add a map for this test case */
    ARRAY_PUSH_NEW(hexmap_t*, game->maps, map)
    err = hexmap_init(map, game, "<test map>");
    if(err)return err;

    /* Add a submap for this test case */
    hexmap_submap_parser_context_t submap_context;
    err = hexmap_submap_parser_context_init_root(&submap_context, map);
    if(err)return err;
    ARRAY_PUSH_NEW(hexmap_submap_t*, map->submaps, submap)
    err = hexmap_submap_init(map, submap, "<test submap>", &submap_context);
    if(err)return err;
    hexcollmap_init_clone(&submap->collmap, test_case->collmap, submap->filename);
    err = hexcollmap_clone(&submap->collmap, test_case->collmap, submap_context.rot);
    if(err)return err;

    /* Add a body using the test case's stateset */
    ARRAY_PUSH_NEW(body_t*, map->bodies, body)
    err = body_init(body, game, map,
        test_case->stateset.filename,
        test_case->state_name,
        NULL);
    if(err)return err;

    hexgame_state_context_t state_context = {
        .game = game,
        .body = body,
    };

    /* Run before_effects */
    err = _run_effects(
        test_case->before_effects,
        test_case->before_effects_len,
        "before", &state_context);
    if(err)return err;

    /* Run steps */
    if(test_case->steps){
        fprintf(stderr, "Running %i game steps...\n", test_case->steps);
        for(int i = 0; i < test_case->steps; i++){
            err = hexgame_step(game);
            if(err)return err;
        }
    }

    /* Run after_effects */
    err = _run_effects(
        test_case->after_effects,
        test_case->after_effects_len,
        "after", &state_context);
    if(err)return err;

    /* HACK: remove our stateset from hexgame, since it's owned by test_case,
    and we don't want hexgame_cleanup to touch it */
    ARRAY_REMOVE(game->statesets, &test_case->stateset, (void))

    /* HACK: remove our test map from hexgame (which also removes the submap,
    bodies, etc */
    ARRAY_REMOVE_PTR(game->maps, map, hexmap_cleanup)

    return 0;
}


static void test_suite_cleanup(test_suite_t *suite){
    ARRAY_FREE_PTR(test_case_t*, suite->test_cases, test_case_cleanup)
}
static void test_suite_init(test_suite_t *suite, const char *filename){
    suite->filename = filename;
    ARRAY_INIT(suite->test_cases)
}
static int test_suite_load(test_suite_t *suite, const char *filename,
    prismelrenderer_t *prend
){
    int err;
    vecspace_t *space = &hexspace;

    char *text = load_file(filename);
    if(text == NULL)return 1;
    fus_lexer_t _lexer, *lexer=&_lexer;
    err = fus_lexer_init(lexer, text, filename);
    if(err)return err;

    test_suite_init(suite, filename);
    GET("cases")
    OPEN
    while(!GOT(")")){
        const char *name = NULL;
        if(GOT_STR){
            GET_STR_CACHED(name, &prend->name_store)
        }
        OPEN
        ARRAY_PUSH_NEW(test_case_t*, suite->test_cases, test_case)
        test_case_init(test_case, name);
        {
            GET("stateset")
            OPEN
            const char *stateset_filename;
            GET_STR_CACHED(stateset_filename, &prend->filename_store)
            stateset_t *stateset = &test_case->stateset;
            err = stateset_load(stateset, stateset_filename,
                prend, space);
            if(err)return err;
            ARRAY_PUSH_NEW(state_context_t*, stateset->contexts, context)
            state_context_init(context, stateset, stateset->root_context);
            test_case->context = context;
            CLOSE
        }
        if(GOT("state")){
            NEXT
            OPEN
            GET_STR_CACHED(test_case->state_name, &prend->name_store)
            CLOSE
        }
        if(GOT("collmap")){
            NEXT
            OPEN
            hexcollmap_t *collmap = calloc(1, sizeof(*collmap));
            if(collmap == NULL)return 1;
            err = hexcollmap_parse(collmap, lexer, space,
                lexer->filename, true,
                &prend->name_store, &prend->filename_store);
            if(err)return err;
            test_case->collmap = collmap;
            CLOSE
        }
        if(GOT("before")){
            NEXT
            OPEN
            while(!GOT(")")){
                ARRAY_PUSH_NEW(state_effect_t*, test_case->before_effects, effect)
                err = state_effect_parse(effect, test_case->context, lexer, prend, space);
                if(err)return err;
            }
            NEXT
        }
        if(GOT("steps")){
            NEXT
            OPEN
            GET_INT(test_case->steps)
            CLOSE
        }
        if(GOT("after")){
            NEXT
            OPEN
            while(!GOT(")")){
                ARRAY_PUSH_NEW(state_effect_t*, test_case->after_effects, effect)
                err = state_effect_parse(effect, test_case->context, lexer, prend, space);
                if(err)return err;
            }
            CLOSE
        }
        CLOSE
    }
    NEXT

    fus_lexer_cleanup(lexer);
    free(text);
    return 0;
}


static hexgame_save_callback_t _save_callback;
static int _save_callback(hexgame_t *game){
    /* Should presumably never happen when running tests! */
    fprintf(stderr, "WARNING: tried to save!..");
    return 0;
}


static void print_usage(FILE *file){
    fprintf(stderr,
        "Usage: animtest [OPTION ...] [--] FILENAME"
        "\n"
        "Reads and runs test cases from FILENAME\n"
        "\n"
        "Options:\n"
        "   -h --help            Print this message and exit\n"
        "      --prend FILENAME  Prismelrenderer (default: %s)\n"
    , DEFAULT_PREND_FILENAME);
}


int main(int n_args, char **args){
    const char *prend_filename = DEFAULT_PREND_FILENAME;

    int arg_i = 1;
    for(; arg_i < n_args; arg_i++){
        char *arg = args[arg_i];
        if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            print_usage(stdout);
            return 0;
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

    const char *suite_filename = args[arg_i];
    {
        int err;

        fprintf(stderr, "Loading prismelrenderer from file: %s\n",
            prend_filename);
        prismelrenderer_t _prend, *prend=&_prend;
        err = prismelrenderer_init(prend, &vec4);
        if(err)return err;
        err = prismelrenderer_load(prend, prend_filename, NULL, NULL);
        if(err)return err;

        prismelrenderer_t minimap_prend;
        err = prismelrenderer_init(&minimap_prend, &vec4);
        if(err)return err;
        err = prismelrenderer_load(&minimap_prend,
            DEFAULT_MINIMAP_PREND_FILENAME,
            NULL, NULL);
        if(err)return err;

        hexgame_t _game, *game=&_game;
        err = hexgame_init(game, prend,
            &minimap_prend,
            HEXGAME_DEFAULT_MINIMAP_TILESET,
            &_save_callback, NULL, /* no callback data */
            false /* no audio */);
        if(err)return err;

        fprintf(stderr, "Loading test suite from file: %s\n",
            suite_filename);
        test_suite_t _suite, *suite=&_suite;
        err = test_suite_load(suite, suite_filename, prend);
        if(err)return err;

        fprintf(stderr, "Running %i tests...\n", suite->test_cases_len);
        for(int i = 0; i < suite->test_cases_len; i++){
            test_case_t *test_case = suite->test_cases[i];
            fprintf(stderr, "Running test %i/%i: %s\n",
                i + 1, suite->test_cases_len,
                test_case->name? test_case->name: "<no name>");
            err = test_case_run(test_case, game);
            if(err)return err;
            fprintf(stderr, "Test %i/%i OK!\n",
                i + 1, suite->test_cases_len);
        }
        fprintf(stderr, "%i tests OK!\n", suite->test_cases_len);

        test_suite_cleanup(suite);
        hexgame_cleanup(game);
        prismelrenderer_cleanup(&minimap_prend);
        prismelrenderer_cleanup(prend);
    }
    return 0;
}
