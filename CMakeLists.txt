cmake_minimum_required(VERSION 3.28)
project(cyka2pp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

find_package(nlohmann_json 3.11 REQUIRED)
find_package(Threads REQUIRED)
find_library(SNAPPY_LIB snappy REQUIRED)

set(CYKA_LIB_SOURCES
    src/cli.cpp
    src/analyze.cpp
    src/geom/mesh.cpp
    src/geom/bvh.cpp
    src/io/json_writer.cpp
    src/io/json_player.cpp
    src/render/scoreboard.cpp
    src/render/table.cpp
    src/render/clutches.cpp
    src/render/highlights.cpp
    src/render/aim.cpp
    src/render/rounds_kills.cpp
    src/demo/mapped_file.cpp
    src/demo/snappy_util.cpp
    src/demo/stream.cpp
    src/demo/header.cpp
    src/demo/event_desc.cpp
    src/demo/game_event.cpp
    src/demo/packet_walk.cpp
    src/demo/string_tables.cpp
    src/demo/listener.cpp
    src/demo/listener_events.cpp
    src/demo/listener_rounds.cpp
    src/demo/listener_extras.cpp
    src/demo/ent/field_path.cpp
    src/demo/ent/field_path_ops.cpp
    src/demo/ent/quantized_float.cpp
    src/demo/ent/decoder.cpp
    src/demo/ent/decoder_types.cpp
    src/demo/ent/decoder_select.cpp
    src/demo/ent/field_type.cpp
    src/demo/ent/field.cpp
    src/demo/ent/entity.cpp
    src/demo/ent/flattener.cpp
    src/demo/ent/context_tables.cpp
    src/demo/ent/context_entities.cpp
    src/demo/ent/string_table_blob.cpp
    src/demo/ent/baselines.cpp
    src/demo/ent/pose_sampler.cpp
    src/demo/ent_bridge.cpp
    src/demo/parser.cpp
    src/demo/build_match.cpp
    src/metrics/ratings.cpp
    src/metrics/compute.cpp
    src/metrics/clutches.cpp
    src/metrics/swing.cpp
    src/highlights/tags.cpp
    src/highlights/tag_rules.cpp
    src/highlights/build.cpp
    src/aim/enrich.cpp
    src/aim/build_samples.cpp
    src/aim/compute.cpp
    src/aim/los_batch.cpp
    src/aim/ttd.cpp
    src/aim/spray.cpp
    src/aim/counter_strafe.cpp
    src/aim/spotted.cpp
    src/aim/crosshair.cpp
    src/csdata/weapons.cpp
    src/csdata/spray_tables.cpp
    src/csdata/spray_base_rifles.cpp
    src/csdata/spray_base_smg.cpp
    src/csdata/spray_base_lmg.cpp
    src/csdata/spray_scoped.cpp
    src/csdata/spray_nosil.cpp
)

add_library(cyka_core STATIC ${CYKA_LIB_SOURCES})
target_include_directories(cyka_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(cyka_core PUBLIC
    nlohmann_json::nlohmann_json
    Threads::Threads
    ${SNAPPY_LIB}
)

add_executable(cyka2pp src/main.cpp)
target_link_libraries(cyka2pp PRIVATE cyka_core)

enable_testing()
add_executable(cyka2pp-tests
    tests/test_main.cpp
    tests/test_ratings.cpp
    tests/test_cli.cpp
    tests/test_vision.cpp
    tests/test_tags.cpp
    tests/test_golden.cpp
)
target_link_libraries(cyka2pp-tests PRIVATE cyka_core)
target_compile_definitions(cyka2pp-tests PRIVATE
    CYKA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    CYKA_DEMO_DIR="${CMAKE_SOURCE_DIR}/../dotdem"
    CYKA_MAPS_DIR="${CMAKE_SOURCE_DIR}/../cs2-maps-tri"
)
add_test(NAME cyka2pp-tests COMMAND cyka2pp-tests)

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
  add_custom_target(cyka_tidy
    COMMAND ${CLANG_TIDY_EXE}
      -p ${CMAKE_BINARY_DIR}
      --warnings-as-errors=*
      ${CYKA_LIB_SOURCES} src/main.cpp
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-tidy (warnings as errors)"
    VERBATIM
  )
endif()
