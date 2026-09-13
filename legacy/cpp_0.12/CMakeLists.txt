cmake_minimum_required(VERSION 3.20)
project(my_life_deviation VERSION 0.12.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(LIFE_SANITIZERS "Address and undefined behavior sanitizers" OFF)
add_library(life_core engine/src/life/career.cpp engine/src/life/career_world.cpp engine/src/life/deliberation.cpp engine/src/life/self_model.cpp engine/src/life/experience.cpp engine/src/life/cognitive_self_world.cpp engine/src/life/numeric.cpp engine/src/life/seed.cpp engine/src/life/mind.cpp engine/src/life/world.cpp engine/src/life/executor.cpp engine/src/life/generation.cpp engine/src/life/validation.cpp engine/src/life/persistence.cpp engine/src/life/cognition.cpp engine/src/life/cognitive_world.cpp engine/src/life/worker.cpp engine/src/life/social.cpp engine/src/life/social_world.cpp engine/src/life/social_diagnostics.cpp engine/src/life/community.cpp engine/src/life/community_world.cpp engine/src/life/projects.cpp engine/src/life/activity_resources.cpp engine/src/life/employment.cpp engine/src/life/project_world.cpp engine/src/life/parallel_world.cpp engine/src/life/employment_world.cpp)
target_include_directories(life_core PUBLIC engine/include)
if(MSVC)
 target_compile_options(life_core PRIVATE /W4 /fp:strict /utf-8)
else()
 target_compile_options(life_core PRIVATE -Wall -Wextra -Wpedantic -Werror -ffp-contract=off)
 if(LIFE_SANITIZERS)
  target_compile_options(life_core PUBLIC -O1 -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(life_core PUBLIC -fsanitize=address,undefined)
 endif()
endif()
enable_testing()
add_subdirectory(reference/cognition_0.4 cog_reference)
find_package(Threads REQUIRED)
target_include_directories(life_core PUBLIC reference/cognition_0.4 "${CMAKE_CURRENT_BINARY_DIR}/cog_reference")
target_link_libraries(life_core PUBLIC Threads::Threads)
add_executable(life_tests engine/tests/life/main.cpp engine/tests/life/test_numeric.cpp engine/tests/life/test_seed.cpp engine/tests/life/test_mind.cpp engine/tests/life/test_world.cpp engine/tests/life/test_cognition.cpp engine/tests/life/test_social.cpp engine/tests/life/test_social_unit.cpp engine/tests/life/test_community.cpp engine/tests/life/test_life11.cpp engine/tests/life/test_projects.cpp engine/tests/life/test_life_contracts.cpp engine/tests/life/test_self_model.cpp engine/tests/life/test_self_runtime.cpp engine/tests/life/test_adaptive.cpp engine/tests/life/test_career.cpp engine/tests/life/test_career_runtime.cpp)
target_link_libraries(life_tests PRIVATE life_core)
add_test(NAME numeric COMMAND life_tests numeric)

add_test(NAME seed COMMAND life_tests seed)
add_test(NAME mind COMMAND life_tests mind)
add_test(NAME world COMMAND life_tests world)
add_executable(life_sim apps/simulate/life_main.cpp)
target_link_libraries(life_sim PRIVATE life_core)
add_test(NAME cli_invalid_argument COMMAND life_sim --population 0 --seconds 1)
set_tests_properties(cli_invalid_argument PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_smoke COMMAND life_sim --population 8 --seconds 600)

add_test(NAME cognition_world COMMAND life_tests cognition)
add_executable(cognitive_scenarios tools/cognitive_scenarios.cpp)
target_link_libraries(cognitive_scenarios PRIVATE life_core)

add_test(NAME social_world COMMAND life_tests social)

add_test(NAME social_contracts COMMAND life_tests social_unit)

add_test(NAME community COMMAND life_tests community)

add_test(NAME community_unit COMMAND life_tests community_unit)

add_test(NAME community_long COMMAND life_tests community_long)

add_executable(community_scenarios tools/community_scenarios.cpp)
target_link_libraries(community_scenarios PRIVATE life_core)

add_test(NAME life11 COMMAND life_tests life11)

add_test(NAME projects COMMAND life_tests projects)

add_test(NAME life_contracts COMMAND life_tests life_contracts)

add_test(NAME life11_long COMMAND life_tests life11_long)
set_tests_properties(life11_long community_long PROPERTIES LABELS "long")

add_test(NAME self_model COMMAND life_tests self_model)

add_test(NAME self_runtime COMMAND life_tests self_runtime)

add_test(NAME adaptive COMMAND life_tests adaptive)
add_executable(self_behavior_scenario tools/self_behavior_scenario.cpp)
target_link_libraries(self_behavior_scenario PRIVATE life_core)
add_test(NAME self_behavior COMMAND self_behavior_scenario)
add_executable(diagnose_actor tools/diagnose_actor.cpp)
target_link_libraries(diagnose_actor PRIVATE life_core)

add_test(NAME career COMMAND life_tests career)

add_test(NAME career_runtime COMMAND life_tests career_runtime)
