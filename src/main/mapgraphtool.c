
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "../hexmap.h"
#include "../hexgame.h"
#include "../defaults.h"
#include "../tooling.h"


#define DEFAULT_FONTSIZE 20


typedef struct opts{
    FILE *file;
} opts_t;
static void opts_init(opts_t *opts){
    opts->file = stdout;
}


static void print_usage(FILE *file){
    fprintf(stderr,
        "Usage: mapgraphtool [OPTION ...] [--] FILENAME"
        "\n"
        "Reads FILENAME, parsing it as a hexmap, and outputs it as a graph\n"
        "(expressed in the Graphviz \"dot\" language).\n"
        "\n"
        "Options:\n"
        "   -h --help            Print this message and exit\n"
        "      --prend FILENAME  Prismelrenderer (default: %s)\n"
    , DEFAULT_PREND_FILENAME);
}


/******************************************************************************/
/* WORLDMAPS (i.e. a set of worldmap filenames) */

static int worldmaps_len(const char **worldmaps){
    if(!worldmaps)return 0;
    const char **p = worldmaps;
    for(; *p; p++);
    return p - worldmaps;
}

static bool worldmaps_contains(const char **worldmaps, const char *filename){
    fprintf(stderr, "=== CONTAINS: %s\n", filename);
    if(!worldmaps)return false;
    for(const char **p = worldmaps; *p; p++){
        if(!strcmp(*p, filename))return true;
    }
    return false;
}

static int worldmaps_push(const char ***worldmaps_ptr, const char *filename){
    /* Pushes a filename onto the end of the array */
    fprintf(stderr, "=== PUSH: %s\n", filename);
    int n_worldmaps = worldmaps_len(*worldmaps_ptr) + 1;
    const char **worldmaps = realloc(*worldmaps_ptr, sizeof(*worldmaps) * (n_worldmaps + 1));
    if(!worldmaps)return 1;
    worldmaps[n_worldmaps - 1] = filename;
    worldmaps[n_worldmaps] = NULL;
    *worldmaps_ptr = worldmaps;
    return 0;
}

static int worldmaps_add(const char ***worldmaps_ptr, const char *filename){
    /* Adds a filename to the set */
    fprintf(stderr, "=== ADD: %s\n", filename);
    if(worldmaps_contains(*worldmaps_ptr, filename))return 0;
    return worldmaps_push(worldmaps_ptr, filename);
}


/******************************************************************************/
/* SUBMAP DOORS ITER */

typedef struct submap_doors_iter {
    hexmap_submap_t *submap;
    int i;
    const char *map_filename;
    const char *location_name;
} submap_doors_iter_t;

static void submap_doors_iter_init(submap_doors_iter_t *it, hexmap_submap_t *submap){
    it->submap = submap;
    it->i = 0;
}

static bool submap_doors_iter_next(submap_doors_iter_t *it){
    hexmap_submap_t *submap = it->submap;
    hexcollmap_t *collmap = &submap->collmap;
    while(it->i < collmap->recordings_len){
        hexmap_recording_t *recording = collmap->recordings[it->i];
        it->i++;
        const char *map_filename = vars_get_str(&recording->vars, "map");
        const char *location_name = vars_get_str(&recording->vars, "location");
        if(!map_filename && location_name){
            map_filename = submap->map->filename;
        }
        if(!map_filename)continue;
        it->map_filename = map_filename;
        it->location_name = location_name;
        return true;
    }
    return false;
}


/******************************************************************************/
/* DUMPING (i.e. writing graphviz output) */

static int dump_map(hexmap_t *map, const char ***worldmaps_ptr, opts_t *opts){
    int err;

    hexgame_t *game = map->game;

    /* NOTE: set this to "cluster_" in order to have graphviz render boxes
    around map subgraphs */
    const char *subgraph_prefix = "";

    fprintf(opts->file, "  subgraph \"%smap_%s\" {\n", subgraph_prefix, map->filename);
    fprintf(opts->file, "    label = \"Map: %s\";\n", map->filename);
    for(int i = 0; i < map->submap_groups_len; i++){
        hexmap_submap_group_t *group = map->submap_groups[i];
        bool is_root = group->name[0] == '\0';
        /* Node for group (or, if root group, basically node for its map) */
        fprintf(opts->file, "    \"map_%s_group_%s\" [label=\"%s\", shape=\"%s\", margin=%f, fontsize=%i]\n",
            map->filename, group->name,
            is_root? map->filename: group->name,
            is_root? "box": "ellipse",
            is_root? .3: .1,
            is_root? 24: DEFAULT_FONTSIZE);
        if(group->parent){
            /* Edge to the group from its parent */
            fprintf(opts->file, "    \"map_%s_group_%s\" -> \"map_%s_group_%s\"\n",
                map->filename, group->parent->name,
                map->filename, group->name);
        }
    }
    for(int i = 0; i < map->submaps_len; i++){
        hexmap_submap_t *submap = map->submaps[i];
        int submap_i = i;
        /* Node for submap */
        fprintf(opts->file, "    \"map_%s_submap_%i\" [label=\"%s%s\", shape=\"box\", fontsize=%i]\n",
            map->filename, submap_i,
            submap->filename, submap->has_mappoint? "\\n[MAP POINT]": "",
            DEFAULT_FONTSIZE);
        /* Edge to submap from its group */
        fprintf(opts->file, "    \"map_%s_group_%s\" -> \"map_%s_submap_%i\"\n",
            map->filename, submap->group->name,
            map->filename, submap_i);

        /* Now find all doors going out of this submap.
        Doors can go to other maps, and each map has a corresponding "cluster"
        subgraph, and the way graphviz cluster subgraphs work is that a node
        belongs to the subgraph it was "declared" in, and being mentioned in
        an edge counts as "declaring" the node.
        So, we need to be careful *not* to add edges for doors here, because
        that could result in nodes which belong to other maps being included
        in the subgraph for our current map.
        So instead, we just make note here of all maps to which we have doors,
        and then later, after we've added subgraphs for all maps, *then* we
        will add the edges for all doors.
        MAKE SENSE?.. */
        submap_doors_iter_t it;
        submap_doors_iter_init(&it, submap);
        while(submap_doors_iter_next(&it)){
            err = worldmaps_add(worldmaps_ptr, it.map_filename);
            if(err)return err;
        }
    }
    fprintf(opts->file, "  }\n");

    return 0;
}


int main(int n_args, char **args){
    const char *prend_filename = DEFAULT_PREND_FILENAME;

    opts_t _opts, *opts=&_opts;
    opts_init(opts);

    int arg_i = 1;
    for(; arg_i < n_args; arg_i++){
        char *arg = args[arg_i];
        if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            print_usage(stdout);
            return 0;
        }else if(!strcmp(arg, "--prend")){
            if(n_args - arg_i < 2){
                fprintf(stderr, "Missing value for option: %s\n", arg);
                return 2;
            }
            prend_filename = args[arg_i+1];
            arg_i++;
        }else if(!strcmp(arg, "--")){
            arg_i++;
            break;
        }else{
            break;
        }
    }

    int remaining_args = n_args - arg_i;
    if(remaining_args != 1){
        print_usage(stderr);
        return 2;
    }

    const char *map_filename = args[arg_i];

    /* Load the hexmap */
    {
        int err;

        hexgame_t _game, *game=&_game;
        err = hexgame_init_for_tooling(game, prend_filename);
        if(err)return err;

        err = hexgame_load_worldmaps(game, HEXGAME_DEFAULT_WORLDMAPS_FILENAME);
        if(err)return err;

        const char **worldmaps = NULL;
        err = worldmaps_push(&worldmaps, map_filename);
        if(err)return err;

        /* What we came here for... */
        fprintf(opts->file, "digraph {\n");
        fprintf(opts->file, "  rankdir = \"LR\";\n");
        fprintf(opts->file, "  fontsize = %i;\n", DEFAULT_FONTSIZE);
        for(int i = 0; i < worldmaps_len(worldmaps); i++){
            /* Each time around this loop, we process another worldmap.
            We begin with a single worldmap in the set, `worldmaps`.
            When we process a worldmap, it may add more worldmaps to the set,
            which we will then process in subsequent loop iterations.
            In other words, we're iterating over a set while it grows... */
            hexmap_t *map;
            err = hexgame_load_map(game, worldmaps[i], &map);
            if(err)return err;
            err = dump_map(map, &worldmaps, opts);
            if(err)return err;
        }
        for(int i = 0; i < worldmaps_len(worldmaps); i++){
            /* Now that we have all subgraphs and nodes, with the nodes
            declared in the correct subgraphs, we can add edges between
            nodes which might not be in the same graph */
            hexmap_t *map = hexgame_get_map(game, worldmaps[i]);
            for(int j = 0; j < map->submaps_len; j++){
                hexmap_submap_t *submap = map->submaps[j];
                int submap_i = j;
                submap_doors_iter_t it;
                submap_doors_iter_init(&it, submap);
                while(submap_doors_iter_next(&it)){
                    const char *map_filename = it.map_filename;
                    const char *location_name = it.location_name;
                    if(location_name){
                        hexmap_t *other_map;
                        err = hexgame_get_or_load_map(game, map_filename, &other_map);
                        if(err)return err;
                        hexmap_submap_t *other_submap = hexmap_get_location_submap(other_map, location_name);
                        if(!other_submap){
                            fprintf(stderr, "Couldn't find submap for location \"%s\" for map \"%s\"\n",
                                map_filename, location_name);
                            return 2;
                        }
                        int other_submap_i = hexmap_get_submap_index(other_map, other_submap);
                        /* Edge from door's submap to the one it points to */
                        fprintf(opts->file, "  \"map_%s_submap_%i\" -> \"map_%s_submap_%i\" [style=\"dashed\"]\n",
                            map->filename, submap_i,
                            map_filename, other_submap_i);
                    }else{
                        /* Edge from door's submap to the root submap group of the map it points to */
                        fprintf(opts->file, "  \"map_%s_submap_%i\" -> \"map_%s_group_\" [style=\"dashed\"]\n",
                            map->filename, submap_i,
                            map_filename);
                    }
                }
            }
        }
        fprintf(opts->file, "}\n");

        free(worldmaps);
        hexgame_cleanup_for_tooling(game);
    }
    return 0;
}
