
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <SDL2/SDL.h>

#include "../test_app.h"


#define SCW 1024
#define SCH 768

/* How many milliseconds we want each frame to last */
#define DELAY_GOAL 30


static test_app_t *get_test_app();


const char* DEFAULT_PREND_FILENAME = "data/test.fus";
const char* DEFAULT_STATESET_FILENAME = "anim/spider.fus";
const char* ENV_DEVEL = "HEXGAME_DEVEL";
const int DEFAULT_PLAYERS = 2;
const int DEFAULT_PLAYERS_PLAYING = 1;


#ifdef __EMSCRIPTEN__
#include "../emscripten.h"

/* Initialize to SDL_GetTicks() before first call to emccdemo_step */
Uint32 tick0;

/* "emccdemo_*" function prototypes */
void emccdemo_initial_syncfs();
void EMSCRIPTEN_KEEPALIVE emccdemo_syncfs();
void EMSCRIPTEN_KEEPALIVE emccdemo_start();
void emccdemo_step(void *arg);

EM_JS(void, emccdemo_initial_syncfs, (), {
    console.log("Starting initial syncfs...");
    FS.syncfs(true, function(err){
        console.log("Initial syncfs finished.");
        if(err){
            console.error("There was an error in FS.syncfs.", err);
            if(!window._syncfs_broke)alert(
                "There was an error in FS.syncfs. " +
                "Saved games may not persist between page refreshes! " +
                "See the console for error details.");
            window._syncfs_broke = true;
        }
        ccall('emccdemo_start', 'v');
    });
})

EM_JS(void, emccdemo_syncfs, (), {
    console.log("Starting syncfs...");
    FS.syncfs(false, function(err){
        console.log("Syncfs finished.");
        if(err){
            console.error("There was an error in FS.syncfs.", err);
            if(!window._syncfs_broke)alert(
                "There was an error in FS.syncfs. " +
                "Saved games may not persist between page refreshes! " +
                "See the console for error details.");
            window._syncfs_broke = true;
        }
    });
})

void emccdemo_start(){
    fprintf(stderr, "Creating test app...\n");

    test_app_t *app = get_test_app();
    if(app == NULL){
        fprintf(stderr, "Couldn't init test app\n");
        return;
    }

    fprintf(stderr, "Starting main loop...\n");

    tick0 = SDL_GetTicks();
    emscripten_set_main_loop_arg(&emccdemo_step, app, 0, true);
    /* NOTE: I believe we never return from the above call. */

    fprintf(stderr, "Cleaning up test app...\n");
    test_app_cleanup(app);
    free(app);
}

void emccdemo_step(void *arg){
    test_app_t *app = arg;

    Uint32 tick1 = SDL_GetTicks();
    Uint32 took = tick1 - tick0;

    /* This function is called by browser's requestAnimationFrame thing,
    so possibly every vsync.
    We only run a step of the game loop if the proper amount of time has
    passed since last step. */
    if(took < app->delay_goal)return;

    app->took = took;
    tick0 = tick1;

    int e = test_app_mainloop_step(app);
    if(e){
        fprintf(stderr, "%s: Exiting with error: %i\n", __func__, e);
        exit(e);
    }
}
#endif


void print_help(FILE *file){
    fprintf(file,
        "Options:\n"
        "   -h   --help           Shows this message\n"
        "   -F                    Fullscreen mode\n"
        "   -FD                   Software simulated fullscreen mode\n"
        "   -f            FILE    Prismelrenderer filename (default: %s)\n"
        "   -a   --anim   FILE    Stateset filename (default: %s)\n"
        "   -m   --map    FILE    Map filename (default: NULL)\n"
        "   -sm  --submap FILE    Submap filename (default: NULL)\n"
        "   -d   --devel          Developer mode\n"
        "                         (You can also use env var %s)\n"
        "   -p   --players   N    Number of players (default: %i)\n"
        "   -l   --load           Load/continue game immediately (skip title screen)\n"
        "        --minimap_alt    Use alternate minimap\n"
        "        --dont_cache_bitmaps\n"
        "                         ...low-level hokey-pokery, should probably get rid of this option\n"
        , DEFAULT_PREND_FILENAME, DEFAULT_STATESET_FILENAME, ENV_DEVEL, DEFAULT_PLAYERS_PLAYING
    );
}

bool get_bool_env(const char *name){
    /* Is indicated environment variable defined and nonempty? */
    const char *value = getenv(name);
    return value && strcmp(value, "");
}


/* Global variables largely just to make them easier to use after
calling emscripten_exit_with_live_runtime */
Uint32 window_flags = SDL_WINDOW_SHOWN;
const char *prend_filename = NULL;
const char *stateset_filename = NULL;
const char *hexmap_filename = NULL;
const char *submap_filename = NULL;
bool minimap_alt = true;
bool cache_bitmaps = true;
bool developer_mode = false;
int n_players = DEFAULT_PLAYERS;
int n_players_playing = DEFAULT_PLAYERS_PLAYING;
bool load_game = false;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

static test_app_t *get_test_app(){
    test_app_t *app = calloc(1, sizeof(*app));
    if(!app){ perror("calloc"); return NULL; }
    int err = test_app_init(app, SCW, SCH, DELAY_GOAL,
        window, renderer, prend_filename, stateset_filename,
        hexmap_filename, submap_filename, developer_mode,
        minimap_alt, cache_bitmaps,
        n_players, n_players_playing, load_game);
    if(err){
        free(app);
        return NULL;
    }
    return app;
}

int main(int n_args, char *args[]){
    int e = 0;

    prend_filename = DEFAULT_PREND_FILENAME;
    stateset_filename = DEFAULT_STATESET_FILENAME;
    developer_mode = get_bool_env(ENV_DEVEL);

#ifdef __EMSCRIPTEN__
    /* Override some defaults if we're running in the browser (and therefore
    aren't getting any commandline arguments) */
    load_game = true;
#endif

    /* The classic */
    srand(time(0));

    for(int arg_i = 1; arg_i < n_args; arg_i++){
        char *arg = args[arg_i];
        if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            print_help(stdout);
            return 0;
        }else if(!strcmp(arg, "-F")){
            window_flags |= SDL_WINDOW_FULLSCREEN;
        }else if(!strcmp(arg, "-FD")){
            window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }else if(!strcmp(arg, "-f")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                goto parse_failure;
            }
            arg = args[arg_i];
            prend_filename = arg;
        }else if(!strcmp(arg, "-a") || !strcmp(arg, "--anim")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                goto parse_failure;
            }
            arg = args[arg_i];
            stateset_filename = arg;
        }else if(!strcmp(arg, "-m") || !strcmp(arg, "--map")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                goto parse_failure;
            }
            arg = args[arg_i];
            hexmap_filename = arg;
        }else if(!strcmp(arg, "-sm") || !strcmp(arg, "--submap")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                goto parse_failure;
            }
            arg = args[arg_i];
            submap_filename = arg;
        }else if(!strcmp(arg, "-d") || !strcmp(arg, "--devel")){
            developer_mode = true;
        }else if(!strcmp(arg, "-p") || !strcmp(arg, "--players")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing int after %s\n", arg);
                goto parse_failure;
            }
            arg = args[arg_i];
            n_players = atoi(arg);
            n_players_playing = n_players;
            if(n_players_playing <= 0){
                fprintf(stderr,
                    "Number of players must be greater than 0. Got: %i\n",
                    n_players_playing);
                goto parse_failure;
            }
            fprintf(stderr, "Number of players set to %i\n", n_players);
        }else if(!strcmp(arg, "-l") || !strcmp(arg, "--load")){
            load_game = true;
        }else if(!strcmp(arg, "--minimap_alt")){
            minimap_alt = !minimap_alt;
        }else if(!strcmp(arg, "--dont_cache_bitmaps")){
            cache_bitmaps = false;
        }else{
            fprintf(stderr, "Unrecognized option: %s\n", arg);
            goto parse_failure;
        }

        continue;
        parse_failure:
        fputc('\n', stderr);
        print_help(stderr);
        return 2;
    }

    if(SDL_Init(SDL_INIT_VIDEO)){
        e = 1;
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
    }else{
        window = SDL_CreateWindow("Spider Game",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            SCW, SCH, window_flags);

        if(!window){
            e = 1;
            fprintf(stderr, "SDL_CreateWindow error: %s\n",
                SDL_GetError());
        }else{
            renderer = SDL_CreateRenderer(window, -1,
                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

            if(!renderer){
                e = 1;
                fprintf(stderr, "SDL_CreateRenderer error: %s\n",
                    SDL_GetError());
            }else{
#ifdef __EMSCRIPTEN__
                /* Load save files from IndexedDB */
                emccdemo_initial_syncfs();

                /* Now kill the current call stack!!!
                Why? Because emccdemo_initial_syncfs has kicked off an
                async JS operation which, when it completes, will set
                current thread's "main loop" to emccdemo_step.
                So the current call to main() never returns.
                Hooray, it's callback hell with C *and* Javascript! */
                emscripten_exit_with_live_runtime();
#endif
                test_app_t *app = get_test_app();
                if(app == NULL){
                    e = 1;
                    fprintf(stderr, "Couldn't init test app\n");
                }else{
                    e = test_app_mainloop(app);
                    fprintf(stderr, "Cleaning up...\n");
                    test_app_cleanup(app);
                    free(app);
                }
                SDL_DestroyRenderer(renderer);
            }
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }
    fprintf(stderr, "Exiting with code: %i\n", e);
    return e;
}
