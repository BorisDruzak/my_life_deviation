cmake_minimum_required(VERSION 3.20)
project(npc_world VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NPC_SANITIZERS "Enable ASan and UBSan" OFF)
option(NPC_COVERAGE "GCC coverage instrumentation" OFF)
option(NPC_STRICT "Treat compiler warnings as errors" ON)

add_library(npc_world STATIC
  engine/src/core/config.cpp
  engine/src/core/json.cpp
  engine/src/core/math.cpp
  engine/src/core/persistence.cpp
  engine/src/core/validation.cpp
  engine/src/core/world.cpp
  engine/src/simulation/dynamics.cpp
  engine/src/simulation/economy.cpp
  engine/src/simulation/physical.cpp
  engine/src/simulation/spatial.cpp
  engine/src/actions/action_start.cpp
  engine/src/actions/action_commit.cpp
  engine/src/social/social.cpp
  engine/src/knowledge/knowledge.cpp
  engine/src/autonomy/controller.cpp
  engine/src/autonomy/local_view.cpp
  engine/src/autonomy/policy/types.cpp
  engine/src/autonomy/policy/knowledge_query.cpp
  engine/src/autonomy/policy/validation.cpp
  engine/src/autonomy/policy/commitments.cpp
)
target_include_directories(npc_world PUBLIC engine/include)
target_include_directories(npc_world SYSTEM PUBLIC third_party)
target_compile_definitions(npc_world PUBLIC BOOST_ALL_NO_LIB)

if(MSVC)
  target_compile_options(npc_world PUBLIC /W4 /utf-8 /permissive- /fp:precise)
else()
  target_compile_options(npc_world PUBLIC -Wall -Wextra -Wpedantic -ffp-contract=off)
  if(NPC_STRICT)
    target_compile_options(npc_world PUBLIC -Werror)
  endif()
  if(NPC_SANITIZERS)
    target_compile_options(npc_world PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(npc_world PUBLIC -fsanitize=address,undefined)
  endif()
  if(NPC_COVERAGE)
    target_compile_options(npc_world PUBLIC --coverage -O0 -g)
    target_link_options(npc_world PUBLIC --coverage)
  endif()
endif()

add_executable(world_sim apps/world_sim/main.cpp)
target_link_libraries(world_sim PRIVATE npc_world)

add_executable(world_api_example apps/world_sim/api_example.cpp)
target_link_libraries(world_api_example PRIVATE npc_world)

include(GNUInstallDirs)
install(TARGETS world_sim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY game/rules/ DESTINATION ${CMAKE_INSTALL_DATADIR}/npc_world)

include(CTest)
if(BUILD_TESTING)
  add_executable(npc_tests
    engine/tests/main.cpp
    engine/tests/math_tests.cpp
    engine/tests/model_tests.cpp
    engine/tests/world_tests.cpp
    engine/tests/social_tests.cpp
    engine/tests/regression_tests.cpp
    engine/tests/controller_tests.cpp
    engine/tests/policy_goals_tests.cpp
    engine/tests/policy_index_tests.cpp
  )
  target_link_libraries(npc_tests PRIVATE npc_world)
  target_include_directories(npc_tests PRIVATE engine/tests)
  target_compile_definitions(npc_tests PRIVATE NPC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME core COMMAND npc_tests)

  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME cli COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/engine/tests/cli_test.py $<TARGET_FILE:world_sim> ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME autonomy_t0 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/engine/tests/autonomy_cli_test.py $<TARGET_FILE:world_sim> ${CMAKE_CURRENT_SOURCE_DIR})
  endif()
endif()
