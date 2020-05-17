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

    void *data;
        /* If data != NULL, app is in "list mode" (so, up/down keys scroll through
        list etc) instead of the default "console mode" (where you type commands etc). */

    int index_x;
    int index_y;
        /* For now, the callbacks *must* implement wraparound themselves:
        there is no way to query list length, so indexes are unbound */

    /* Callbacks */
    test_app_list_callback_t *render;
    test_app_list_callback_t *cleanup;
    test_app_list_callback_t *select_item;
        /* E.g. user hits "enter" on a particular item */
    test_app_list_callback_t *back;
        /* E.g. user hits the "back" button */

} test_app_list_t;

void test_app_list_clear(test_app_list_t *list);
void test_app_list_cleanup(test_app_list_t *list);


/*********************
* TEST_APP_LIST_DATA *
*********************/

typedef struct test_app_list_data {
    struct test_app *app;

    const char *title;

    /* Now a bag of fields which might be useful depending on what you're
    listing. This structure doesn't know; that's determined by the callbacks
    you passed to the app_list_t. */
    hexmap_t *map;
    hexmap_submap_t *submap;
} test_app_list_data_t;

test_app_list_data_t *test_app_list_data_create(struct test_app *app, const char *title);
void test_app_list_data_cleanup(test_app_list_data_t *data);


/*******************************
* TEST_APP_LIST_DATA CALLBACKS *
*******************************/

int test_app_list_cleanup_data(test_app_list_t *list);
int test_app_list_maps_render(test_app_list_t *list);
int test_app_list_bodies_render(test_app_list_t *list);
int test_app_list_players_render(test_app_list_t *list);
int test_app_list_actors_render(test_app_list_t *list);


#endif