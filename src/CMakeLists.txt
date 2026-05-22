file(GLOB JPEG_SRCS jpeg-6/*.c)
list(FILTER JPEG_SRCS EXCLUDE REGEX ".*jpegtran\\.c$")
list(FILTER JPEG_SRCS EXCLUDE REGEX ".*cjpeg\\.c$")
list(FILTER JPEG_SRCS EXCLUDE REGEX ".*djpeg\\.c$")
list(FILTER JPEG_SRCS EXCLUDE REGEX ".*rdjpgcom\\.c$")
list(FILTER JPEG_SRCS EXCLUDE REGEX ".*wrjpgcom\\.c$")
list(FILTER JPEG_SRCS EXCLUDE REGEX ".*jmemdos\\.c$")
list(FILTER JPEG_SRCS EXCLUDE REGEX ".*jmemname\\.c$")
list(FILTER JPEG_SRCS EXCLUDE REGEX ".*jload\\.c$")

add_library(jpeg6 STATIC ${JPEG_SRCS})
target_include_directories(jpeg6 PUBLIC jpeg-6)
target_compile_options(jpeg6 PRIVATE -w)

set(BOTLIB_SRCS
    botlib/be_aas_bspq3.c
    botlib/be_aas_cluster.c
    botlib/be_aas_debug.c
    botlib/be_aas_entity.c
    botlib/be_aas_file.c
    botlib/be_aas_main.c
    botlib/be_aas_move.c
    botlib/be_aas_optimize.c
    botlib/be_aas_reach.c
    botlib/be_aas_route.c
    botlib/be_aas_routealt.c
    botlib/be_aas_sample.c
    botlib/be_ai_char.c
    botlib/be_ai_chat.c
    botlib/be_ai_gen.c
    botlib/be_ai_goal.c
    botlib/be_ai_move.c
    botlib/be_ai_weap.c
    botlib/be_ai_weight.c
    botlib/be_ea.c
    botlib/be_interface.c
    botlib/l_crc.c
    botlib/l_libvar.c
    botlib/l_log.c
    botlib/l_memory.c
    botlib/l_precomp.c
    botlib/l_script.c
    botlib/l_struct.c
)

add_library(botlib STATIC ${BOTLIB_SRCS} game/q_shared.c game/q_math.c)
target_include_directories(botlib PUBLIC
    botlib
    game
    qcommon
)
target_compile_definitions(botlib PRIVATE BOTLIB)

set(RENDERER_SRCS
    renderer/tr_animation.c
    renderer/tr_backend.c
    renderer/tr_bsp.c
    renderer/tr_cmds.c
    renderer/tr_curve.c
    renderer/tr_flares.c
    renderer/tr_font.c
    renderer/tr_image.c
    renderer/tr_init.c
    renderer/tr_light.c
    renderer/tr_main.c
    renderer/tr_marks.c
    renderer/tr_mesh.c
    renderer/tr_model.c
    renderer/tr_noise.c
    renderer/tr_scene.c
    renderer/tr_shade.c
    renderer/tr_shade_calc.c
    renderer/tr_shader.c
    renderer/tr_shadows.c
    renderer/tr_sky.c
    renderer/tr_surface.c
    renderer/tr_world.c
    sdl/sdl_qgl.c
)

add_library(renderer STATIC ${RENDERER_SRCS})
target_compile_definitions(renderer PRIVATE QGL_LINKED)
target_include_directories(renderer PUBLIC
    renderer
    game
    qcommon
    unix
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
)
target_link_libraries(renderer PUBLIC jpeg6 ${OPENGL_LIBRARIES} ${SDL2_LIBRARIES})

set(QCOMMON_SRCS
    qcommon/cmd.c
    qcommon/cm_load.c
    qcommon/cm_patch.c
    qcommon/cm_polylib.c
    qcommon/cm_test.c
    qcommon/cm_trace.c
    qcommon/common.c
    qcommon/cvar.c
    qcommon/files.c
    qcommon/huffman.c
    qcommon/md4.c
    qcommon/msg.c
    qcommon/net_chan.c
    qcommon/unzip.c
    qcommon/vm.c
    qcommon/vm_interpreted.c
)
set_source_files_properties(qcommon/vm_x86.c PROPERTIES HEADER_FILE_ONLY TRUE)

set(CLIENT_SRCS
    client/cl_cgame.c
    client/cl_cin.c
    client/cl_console.c
    client/cl_input.c
    client/cl_keys.c
    client/cl_main.c
    client/cl_net_chan.c
    client/cl_parse.c
    client/cl_scrn.c
    client/cl_ui.c
    client/snd_adpcm.c
    client/snd_dma.c
    client/snd_mem.c
    client/snd_mix.c
    client/snd_wavelet.c
)

set(SERVER_SRCS
    server/sv_bot.c
    server/sv_ccmds.c
    server/sv_client.c
    server/sv_game.c
    server/sv_init.c
    server/sv_main.c
    server/sv_net_chan.c
    server/sv_snapshot.c
    server/sv_world.c
)

set(SHARED_SRCS
    game/q_shared.c
    game/q_math.c
)

set(PLATFORM_SRCS
    unix/unix_main.c
    unix/unix_net.c
    unix/unix_shared.c
    unix/linux_signals.c
    sdl/sdl_glimp.c
    sdl/sdl_snd.c
)

if(NOT APPLE)
    list(APPEND PLATFORM_SRCS unix/linux_common.c)
endif()

add_executable(Q3ARailgun
    ${QCOMMON_SRCS}
    ${CLIENT_SRCS}
    ${SERVER_SRCS}
    ${PLATFORM_SRCS}
    ${SHARED_SRCS}
    sdl/sdl_stubs.c
)

target_include_directories(Q3ARailgun PRIVATE
    game
    qcommon
    client
    server
    renderer
    unix
    sdl
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
)

target_link_libraries(Q3ARailgun PRIVATE
    renderer
    botlib
    jpeg6
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    m
    dl
    pthread
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "i.86|x86_64")
    target_compile_definitions(Q3ARailgun PRIVATE id386)
endif()

set(GAME_SRCS
    game/ai_chat.c
    game/ai_cmd.c
    game/ai_dmnet.c
    game/ai_dmq3.c
    game/ai_main.c
    game/ai_team.c
    game/ai_vcmd.c
    game/bg_lib.c
    game/bg_misc.c
    game/bg_pmove.c
    game/bg_slidemove.c
    game/g_active.c
    game/g_arenas.c
    game/g_bot.c
    game/g_client.c
    game/g_cmds.c
    game/g_combat.c
    game/g_items.c
    game/g_main.c
    game/g_mem.c
    game/g_misc.c
    game/g_missile.c
    game/g_mover.c
    game/g_session.c
    game/g_spawn.c
    game/g_svcmds.c
    game/g_syscalls.c
    game/g_target.c
    game/g_team.c
    game/g_trigger.c
    game/g_utils.c
    game/g_weapon.c
    game/q_math.c
    game/q_shared.c
)

add_library(qagame SHARED ${GAME_SRCS})
target_include_directories(qagame PRIVATE game qcommon)
target_compile_definitions(qagame PRIVATE GAME)
target_compile_options(qagame PRIVATE -fno-builtin)
set_target_properties(qagame PROPERTIES
    PREFIX ""
    OUTPUT_NAME "qagamex86_64"
)

set(CGAME_SRCS
    cgame/cg_consolecmds.c
    cgame/cg_draw.c
    cgame/cg_drawtools.c
    cgame/cg_effects.c
    cgame/cg_ents.c
    cgame/cg_event.c
    cgame/cg_info.c
    cgame/cg_localents.c
    cgame/cg_main.c
    cgame/cg_marks.c
    cgame/cg_players.c
    cgame/cg_playerstate.c
    cgame/cg_predict.c
    cgame/cg_scoreboard.c
    cgame/cg_servercmds.c
    cgame/cg_snapshot.c
    cgame/cg_syscalls.c
    cgame/cg_view.c
    cgame/cg_weapons.c
    game/bg_lib.c
    game/bg_misc.c
    game/bg_pmove.c
    game/bg_slidemove.c
    game/q_math.c
    game/q_shared.c
)

add_library(cgame SHARED ${CGAME_SRCS})
target_include_directories(cgame PRIVATE cgame game qcommon)
target_compile_definitions(cgame PRIVATE CGAME)
target_compile_options(cgame PRIVATE -fno-builtin)
set_target_properties(cgame PROPERTIES
    PREFIX ""
    OUTPUT_NAME "cgamex86_64"
)

file(GLOB UI_SRCS q3_ui/*.c)
list(FILTER UI_SRCS EXCLUDE REGEX ".*ui_login\\.c$")
list(FILTER UI_SRCS EXCLUDE REGEX ".*ui_rankings\\.c$")
list(FILTER UI_SRCS EXCLUDE REGEX ".*ui_rankstatus\\.c$")
list(FILTER UI_SRCS EXCLUDE REGEX ".*ui_signup\\.c$")
list(FILTER UI_SRCS EXCLUDE REGEX ".*ui_specifyleague\\.c$")
list(APPEND UI_SRCS
    game/bg_lib.c
    game/bg_misc.c
    game/q_math.c
    game/q_shared.c
    ui/ui_syscalls.c
)

add_library(ui SHARED ${UI_SRCS})
target_include_directories(ui PRIVATE q3_ui ui game qcommon)
target_compile_definitions(ui PRIVATE UI)
target_compile_options(ui PRIVATE -fno-builtin)
if(APPLE)
    target_link_options(ui PRIVATE -undefined dynamic_lookup)
endif()
set_target_properties(ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "uix86_64"
)
