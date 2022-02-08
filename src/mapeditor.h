#ifndef _MAPEDITOR_H_
#define _MAPEDITOR_H_

#include "hexcollmap.h"

int mapeditor(const char *collmap_filename, hexcollmap_write_options_t *opts,
    hexcollmap_t *collmap, hexcollmap_part_t **parts, int parts_len);


#endif