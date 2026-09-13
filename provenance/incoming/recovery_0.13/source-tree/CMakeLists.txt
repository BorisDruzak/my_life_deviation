cmake_minimum_required(VERSION 3.20)
project(my_life_deviation VERSION 0.13.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(LIFE_SANITIZERS "Address and undefined behavior sanitizers" OFF)
add_library(life_core src/life/resources.cpp src/life/civil.cpp src/life/civil_world.cpp src/life/civil_social_world.cpp src/life/phone_world.cpp src/life/career.cpp src/life/career_world.cpp src/life/deliberation.cpp src/life/self_model.cpp src/life/experience.cpp src/life/cognitive_self_world.cpp src/life/numeric.cpp src/life/seed.cpp src/life/mind.cpp src/life/world.cpp src/life/executor.cpp src/life/generation.cpp src/life/validation.cpp src/life/persistence.cpp src/life/cognition.cpp src/life/cognitive_world.cpp src/life/worker.cpp src/life/social.cpp src/life/social_world.cpp src/life/social_diagnostics.cpp src/life/community.cpp src/life/community_world.cpp src/life/projects.cpp src/life/activity_resources.cpp src/life/employment.cpp src/life/project_world.cpp src/life/parallel_world.cpp src/life/employment_world.cpp)
target_include_directories(life_core PUBLIC include)
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
add_subdirectory("Документация/Проверки/cognition_0_4" cog_reference)
find_package(Threads REQUIRED)
target_include_directories(life_core PUBLIC "Документация/Проверки/cognition_0_4" "${CMAKE_CURRENT_BINARY_DIR}/cog_reference")
target_link_libraries(life_core PUBLIC Threads::Threads)
add_executable(life_tests tests/main.cpp tests/test_numeric.cpp tests/test_seed.cpp tests/test_mind.cpp tests/test_world.cpp tests/test_cognition.cpp tests/test_social.cpp tests/test_social_unit.cpp tests/test_community.cpp tests/test_life11.cpp tests/test_projects.cpp tests/test_life_contracts.cpp tests/test_self_model.cpp tests/test_self_runtime.cpp tests/test_adaptive.cpp tests/test_career.cpp tests/test_career_runtime.cpp tests/test_resources.cpp tests/test_civil_unit.cpp tests/test_civil_world.cpp tests/test_phone.cpp tests/test_civil_social.cpp)
target_link_libraries(life_tests PRIVATE life_core)
add_test(NAME numeric COMMAND life_tests numeric)

add_test(NAME seed COMMAND life_tests seed)
add_test(NAME mind COMMAND life_tests mind)
add_test(NAME world COMMAND life_tests world)
add_executable(life_sim app/main.cpp)
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

foreach(group resources civil_unit civil_world phone civil_social)
 add_test(NAME ${group} COMMAND life_tests ${group})
endforeach()
