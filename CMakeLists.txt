cmake_minimum_required(VERSION 3.28)
project(cyka2pp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Build knobs (CI / releases) ---
set(CYKA_MARCH "" CACHE STRING "Optional -march= value (e.g. x86-64-v2, x86-64-v4)")
option(CYKA_STATIC "Static-link libstdc++/libgcc and prefer static snappy" ON)
option(CYKA_FETCH_DEPS "Fetch nlohmann_json + snappy via FetchContent" OFF)

if(CYKA_STATIC)
  # Portable Linux binary: no libstdc++.so / libgcc_s.so dependency.
  # Still dynamically linked to glibc (fully -static + glibc is fragile).
  add_link_options(-static-libgcc -static-libstdc++)
endif()

include(FetchContent)

# Fetch deps BEFORE adding our -Werror / -march so they don't inherit them.
if(CYKA_FETCH_DEPS)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

  FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
  )
  set(JSON_BuildTests OFF CACHE INTERNAL "")
  FetchContent_MakeAvailable(json)

  set(SNAPPY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(SNAPPY_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
  set(SNAPPY_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    snappy
    GIT_REPOSITORY https://github.com/google/snappy.git
    GIT_TAG 1.2.2
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(snappy)
  set(CYKA_SNAPPY_TARGET snappy)
else()
  find_package(nlohmann_json 3.11 REQUIRED)
  if(CYKA_STATIC)
    find_library(SNAPPY_LIB NAMES libsnappy.a snappy REQUIRED)
  else()
    find_library(SNAPPY_LIB snappy REQUIRED)
  endif()
  set(CYKA_SNAPPY_TARGET ${SNAPPY_LIB})
endif()

find_package(Threads REQUIRED)

# meshoptimizer: simplify skinned POV player/weapon meshes (not commonly packaged).
set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  meshoptimizer
  GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
  GIT_TAG v0.23
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(meshoptimizer)

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
    src/demo/debug.cpp
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
    src/aim/visibility_batch.cpp
    src/aim/ttd.cpp
    src/aim/ttd_trace.cpp
    src/aim/gltf_player.cpp
    src/aim/spray.cpp
    src/aim/counter_strafe.cpp
    src/aim/spotted.cpp
    src/aim/shot_visible.cpp
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
target_include_directories(cyka_core PRIVATE ${CMAKE_SOURCE_DIR}/third_party/cgltf)
target_link_libraries(cyka_core PUBLIC
    nlohmann_json::nlohmann_json
    Threads::Threads
    ${CYKA_SNAPPY_TARGET}
)
# Keep third-party headers out of -Werror (e.g. nlohmann std::is_trivial under C++26).
get_target_property(_cyka_json_inc nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
if(_cyka_json_inc)
  target_include_directories(cyka_core SYSTEM PUBLIC ${_cyka_json_inc})
endif()
target_link_libraries(cyka_core PRIVATE meshoptimizer)

set(CYKA_SPRAY_SOURCES
    src/csdata/spray_base_rifles.cpp
    src/csdata/spray_base_smg.cpp
    src/csdata/spray_base_lmg.cpp
    src/csdata/spray_scoped.cpp
    src/csdata/spray_nosil.cpp
)
# Portable spray tables use generated/*.inc (g++-14 has no #embed).
target_include_directories(cyka_core PRIVATE ${CMAKE_SOURCE_DIR}/src/csdata)

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
    tests/test_corpus.cpp
    tests/test_ttd_trace.cpp
    tests/test_steam_id.cpp
    tests/test_ent_decode.cpp
)
target_link_libraries(cyka2pp-tests PRIVATE cyka_core)
target_compile_definitions(cyka2pp-tests PRIVATE
    CYKA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    CYKA_DEMO_DIR="${CMAKE_SOURCE_DIR}/../dotdem"
    CYKA_MAPS_DIR="${CMAKE_SOURCE_DIR}/../cs2-maps-tri"
)
add_test(NAME cyka2pp-tests COMMAND cyka2pp-tests)

# Warnings / march only on our targets (not FetchContent deps).
set(CYKA_WARNINGS -Wall -Wextra -Wpedantic -Werror)
foreach(tgt cyka_core cyka2pp cyka2pp-tests)
  target_compile_options(${tgt} PRIVATE ${CYKA_WARNINGS})
  if(CYKA_MARCH)
    target_compile_options(${tgt} PRIVATE -march=${CYKA_MARCH})
    # GCC -march=x86-64-v3/v4 enables AVX and miscompiles our Source 2 entity
    # bit reader (ghost steam IDs, zero eye angles, junk names). Keep BMI2/etc
    # from the march level but force scalar SSE codegen until that is fixed.
    if(CYKA_MARCH MATCHES "^x86-64-v[34]")
      target_compile_options(${tgt} PRIVATE -mno-avx)
    endif()
  endif()
endforeach()
if(CYKA_MARCH)
  message(STATUS "CYKA_MARCH=${CYKA_MARCH}")
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
find_program(RUN_CLANG_TIDY_EXE NAMES run-clang-tidy)
if(RUN_CLANG_TIDY_EXE)
  add_custom_target(cyka_tidy
    COMMAND ${RUN_CLANG_TIDY_EXE}
      -p ${CMAKE_BINARY_DIR}
      -warnings-as-errors=*
      "^(${CMAKE_SOURCE_DIR}/(include|src|tests)/)"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-tidy on first-party sources (warnings as errors)"
    VERBATIM
  )
elseif(CLANG_TIDY_EXE)
  add_custom_target(cyka_tidy
    COMMAND ${CLANG_TIDY_EXE}
      -p ${CMAKE_BINARY_DIR}
      -warnings-as-errors=*
      ${CYKA_LIB_SOURCES} src/main.cpp
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-tidy (warnings as errors)"
    VERBATIM
  )
endif()
