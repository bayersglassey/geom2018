
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "test_app.h"
#include "console.h"



int test_app_process_event_console(test_app_t *app, SDL_Event *event){
    int err;
    switch(event->type){
        case SDL_KEYDOWN: {

            /* Enter a line of console input */
            if(event->key.keysym.sym == SDLK_RETURN){
                console_newline(&app->console);

                err = test_app_process_console_input(app);
                if(err)return err;

                console_input_clear(&app->console);
                console_write_msg(&app->console, TEST_APP_CONSOLE_START_TEXT);
            }

            /* Tab completion */
            if(event->key.keysym.sym == SDLK_TAB){
                console_newline(&app->console);
                test_app_write_console_commands(app, app->console.input);
                console_write_msg(&app->console, TEST_APP_CONSOLE_START_TEXT);
                console_write_msg(&app->console, app->console.input);
            }

            /* Copy/paste a line of console input */
            if(
                event->key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)
                && event->key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT)
            ){
                if(event->key.keysym.sym == SDLK_c){
                    SDL_SetClipboardText(app->console.input);}
                if(event->key.keysym.sym == SDLK_v
                    && SDL_HasClipboardText()
                ){
                    char *input = SDL_GetClipboardText();
                    char *c = input;
                    while(*c != '\0'){
                        console_input_char(&app->console, *c);
                        c++;
                    }
                    SDL_free(input);
                }
            }

            if(event->key.keysym.sym == SDLK_BACKSPACE){
                console_input_backspace(&app->console);}
            if(event->key.keysym.sym == SDLK_DELETE){
                console_input_delete(&app->console);}

        } break;
        case SDL_TEXTINPUT: {
            for(char *c = event->text.text; *c != '\0'; c++){
                console_input_char(&app->console, *c);
            }
        } break;
        default: break;
    }
    return 0;
}

int test_app_blit_console(test_app_t *app, int x, int y){
    int err;

    geomfont_blitter_t blitter;
    geomfont_blitter_render_init(&blitter, app->geomfont,
        app->surface, app->sdl_palette,
        0, 0, x, y, 1, NULL, NULL);
    err = console_blit(&app->console, &geomfont_blitter_putc_callback,
        &blitter);
    if(err)return err;

    return 0;
}

void test_app_start_console(test_app_t *app){
    console_write_msg(&app->console, TEST_APP_CONSOLE_START_TEXT);
    console_write_msg(&app->console, app->console.input);
    SDL_StartTextInput();
    app->process_console = true;
}

void test_app_stop_console(test_app_t *app){
    SDL_StopTextInput();
    app->process_console = false;
}

void test_app_show_console(test_app_t *app){
    console_clear(&app->console);
    console_write_msg(&app->console, "Welcome to debug console! Try \"help\".\n");
    app->show_console = true;
    test_app_start_console(app);
}

void test_app_hide_console(test_app_t *app){
    console_newline(&app->console);
    console_write_msg(&app->console, "Leaving debug console...\n");
    app->show_console = false;
    test_app_stop_console(app);
}
