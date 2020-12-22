
CFLAGS += -O2 -g -rdynamic -std=c99 \
 `sdl2-config --libs --cflags` \
 -Wall -Werror -Wno-unused -Wno-missing-braces -Wno-tautological-compare -Wno-parentheses \
 -DGEOM_HEXGAME_DEBUG_FRAMERATE

PROGS = \
 bin/lexertool bin/collmaptool bin/hexpicturetest bin/sdltest bin/directorytest \
 bin/prendtool bin/demo bin/minieditor

TESTS = \
 bin/lexertest bin/frozenstringtest bin/geomtest bin/stringstoretest bin/varstest

PROGS += $(TESTS)

OFILES = \
 src/hexcollmap.o src/hexcollmap_parse.o src/hexcollmap_write.o \
 src/str_utils.o src/file_utils.o src/lexer.o src/write.o src/stringstore.o \
 src/vars.o src/var_utils.o src/valexpr.c \
 src/geom.o src/vec4.o src/hexspace.o src/bounds.o src/hexbox.o src/hexgame_location.o \
 src/font.o src/console.o src/directory.o src/directory_shell.o \
 src/hexpicture.o src/generic_printf.o

SDL_OFILES = \
 src/prismelrenderer.o src/prismelrenderer_parse.o \
 src/rendergraph.o src/anim.o src/util.o src/sdl_util.o \
 src/sdlfont.o src/geomfont.o src/minieditor.o \
 src/hexmap.o src/hexmap_submap_create_rgraph.o \
 src/hexgame.o src/hexgame_body.o src/hexgame_player.o src/hexgame_recording.o \
 src/hexgame_state.o src/hexgame_actor.o \
 src/hexgame_savelocation.o \
 src/test_app.o src/test_app_console.o src/test_app_game.o src/test_app_editor.o \
 src/test_app_list.o src/test_app_commands.o \
 src/test_app_list_choices.o src/test_app_list_maps.o src/test_app_list_submaps.o \
 src/test_app_list_bodies.o src/test_app_list_players.o src/test_app_list_actors.o \
 src/test_app_list_recording.o src/test_app_list_utils.o

.PHONY: all clean check www

all: $(PROGS)

clean:
	rm -f src/*.o src/main/*.o $(PROGS)

check: $(TESTS)
	./runtests.sh $(TESTS)

bin/collmaptool: src/main/collmaptool.o $(OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/lexertest: src/main/lexertest.o $(OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/lexertool: src/main/lexertool.o $(OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/frozenstringtest: src/main/frozen_string_test.o src/frozen_string.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/geomtest: src/main/geomtest.o $(OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/stringstoretest: src/main/stringstoretest.o src/stringstore.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/varstest: src/main/varstest.o src/vars.o src/lexer.o src/write.o src/str_utils.o src/file_utils.o src/var_utils.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/hexpicturetest: src/main/hexpicture.o src/hexpicture.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/sdltest: src/main/sdltest.o src/util.o src/file_utils.o
	mkdir -p bin
	$(CC) $(CFLAGS) -lm -o $@ $^

bin/directorytest: src/main/directorytest.o src/directory.o src/directory_shell.o $(OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/prendtool: src/main/prendtool.o $(OFILES) $(SDL_OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/demo: src/main/demo.o $(OFILES) $(SDL_OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/minieditor: src/main/minieditor.o $(OFILES) $(SDL_OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^
