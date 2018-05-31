
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "prismelrenderer.h"
#include "vec4.h"
#include "font.h"
#include "console.h"


#define SCW 640
#define SCH 400


#define ERR_INFO() fprintf(stderr, "%s: %i: ", __func__, __LINE__)
#define RET_IF_SDL_ERR(x) {int e=(x); \
    if(e < 0){ERR_INFO(); \
    fprintf(stderr, "SDL error: %s\n", SDL_GetError()); \
    return 2;}}



int mainloop(SDL_Renderer *renderer, int n_args, char *args[]){
    int err;

    SDL_Color pal[] = {
        {.r=255, .g= 60, .b= 60},
        {.r= 60, .g=255, .b= 60},
        {.r= 60, .g= 60, .b=255},
        {.r=255, .g=255, .b=255},
    };

    prismelrenderer_t prend;
    font_t font;
    console_t console;

    char *filename = "data/test.fus";
    if(n_args >= 2)filename = args[1];

    err = prismelrenderer_load(&prend, filename, &vec4);
    if(err)return err;

    err = font_load(&font, "data/font.fus", renderer);
    if(err)return err;

    err = console_init(&console, 40, 20, 200);
    if(err)return err;

    int n_rgraphs;
    rendergraph_t **rgraphs;
    err = prismelrenderer_get_rendergraphs(&prend,
        &n_rgraphs, &rgraphs);
    if(err)return err;
    if(n_rgraphs < 1){
        fprintf(stderr, "No rendergraphs in %s\n", filename);
        return 2;}

    int cur_rgraph_i = 0;

    SDL_Event event;

    int rot = 0;
    int zoom = 4;
    bool loop = true;
    bool refresh = true;
    bool keydown_shift = false;
    int keydown_l = 0;
    int keydown_r = 0;

    SDL_StartTextInput();
    while(loop){
        if(refresh){
            refresh = false;

            rendergraph_bitmap_t *bitmap;
            err = rendergraph_get_or_render_bitmap(
                rgraphs[cur_rgraph_i], &bitmap,
                rot, false, pal, renderer);
            if(err)return err;

            SDL_Rect dst_rect = {
                SCW/2 - bitmap->pbox.x*zoom,
                SCH/2 - bitmap->pbox.y*zoom,
                bitmap->pbox.w*zoom,
                bitmap->pbox.h*zoom
            };

            RET_IF_SDL_ERR(SDL_SetRenderDrawColor(renderer,
                0, 0, 0, 255));
            RET_IF_SDL_ERR(SDL_RenderClear(renderer));

            /*
            RET_IF_SDL_ERR(SDL_RenderCopy(renderer, font.texture,
                NULL, NULL));
            */

            RET_IF_SDL_ERR(SDL_RenderCopy(renderer, bitmap->texture,
                NULL, &dst_rect));
            font_blitmsg(&font, renderer, 0, 0,
                "Controls:\n"
                "  up/down - zoom\n"
                "  left/right - rotate (hold shift for tap mode)\n"
                "  page up/down - cycle through available rendergraphs\n"
                "  0 - reset rotation\n"
                "Currently displaying rendergraphs from file: %s\n"
                "Currently displaying rendergraph %i: %s",
                filename, cur_rgraph_i, rgraphs[cur_rgraph_i]->name);
            console_blit(&console, &font, renderer, 0, 10 * font.char_h);
            SDL_RenderPresent(renderer);
        }
        while(SDL_PollEvent(&event)){
            switch(event.type){
                case SDL_KEYDOWN: {
                    if(event.key.keysym.sym == SDLK_ESCAPE){
                        loop = false;}

                    if(event.key.keysym.sym == SDLK_RETURN){
                        printf("GOT CONSOLE INPUT: %s\n", console.input);

                        int action = -1;
                        if(!strcmp(console.input, "exit"))action = 0;
                        else if(!strcmp(console.input, "cls"))action = 1;

                        console_input_accept(&console); refresh = true;

                        if(action == 0)loop = false;
                        else if(action == 1)console_clear(&console);
                    }
                    if(event.key.keysym.sym == SDLK_BACKSPACE){
                        console_input_backspace(&console); refresh = true;}
                    if(event.key.keysym.sym == SDLK_DELETE){
                        console_input_delete(&console); refresh = true;}

                    if(event.key.keysym.sym == SDLK_0){
                        rot = 0; refresh = true;}
                    if(event.key.keysym.sym == SDLK_LEFT && keydown_l == 0){
                        keydown_l = 2;}
                    if(event.key.keysym.sym == SDLK_RIGHT && keydown_r == 0){
                        keydown_r = 2;}
                    if(event.key.keysym.sym == SDLK_LSHIFT
                        || event.key.keysym.sym == SDLK_RSHIFT){
                            keydown_shift = true;}
                    if(event.key.keysym.sym == SDLK_PAGEUP){
                        cur_rgraph_i++;
                        if(cur_rgraph_i >= n_rgraphs)cur_rgraph_i = 0;
                        refresh = true;}
                    if(event.key.keysym.sym == SDLK_PAGEDOWN){
                        cur_rgraph_i--;
                        if(cur_rgraph_i < 0)cur_rgraph_i = n_rgraphs - 1;
                        refresh = true;}
                    if(event.key.keysym.sym == SDLK_UP){
                        zoom += 1; if(zoom > 10)zoom=10; refresh = true;}
                    if(event.key.keysym.sym == SDLK_DOWN){
                        zoom -= 1; if(zoom <= 0)zoom=1; refresh = true;}
                } break;
                case SDL_KEYUP: {
                    if(event.key.keysym.sym == SDLK_LEFT){
                        keydown_l = 0;}
                    if(event.key.keysym.sym == SDLK_RIGHT){
                        keydown_r = 0;}
                    if(event.key.keysym.sym == SDLK_LSHIFT
                        || event.key.keysym.sym == SDLK_RSHIFT){
                            keydown_shift = false;}
                } break;
                case SDL_TEXTINPUT: {
                    for(char *c = event.text.text; *c != '\0'; c++){
                        console_input_char(&console, *c);
                    }
                    refresh = true;
                } break;
                case SDL_QUIT: loop = false; break;
                default: break;
            }
        }
        if(keydown_l >= (keydown_shift? 2: 1)){
            rot += 1; refresh = true; keydown_l = 1;}
        if(keydown_r >= (keydown_shift? 2: 1)){
            rot -= 1; refresh = true; keydown_r = 1;}
    }

    free(rgraphs);
    return 0;
}

int main(int n_args, char *args[]){
    int e = 0;
    if(SDL_Init(SDL_INIT_VIDEO)){
        e = 1;
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
    }else{
        SDL_Window *window = SDL_CreateWindow("Hello World!",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            SCW, SCH, SDL_WINDOW_SHOWN);

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
                e = mainloop(renderer, n_args, args);
                SDL_DestroyRenderer(renderer);
            }
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }
    fprintf(stderr, "Exiting with code: %i\n", e);
    return e;
}
