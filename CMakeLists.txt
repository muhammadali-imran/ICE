cmake_minimum_required(VERSION 3.20)
project(ice LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Debug builds: sanitizers. Release builds: optimizations.
# Turn the sanitizers off with -DICE_SANITIZERS=OFF, for example for a Clang build on a
# machine without Clang's sanitizer runtime package.
option(ICE_SANITIZERS "Enable AddressSanitizer and UBSan in Debug builds" ON)
if(ICE_SANITIZERS)
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fsanitize=address,undefined")
else()
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")   # add -march=native for local builds only

# Default to a Release build when no build type is given.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(ice_core
  src/attacks.cpp
  src/board.cpp
  src/movegen.cpp
  src/perft.cpp)
target_include_directories(ice_core PUBLIC src)
target_compile_options(ice_core PUBLIC -Wall -Wextra -Wpedantic -Wshadow -Wconversion)

add_executable(ice src/main.cpp)
target_link_libraries(ice PRIVATE ice_core)

include(CTest)
if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.1)
  FetchContent_MakeAvailable(Catch2)

  add_executable(ice_tests
    tests/attacks_test.cpp
    tests/board_test.cpp
    tests/move_test.cpp
    tests/perft_test.cpp)
  target_link_libraries(ice_tests PRIVATE ice_core Catch2::Catch2WithMain)

  # Fast tests: everything not hidden. Runs in every build type.
  add_test(NAME ice_tests COMMAND ice_tests)
  # Deeper perft (depth 5, tag [slow]) takes minutes under sanitizers, so it is only
  # registered for optimized builds. Run it by hand anywhere with: ice_tests "[slow]"
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME ice_perft_slow COMMAND ice_tests "[slow]")
  endif()
endif()
