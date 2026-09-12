cmake_minimum_required(VERSION 3.16)
project(my_life_deviation VERSION 0.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MLD_SANITIZE "Enable address/undefined sanitizers" OFF)

add_library(mld
  engine/src/core/internal.cpp
  engine/src/core/hash.cpp
  engine/src/simulation/generation.cpp
  engine/src/core/storage.cpp
  engine/src/autonomy/controller.cpp
  engine/src/core/world.cpp
)
target_include_directories(mld PUBLIC engine/include)

if(MSVC)
  target_compile_options(mld PUBLIC /W4 /permissive- /fp:strict /utf-8)
else()
  target_compile_options(mld PUBLIC -Wall -Wextra -Wpedantic -Werror -ffp-contract=off)
  if(MLD_SANITIZE)
    target_compile_options(mld PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(mld PUBLIC -fsanitize=address,undefined)
  endif()
endif()

enable_testing()

add_executable(mld_unit engine/tests/unit.cpp)
target_link_libraries(mld_unit PRIVATE mld)
add_test(NAME numerical_contracts COMMAND mld_unit)

add_executable(mld_world_tests engine/tests/world_tests.cpp)
target_link_libraries(mld_world_tests PRIVATE mld)
add_test(NAME world_contracts COMMAND mld_world_tests)

add_executable(mld_sim apps/simulate/simulate.cpp)
target_link_libraries(mld_sim PRIVATE mld)
add_test(NAME cli_rejects_unknown COMMAND mld_sim --unknown)
set_tests_properties(cli_rejects_unknown PROPERTIES WILL_FAIL TRUE)
