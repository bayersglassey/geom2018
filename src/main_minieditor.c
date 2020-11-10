
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <SDL2/SDL.h>

#include "minieditor.h"


#define SCW 1024
#define SCH 768

/* How many milliseconds we want each frame to last */
#define DELAY_GOAL 30


int minieditor_mainloop(minieditor_t *editor){
    int err;

    SDL_StartTextInput();

    bool loop = true;
    while(loop){
        SDL_Event _event, *event = &_event;
        while(SDL_PollEvent(event)){
            if(event->type == SDL_QUIT){
                loop = false;
                break;
            }

            if(event->type == SDL_KEYDOWN){
                if(event->key.keysym.sym == SDLK_ESCAPE){
                    loop = false;
                    break;
                }
            }

            err = minieditor_process_event(editor, event);
            if(err)return err;
        }
    }

    SDL_StopTextInput();
    return 0;
}



int main(int n_args, char *args[]){
    int e = 0;
    Uint32 window_flags = SDL_WINDOW_SHOWN;
    const char *prend_filename = "data/test.fus";
    bool cache_bitmaps = true;

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
        }else if(!strcmp(arg, "--dont_cache_bitmaps")){
            cache_bitmaps = false;
        }else{
            fprintf(stderr, "Unrecognized option: %s\n", arg);
            return 2;
        }
    }

    const char *window_title = "Geom Editor";

    if(SDL_Init(SDL_INIT_VIDEO)){
        e = 1;
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
    }else{
        SDL_Window *window = SDL_CreateWindow(window_title,
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
                minieditor_t editor;
                minieditor_init(&editor,
                    NULL, NULL, prend_filename,
                    NULL, NULL, NULL,
                    SCW, SCH);

                e = minieditor_mainloop(&editor);

                fprintf(stderr, "Cleaning up...\n");
                minieditor_cleanup(&editor);

                SDL_DestroyRenderer(renderer);
            }
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }
    fprintf(stderr, "Exiting with code: %i\n", e);
    return e;
}
