#ifndef _TEST_APP_LIST_H_
#define _TEST_APP_LIST_H_


#include "hexmap.h"


struct test_app;
struct test_app_list;
struct test_app_list_data;


typedef int test_app_list_callback_t(struct test_app_list *list);


/****************
* TEST_APP_LIST *
****************/

typedef struct test_app_list {
    /* Structure allowing app's console to display a list of objects.
    Includes callbacks for list navigation, list item rendering, etc. */

    const char *title;

    struct test_app_list *prev;
        /* Stack of lists: you can push new ones, hit "back" button to go
        back to previous list, kind of thing */

    int index_x;
    int index_y;
        /* For now, the callbacks *must* implement wraparound themselves:
        there is no way to query list length, so indexes are unbound */

    /* Callbacks */
    void *data;
    test_app_list_callback_t *step;
    test_app_list_callback_t *render;
    test_app_list_callback_t *cleanup;
    test_app_list_callback_t *select_item;
        /* E.g. user hits "enter" */
    test_app_list_callback_t *back;
        /* E.g. user hits the "back" key */

} test_app_list_t;

void test_app_list_init(test_app_list_t *list,
    const char *title, test_app_list_t *prev,
    int index_x, int index_y,
    void *data,
    test_app_list_callback_t *step,
    test_app_list_callback_t *render,
    test_app_list_callback_t *select_item,
    test_app_list_callback_t *cleanup);
void test_app_list_cleanup(test_app_list_t *list);


/*********************
* TEST_APP_LIST_DATA *
*********************/

typedef struct test_app_list_data {
    /* Weakrefs */

    int index;
    int length;
    void *item;

    const char **options;
    int options_index;
    int options_length;

    struct test_app *app;
    /* The following fields may or may not be used depending on the callbacks
    passed to the app_list_t: */
    body_t *body;
    hexmap_t *map;
    hexmap_submap_t *submap;
} test_app_list_data_t;

test_app_list_data_t *test_app_list_data_create(struct test_app *app);
void test_app_list_data_cleanup(test_app_list_data_t *data);


/*******************************
* TEST_APP_LIST_DATA CALLBACKS *
*******************************/

int test_app_list_cleanup_data(test_app_list_t *list);

int test_app_open_list_maps(struct test_app *app, body_t *body, hexmap_t *map);
int test_app_list_maps_step(test_app_list_t *list);
int test_app_list_maps_render(test_app_list_t *list);
int test_app_list_maps_select_item(test_app_list_t *list);

int test_app_open_list_bodies(struct test_app *app, body_t *body, hexmap_t *map);
int test_app_list_bodies_step(test_app_list_t *list);
int test_app_list_bodies_render(test_app_list_t *list);
int test_app_list_bodies_select_item(test_app_list_t *list);

int test_app_open_list_players(struct test_app *app);
int test_app_list_players_step(test_app_list_t *list);
int test_app_list_players_render(test_app_list_t *list);
int test_app_list_players_select_item(test_app_list_t *list);

int test_app_open_list_actors(struct test_app *app);
int test_app_list_actors_step(test_app_list_t *list);
int test_app_list_actors_render(test_app_list_t *list);
int test_app_list_actors_select_item(test_app_list_t *list);


#endif