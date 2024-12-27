
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <SDL2/SDL.h>

#include "../test_app.h"
#include "../save_slots.h"


#define SCW 1024
#define SCH 768

/* How many milliseconds we want each frame to last */
#define DEFAULT_DELAY_GOAL 30


static test_app_t *get_test_app();


const char* DEFAULT_PREND_FILENAME = "data/test.fus";
const char* DEFAULT_STATESET_FILENAME = "anim/spider.fus";
const char* ENV_DEVEL = "HEXGAME_DEVEL";
const int DEFAULT_PLAYERS = 1;


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

    if(app->state == TEST_APP_STATE_QUIT){
        fprintf(stderr, "%s: Quitting...\n", __func__);
        emscripten_force_exit(0);
    }

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
        emscripten_force_exit(e);
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
        "   -D   --devel          Developer mode\n"
        "                         (You can also use env var %s)\n"
        "   -d   --delay     N    Delay goal (in milliseconds) (default: %i)\n"
        "   -p   --players   N    Number of players (default: %i)\n"
        "   -s   --save-slot SLOT Use indicated save slot\n"
        "   -l   --load-game SLOT Load saved game immediately on startup\n"
        "   -n   --new-game  SLOT Start a new game (delete save slot and start)\n"
        "        --minimap-alt    Use alternate minimap\n"
        "        --dont-cache-bitmaps\n"
        "                         ...low-level hokey-pokery, should probably get rid of this option\n"
        "        --dont-animate-palettes\n"
        "                         Don't animate the colour palettes; improves the quality of\n"
        "                         gameplay recordings saved as GIFs\n"
        "   -lrf --load-recording-filename FILE\n"
        "                         Set \"load recording filename\"\n"
        "                         (where to load recording from, used by F10)\n"
        "   -srf --save-recording-filename FILE\n"
        "                         Set \"save recording filename\"\n"
        "                         (where to save recording to, used by F9)\n"
        "        --no-audio       Disable audio\n"
        , DEFAULT_PREND_FILENAME, DEFAULT_STATESET_FILENAME, ENV_DEVEL,
        DEFAULT_DELAY_GOAL, DEFAULT_PLAYERS
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
bool animate_palettes = true;
bool developer_mode = false;
int n_players = DEFAULT_PLAYERS;
int delay_goal = DEFAULT_DELAY_GOAL;
int save_slot = 0;
const char *load_recording_filename = NULL;
const char *save_recording_filename = NULL;
bool have_audio = true;
bool load_save_slot = false;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

static test_app_t *get_test_app(){
    test_app_t *app = calloc(1, sizeof(*app));
    if(!app){ perror("calloc"); return NULL; }
    int err = test_app_init(app, SCW, SCH, delay_goal,
        window, renderer, prend_filename, stateset_filename,
        hexmap_filename, submap_filename, developer_mode,
        minimap_alt, cache_bitmaps, animate_palettes,
        n_players, save_slot,
        load_recording_filename, save_recording_filename,
        have_audio, load_save_slot);
    if(err){
        free(app);
        return NULL;
    }
    return app;
}

int main(int n_args, char *args[]){
    int err = 0;

    prend_filename = DEFAULT_PREND_FILENAME;
    stateset_filename = DEFAULT_STATESET_FILENAME;
    developer_mode = get_bool_env(ENV_DEVEL);

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
        }else if(!strcmp(arg, "-D") || !strcmp(arg, "--devel")){
            developer_mode = true;
        }else if(!strcmp(arg, "-d") || !strcmp(arg, "--delay")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing int after %s\n", arg);
                goto parse_failure;
            }
            arg = args[arg_i];
            delay_goal = atoi(arg);
        }else if(!strcmp(arg, "-p") || !strcmp(arg, "--players")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing int after %s\n", arg);
                goto parse_failure;
            }
            arg = args[arg_i];
            n_players = atoi(arg);
            if(n_players <= 0){
                fprintf(stderr,
                    "Number of players must be greater than 0. Got: %i\n",
                    n_players);
                goto parse_failure;
            }
        }else if(
            !strcmp(arg, "-s") || !strcmp(arg, "--save-slot") ||
            !strcmp(arg, "-l") || !strcmp(arg, "--load-game") ||
            !strcmp(arg, "-n") || !strcmp(arg, "--new-game")
        ){
            char action = arg[1] == '-'? arg[2]: arg[1]; /* 's' or 'l' or 'n' */
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing int after %s\n", arg);
                goto parse_failure;
            }
            arg = args[arg_i];
            save_slot = atoi(arg);
            if(save_slot < 0 || save_slot >= SAVE_SLOTS){
                fprintf(stderr,
                    "Save slot must be in [0..%i). Got: %i\n",
                    SAVE_SLOTS, save_slot);
                goto parse_failure;
            }
            if(action == 'n'){
                int err2 = delete_save_slot(save_slot);
                if(err2){
                    /* Presumably there was just nothing saved in that slot,
                    so we don't exit with an error, we just print a
                    warning. */
                    fprintf(stderr, "WARNING: couldn't delete save slot %i\n",
                        save_slot);
                }
            }else if(action == 'l'){
                load_save_slot = true;
            }
        }else if(!strcmp(arg, "--minimap-alt")){
            minimap_alt = !minimap_alt;
        }else if(!strcmp(arg, "--dont-cache-bitmaps")){
            cache_bitmaps = false;
        }else if(!strcmp(arg, "--dont-animate-palettes")){
            animate_palettes = false;
        }else if(!strcmp(arg, "-lrf") || !strcmp(arg, "--load-recording-filename")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                goto parse_failure;
            }
            load_recording_filename = args[arg_i];
        }else if(!strcmp(arg, "-srf") || !strcmp(arg, "--save-recording-filename")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                goto parse_failure;
            }
            save_recording_filename = args[arg_i];
        }else if(!strcmp(arg, "--no-audio")){
            have_audio = false;
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
        err = 1;
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
    }else{
        if(have_audio && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0){
            fprintf(stderr, "SDL_InitSubSystem error: %s\n", SDL_GetError());
            fprintf(stderr, "...disabling audio!\n");
            have_audio = false;
        }

        window = SDL_CreateWindow("Spider Game",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            SCW, SCH, window_flags);

        if(!window){
            err = 1;
            fprintf(stderr, "SDL_CreateWindow error: %s\n",
                SDL_GetError());
        }else{
            renderer = SDL_CreateRenderer(window, -1,
                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

            if(!renderer){
                err = 1;
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
                    err = 1;
                    fprintf(stderr, "Couldn't init test app\n");
                }else{
                    if(have_audio){
                        /* Open an audio device */
                        SDL_AudioSpec want_spec, spec;
                        err = hexgame_audio_sdl_open_device(
                            &app->hexgame.audio_data,
                            &want_spec, &spec, &app->audio_id
                        );
                        if(!err){
                            /* Start playing sound */
                            SDL_PauseAudioDevice(app->audio_id, 0);
                        }
                    }
                    if(!err){
                        err = test_app_mainloop(app);
                    }
                    fprintf(stderr, "Cleaning up...\n");
                    SDL_CloseAudioDevice(app->audio_id);
                    test_app_cleanup(app);
                    free(app);
                }
                SDL_DestroyRenderer(renderer);
            }
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }

    fprintf(stderr, "Exiting with code: %i\n", err);
    return err;
}
