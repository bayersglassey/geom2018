

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "test_app_menu.h"
#include "test_app.h"
#include "save_slots.h"


const char *test_app_menu_titles[TEST_APP_MENU_SCREENS] = {
    "Spider Game", "Start Game", "Delete saved game", "Paused"};
int test_app_menu_parents[TEST_APP_MENU_SCREENS] = {
    -1, TEST_APP_MENU_SCREEN_TITLE, TEST_APP_MENU_SCREEN_TITLE, -1};
bool _test_app_menu_pauses_game[TEST_APP_MENU_SCREENS] = {
    false, false, false, true};
const char *_title_options[] = {"Start game", "Delete saved game", "Quit game", NULL};
const char *_save_slot_options[] = {"Slot 1", "Slot 2", "Slot 3", "Back to title screen", NULL};
const char *_paused_options[] = {"Continue", "Exit to title screen", "Quit game", NULL};
const char **test_app_menu_options[TEST_APP_MENU_SCREENS] = {
    _title_options, _save_slot_options, _save_slot_options, _paused_options};


/* Caches the result of get_save_slot_file_exists(i) for
0 <= i < SAVE_SLOTS, for use with TEST_APP_MENU_SCREEN_START_GAME */
bool save_slot_file_exists[SAVE_SLOTS] = {0};



static int _get_n_options(test_app_menu_t *menu){
    const char **options = test_app_menu_options[menu->screen_i];
    int i = 0;
    while(options[i])i++;
    return i;
}

void test_app_menu_cleanup(test_app_menu_t *menu){
    /* Nothin to do */
}

void test_app_menu_init(test_app_menu_t *menu, test_app_t *app){
    menu->app = app;
    menu->message = NULL;
    test_app_menu_set_screen(menu, TEST_APP_MENU_SCREEN_TITLE);
}

void test_app_menu_set_screen(test_app_menu_t *menu, int screen_i){

    /* Caller can set message afterwards */
    menu->message = NULL;

    menu->screen_i = screen_i;
    menu->option_i = 0;

    const char **options = test_app_menu_options[menu->screen_i];
    if(options == _save_slot_options){
        for(int i = 0; i < SAVE_SLOTS; i++){
            /* Check and cache whether each save slot has a
            corresponding file, so that e.g. we know whether to
            render each save slot as "New game" or "Load game" */
            save_slot_file_exists[i] = get_save_slot_file_exists(i);
        }
    }
}

void test_app_menu_up(test_app_menu_t *menu){
    int n_options = _get_n_options(menu);
    if(menu->option_i > 0)menu->option_i--;
    else menu->option_i = n_options - 1;
}

void test_app_menu_down(test_app_menu_t *menu){
    int n_options = _get_n_options(menu);
    if(menu->option_i < n_options - 1)menu->option_i++;
    else menu->option_i = 0;
}

void test_app_menu_left(test_app_menu_t *menu){
    /* Does nothing for now */
}

void test_app_menu_right(test_app_menu_t *menu){
    /* Does nothing for now */
}

void test_app_menu_back(test_app_menu_t *menu){
    int parent_screen_i = test_app_menu_parents[menu->screen_i];
    if(parent_screen_i >= 0){
        test_app_menu_set_screen(menu, parent_screen_i);
    }else{
        switch(menu->screen_i){
            case TEST_APP_MENU_SCREEN_PAUSED:
                menu->app->show_menu = false;
                break;
            default: break;
        }
    }
}

int test_app_menu_select(test_app_menu_t *menu){
    int err;
    test_app_t *app = menu->app;
    switch(menu->screen_i){
        case TEST_APP_MENU_SCREEN_TITLE:
            switch(menu->option_i){
                case 0: /* START GAME */
                    test_app_menu_set_screen(menu,
                        TEST_APP_MENU_SCREEN_START_GAME);
                    break;
                case 1: /* DELETE GAME */
                    test_app_menu_set_screen(menu,
                        TEST_APP_MENU_SCREEN_DELETE_GAME);
                    break;
                case 2: /* QUIT */
                    app->state = TEST_APP_STATE_QUIT;
                    app->show_menu = false;
                    break;
                default:
                    fprintf(stderr, "Unrecognized option: %i\n",
                        menu->option_i);
                    return 2;
            }
            break;
        case TEST_APP_MENU_SCREEN_START_GAME:
            if(menu->option_i == _get_n_options(menu) - 1){
                test_app_menu_back(menu);
            }else{
                app->state = TEST_APP_STATE_START_GAME;
                app->save_slot = menu->option_i;
                app->show_menu = false;
            }
            break;
        case TEST_APP_MENU_SCREEN_DELETE_GAME:
            if(menu->option_i == _get_n_options(menu) - 1){
                test_app_menu_back(menu);
            }else if(save_slot_file_exists[menu->option_i]){
                err = delete_save_slot(menu->option_i);
                if(err)return err;
                save_slot_file_exists[menu->option_i] = false;
            }else{
                /* Save slot doesn't exist, so don't try to delete it */
            }
            break;
        case TEST_APP_MENU_SCREEN_PAUSED:
            switch(menu->option_i){
                case 0: /* CONTINUE */
                    app->show_menu = false;
                    break;
                case 1: /* EXIT TO TITLE */
                    test_app_menu_set_screen(menu,
                        TEST_APP_MENU_SCREEN_TITLE);
                    app->state = TEST_APP_STATE_TITLE_SCREEN;
                    break;
                case 2: /* QUIT */
                    app->state = TEST_APP_STATE_QUIT;
                    app->show_menu = false;
                    break;
                default:
                    fprintf(stderr, "Unrecognized option: %i\n",
                        menu->option_i);
                    return 2;
            }
            break;
        default:
            fprintf(stderr, "Unrecognized menu screen: %i\n",
                menu->screen_i);
            return 2;
    }
    return 0;
}

static void _render_option(test_app_menu_t *menu, int i, int col,
    const char *option
){
    test_app_t *app = menu->app;

    if(!option){
        const char **options = test_app_menu_options[menu->screen_i];
        option = options[i];
    }

    test_app_printf(app, col, app->lines_printed * app->font.char_h,
        "[%c] %s\n",
        menu->option_i == i? '*': ' ',
        option);
}
void test_app_menu_render(test_app_menu_t *menu){
    test_app_t *app = menu->app;
    const char *title = test_app_menu_titles[menu->screen_i];
    const char **options = test_app_menu_options[menu->screen_i];
    int n_options = _get_n_options(menu);

    int col = 10;
    app->lines_printed += 10;

    /* Render menu title */
    test_app_printf(app, col, app->lines_printed * app->font.char_h,
        "== %s ==\n", title);
    test_app_printf(app, col, app->lines_printed * app->font.char_h,
        "\n");

    /* Special messages */
    if(menu->screen_i == TEST_APP_MENU_SCREEN_DELETE_GAME){
        test_app_printf(app, col, app->lines_printed * app->font.char_h,
            "WARNING: deleting a save file is permanent!\n\n");
    }

    /* Render menu options */
    if(options == _save_slot_options){
        for(int i = 0; i < n_options - 1; i++){
            const char *option;
            if(save_slot_file_exists[i]){
                static char _option[] = "Slot i: Saved game";
                _option[5] = '1' + i;
                option = _option;
            }else{
                static char _option[] = "Slot i: [empty]";
                _option[5] = '1' + i;
                option = _option;
            }
            _render_option(menu, i, col, option);
        }
        _render_option(menu, n_options - 1, col, NULL);
    }else{
        for(int i = 0; i < n_options; i++){
            _render_option(menu, i, col, NULL);
        }
    }

    /* Render message */
    if(menu->message){
        test_app_printf(app, col, app->lines_printed * app->font.char_h,
            "\n");
        test_app_printf(app, col, app->lines_printed * app->font.char_h,
            menu->message);
        test_app_printf(app, col, app->lines_printed * app->font.char_h,
            "\n");
    }
}

bool test_app_menu_pauses_game(test_app_menu_t *menu){
    return _test_app_menu_pauses_game[menu->screen_i];
}
