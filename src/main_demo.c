
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <SDL2/SDL.h>

#include "test_app.h"


#define SCW 1024
#define SCH 768

/* How many milliseconds we want each frame to last */
#define DELAY_GOAL 30


#ifdef __EMSCRIPTEN__
#include "emscripten.h"
static void test_app_mainloop_emcc(void *arg){
    test_app_t *app = arg;
    int e = test_app_mainloop_step(app);
    if(e){
        fprintf(stderr, "%s: Exiting with error: %i\n", __func__, e);
        exit(e);
    }
}
#endif


int main(int n_args, char *args[]){
    int e = 0;
    Uint32 window_flags = SDL_WINDOW_SHOWN;
    const char *prend_filename = "data/test.fus";
    const char *stateset_filename = "anim/player.fus";
    const char *hexmap_filename = "data/maps/title/worldmap.fus";
    const char *submap_filename = NULL;
    bool minimap_alt = true;
    bool cache_bitmaps = true;
    int n_players = 2;
    int n_players_playing = 1;

    /* The classic */
    srand(time(0));

    for(int arg_i = 1; arg_i < n_args; arg_i++){
        char *arg = args[arg_i];
        if(!strcmp(arg, "-F")){
            window_flags |= SDL_WINDOW_FULLSCREEN;
        }else if(!strcmp(arg, "-FD")){
            window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }else if(!strcmp(arg, "-f")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                return 2;}
            arg = args[arg_i];
            prend_filename = arg;
        }else if(!strcmp(arg, "--anim")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                return 2;}
            arg = args[arg_i];
            stateset_filename = arg;
        }else if(!strcmp(arg, "--map")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                return 2;}
            arg = args[arg_i];
            hexmap_filename = arg;
        }else if(!strcmp(arg, "--submap")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                return 2;}
            arg = args[arg_i];
            submap_filename = arg;
        }else if(!strcmp(arg, "--minimap_alt")){
            minimap_alt = !minimap_alt;
        }else if(!strcmp(arg, "--dont_cache_bitmaps")){
            cache_bitmaps = false;
        }else if(!strcmp(arg, "--players")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing int after %s\n", arg);
                return 2;}
            arg = args[arg_i];
            n_players = atoi(arg);
            n_players_playing = n_players;
            fprintf(stderr, "Number of players set to %i\n", n_players);
        }else{
            fprintf(stderr, "Unrecognized option: %s\n", arg);
            return 2;
        }
    }

    if(SDL_Init(SDL_INIT_VIDEO)){
        e = 1;
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
    }else{
        SDL_Window *window = SDL_CreateWindow("Spider Game",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            SCW, SCH, window_flags);

        if(!window){
            e = 1;
            fprintf(stderr, "SDL_CreateWindow error: %s\n",
                SDL_GetError());
        }else{
            SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

            if(!renderer){
                e = 1;
                fprintf(stderr, "SDL_CreateRenderer error: %s\n",
                    SDL_GetError());
            }else{
                test_app_t app;
                if(test_app_init(&app, SCW, SCH, DELAY_GOAL,
                    window, renderer, prend_filename, stateset_filename,
                    hexmap_filename, submap_filename, minimap_alt,
                    cache_bitmaps, n_players, n_players_playing)
                ){
                    e = 1;
                    fprintf(stderr, "Couldn't init test app\n");
                }else{
#ifdef __EMSCRIPTEN__
                    SDL_StartTextInput();
                    emscripten_set_main_loop_arg(&test_app_mainloop_emcc,
                        &app, 0, true);
#else
                    e = test_app_mainloop(&app);
#endif
                    fprintf(stderr, "Cleaning up...\n");
                    test_app_cleanup(&app);
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
