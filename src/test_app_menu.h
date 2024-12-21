#ifndef _TEST_APP_MENU_H_
#define _TEST_APP_MENU_H_


/* Don't #include "test_app.h" here, do it in test_app_menu.c, to
avoid a circular dependency between test_app.h and this file. */
struct test_app;


enum test_app_menu_screen {
    TEST_APP_MENU_SCREEN_TITLE,
    TEST_APP_MENU_SCREEN_START_GAME,
    TEST_APP_MENU_SCREEN_NEW_GAME,
    TEST_APP_MENU_SCREEN_DELETE_GAME,
    TEST_APP_MENU_SCREEN_PAUSED,
    TEST_APP_MENU_SCREENS
};


typedef struct test_app_menu {
    int screen_i; /* test_app_menu_screen_t */
    int option_i;

    const char *message;

    /* Weakrefs: */
    struct test_app *app;
} test_app_menu_t;


void test_app_menu_cleanup(test_app_menu_t *menu);
void test_app_menu_init(test_app_menu_t *menu, struct test_app *app);
void test_app_menu_set_screen(test_app_menu_t *menu, int screen_i);
void test_app_menu_up(test_app_menu_t *menu);
void test_app_menu_down(test_app_menu_t *menu);
void test_app_menu_left(test_app_menu_t *menu);
void test_app_menu_right(test_app_menu_t *menu);
void test_app_menu_back(test_app_menu_t *menu);
int test_app_menu_select(test_app_menu_t *menu);
void test_app_menu_render(test_app_menu_t *menu);

bool test_app_menu_pauses_game(test_app_menu_t *menu);

#endif
