cmake_minimum_required(VERSION 3.16)
project(HoldemLab VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  add_compile_options(/utf-8)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

option(HOLDEM_BUILD_GUI "Build the graphical application (fetches raylib if not installed)" ON)
option(HOLDEM_BUILD_TOOLS "Build tests and the bot simulator" ON)

# ---- engine library (no graphics dependency) ----
file(GLOB_RECURSE HOLDEM_CORE_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/bots/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/coach/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/train/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.cpp)
add_library(holdem_core STATIC ${HOLDEM_CORE_SOURCES})
target_include_directories(holdem_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
  target_compile_options(holdem_core PRIVATE /W3)
else()
  target_compile_options(holdem_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(HOLDEM_BUILD_TOOLS)
  enable_testing()
  add_executable(holdem_tests tests/tests.cpp)
  target_link_libraries(holdem_tests PRIVATE holdem_core)
  add_test(NAME holdem_tests COMMAND holdem_tests)

  add_executable(holdem_sim tools/simulate.cpp)
  target_link_libraries(holdem_sim PRIVATE holdem_core)
endif()

# ---- graphical application ----
if(HOLDEM_BUILD_GUI)
  find_package(raylib 5.0 QUIET)
  if(NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
      GIT_REPOSITORY https://github.com/raysan5/raylib.git
      GIT_TAG 5.5
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(raylib)
  endif()

  # Embed the fonts so the app is a single self-contained executable.
  set(EMBED_OUT ${CMAKE_CURRENT_BINARY_DIR}/embedded_fonts.c)
  add_custom_command(
    OUTPUT ${EMBED_OUT}
    COMMAND ${CMAKE_COMMAND}
      -DOUT=${EMBED_OUT}
      -DREGULAR=${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/DejaVuSans.ttf
      -DBOLD=${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/DejaVuSans-Bold.ttf
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_fonts.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/DejaVuSans.ttf
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/DejaVuSans-Bold.ttf
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_fonts.cmake
    COMMENT "Embedding fonts")

  file(GLOB HOLDEM_UI_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp)
  add_executable(HoldemLab src/main.cpp ${HOLDEM_UI_SOURCES} ${EMBED_OUT})
  target_link_libraries(HoldemLab PRIVATE holdem_core raylib)
  if(MSVC)
    set_target_properties(HoldemLab PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
  elseif(WIN32)
    target_link_options(HoldemLab PRIVATE -mwindows)
  endif()
  if(APPLE)
    target_link_libraries(HoldemLab PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
  endif()
endif()
