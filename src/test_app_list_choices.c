
#include <stdio.h>
#include <stdbool.h>

#include "console.h"
#include "test_app.h"
#include "test_app_list.h"
#include "test_app_list_utils.h"



/************************
* TEST_APP_LIST_CHOICES *
*************************/

int test_app_open_list_choices(test_app_t *app, const char *title,
    const char **options, int options_length, int options_index,
    int select_item(test_app_list_t *list)
){
    test_app_list_data_t *new_data = test_app_list_data_create(app);
    if(new_data == NULL)return 1;

    new_data->options = options;
    new_data->options_length = options_length;
    new_data->options_index = options_index;

    return test_app_open_list(app, title,
        0, 0,
        new_data,
        &test_app_list_choices_step,
        &test_app_list_choices_render,
        select_item? select_item: &test_app_list_choices_select_item,
        &test_app_list_cleanup_data);
}

int test_app_list_choices_step(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    data->length = 0;
    data->index = 0;
    data->item = NULL;
    data->options_index = _remainder(list->index_y, data->options_length);
    return 0;
}

int test_app_list_choices_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    console_t *console = &data->app->console;
    _console_write_options(console, data->options,
        data->options_index, data->options_length);
    return 0;
}

int test_app_list_choices_select_item(test_app_list_t *list){
    /* This is a sort of default implementation of selecting a choice:
    we simply close this list and return to the previous one, without
    doing anything with the selected item (data->options_index).
    Generally one passes a custom select_item callback to
    test_app_open_list_choices. */
    test_app_list_data_t *data = list->data;
    return test_app_close_list(data->app);
}

