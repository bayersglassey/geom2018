

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "test_app_menu.h"
#include "test_app.h"


const char *test_app_menu_titles[TEST_APP_MENU_SCREENS] = {
    "Spider Game", "Load Game", "Paused"};
const char *_title_options[] = {"New game", "Load game", NULL};
const char *_load_game_options[] = {"Slot 1", "Slot 2", "Slot 3", NULL};
const char *_paused_options[] = {"Continue", "Exit to title", NULL};
const char **test_app_menu_options[TEST_APP_MENU_SCREENS] = {
    _title_options, _load_game_options, _paused_options};


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
    test_app_menu_set_screen(menu, TEST_APP_MENU_SCREEN_TITLE);
}

void test_app_menu_set_screen(test_app_menu_t *menu, int screen_i){
    menu->screen_i = screen_i;
    menu->option_i = 0;
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

int test_app_menu_select(test_app_menu_t *menu){
    int err;
    test_app_t *app = menu->app;
    switch(menu->screen_i){
        case TEST_APP_MENU_SCREEN_TITLE:
            switch(menu->option_i){
                case 0: /* NEW GAME */
                    fprintf(stderr, "TODO: new game\n");
                    app->show_menu = false;
                    break;
                case 1: /* LOAD GAME */
                    test_app_menu_set_screen(menu,
                        TEST_APP_MENU_SCREEN_LOAD_GAME);
                    break;
                default:
                    fprintf(stderr, "Unrecognized option: %i\n",
                        menu->option_i);
                    return 2;
            }
            break;
        case TEST_APP_MENU_SCREEN_LOAD_GAME:
            fprintf(stderr, "TODO: load slot %i\n", menu->option_i);
            app->show_menu = false;
            break;
        case TEST_APP_MENU_SCREEN_PAUSED:
            switch(menu->option_i){
                case 0: /* CONTINUE */
                    app->show_menu = false;
                    break;
                case 1: /* EXIT TO TITLE */
                    fprintf(stderr, "TODO: exit to title\n");
                    test_app_menu_set_screen(menu,
                        TEST_APP_MENU_SCREEN_TITLE);
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

void test_app_menu_render(test_app_menu_t *menu){
    test_app_t *app = menu->app;
    const char *title = test_app_menu_titles[menu->screen_i];
    const char **options = test_app_menu_options[menu->screen_i];
    int n_options = _get_n_options(menu);

    int col = 10;
    app->lines_printed += 10;

    test_app_printf(app, col, app->lines_printed * app->font.char_h,
        "== %s ==\n", title);
    test_app_printf(app, col, app->lines_printed * app->font.char_h,
        "\n");
    for(int i = 0; i < n_options; i++){
        test_app_printf(app, col, app->lines_printed * app->font.char_h,
            "[%c] %s\n",
            menu->option_i == i? '*': ' ',
            options[i]);
    }
}
