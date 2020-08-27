set -e

TARGET="emcc"

RELEASE=0
if [ "$1" = "release" ]
then
    RELEASE=1
    shift
    echo "Starting a release build..."
else
    echo "Starting a debug build..."
fi

MAIN_C="src/main_demo.c"

CFLAGS=""
CFLAGS+=" -std=c99 -Wall -Werror -Wno-unused -Wno-missing-braces -Wno-tautological-compare -Wno-parentheses"

SOURCES=""
SOURCES+=" src/anim.c src/hexmap.c src/hexcollmap.c src/hexcollmap_parse.c src/hexmap_submap_create_rgraph.c"
SOURCES+=" src/util.c src/str_utils.c src/file_utils.c src/lexer.c src/write.c src/stringstore.c src/vars.c"
SOURCES+=" src/geom.c src/vec4.c src/hexspace.c src/bounds.c src/location.c"
SOURCES+=" src/font.c src/sdlfont.c src/geomfont.c src/console.c"
SOURCES+=" src/test_app.c src/test_app_console.c src/test_app_game.c src/test_app_editor.c"
SOURCES+=" src/test_app_list.c src/test_app_commands.c src/hexgame.c"
SOURCES+=" src/test_app_list_choices.c src/test_app_list_maps.c src/test_app_list_submaps.c"
SOURCES+=" src/test_app_list_bodies.c src/test_app_list_players.c src/test_app_list_actors.c"
SOURCES+=" src/test_app_list_recording.c src/test_app_list_utils.c"
SOURCES+=" src/hexgame_body.c src/hexgame_player.c src/hexgame_recording.c src/hexgame_state.c"
SOURCES+=" src/hexgame_actor.c"
SOURCES+=" src/prismelrenderer.c src/prismelrenderer_parse.c"
SOURCES+=" src/hexpicture.c"
SOURCES+=" src/rendergraph.c src/sdl_util.c src/generic_printf.c"

if [ "$TARGET" = "emcc" ]
then
    CC="emcc"
    OUTFILE="main.html"
    CFLAGS+=" -DNO_EXECINFO"
    CFLAGS+=" -s USE_SDL=2 "
    CFLAGS+=" --preload-file data --preload-file actor --preload-file anim"

    #CFLAGS+=" -s TOTAL_MEMORY=1073741824" # 1024*1024*1024 = 1G
    CFLAGS+=" -s ALLOW_MEMORY_GROWTH=1"

    if [ "$RELEASE" = 1 ]
    then
        CFLAGS+=" -O2"
    else
        CFLAGS+=" -O0 -g"
    fi
else
    echo "Bad target: $TARGET"
    exit 1
fi

"$CC" $CFLAGS $SOURCES "$MAIN_C" -o "$OUTFILE" $@

if [ "$TARGET" = "emcc" ]
then

    # We've already got a modified version of emcc's main.html in www/,
    # so get rid of the unmodified version it generates:
    #
    EMCC_FILES=$(echo main.{html,html.map})
    echo "Removing files: $EMCC_FILES"
    rm $EMCC_FILES

    # Remove the following files or move them into www/:
    #
    WWW_EMCC_FILES=$(echo main.{js,html.mem,data})
    if [ "$RELEASE" = 1 ]
    then
        echo "Copying www files: $WWW_EMCC_FILES"
        cp $WWW_EMCC_FILES www/
    else
        echo "Removing www files: $WWW_EMCC_FILES"
        rm $WWW_EMCC_FILES
    fi

fi