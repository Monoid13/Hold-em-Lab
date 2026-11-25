cmake_minimum_required(VERSION 3.16)
project(HoldemLab VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Link the C/C++ runtime statically on Windows so the .exe runs without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_definitions(HOLDEMLAB_VERSION="${PROJECT_VERSION}")

if(MSVC)
  add_compile_options(/utf-8)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

if(ANDROID)
  set(PLATFORM "Android" CACHE STRING "raylib platform" FORCE)
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
      -DICON=${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/icon.png
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_fonts.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/DejaVuSans.ttf
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/DejaVuSans-Bold.ttf
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/icon.png
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_fonts.cmake
    COMMENT "Embedding fonts and icon")

  file(GLOB HOLDEM_UI_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp)
  set(HOLDEM_PLATFORM_SOURCES)
  if(WIN32)
    list(APPEND HOLDEM_PLATFORM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/app.rc)
  elseif(APPLE)
    set(HOLDEM_ICNS ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/HoldemLab.icns)
    set_source_files_properties(${HOLDEM_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND HOLDEM_PLATFORM_SOURCES ${HOLDEM_ICNS})
  endif()
  if(ANDROID)
    # Android loads the app as libmain.so through NativeActivity (see android/).
    add_library(main SHARED src/main.cpp ${HOLDEM_UI_SOURCES} ${EMBED_OUT})
    target_include_directories(main PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)
    target_link_libraries(main PRIVATE holdem_core raylib android log EGL GLESv2 OpenSLES m)
    target_link_options(main PRIVATE "-u" "ANativeActivity_onCreate")
    return()
  endif()
  add_executable(HoldemLab src/main.cpp ${HOLDEM_UI_SOURCES} ${EMBED_OUT} ${HOLDEM_PLATFORM_SOURCES})
  target_link_libraries(HoldemLab PRIVATE holdem_core raylib)
  if(MSVC)
    set_target_properties(HoldemLab PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
  elseif(WIN32)
    target_link_options(HoldemLab PRIVATE -mwindows)
  endif()
  if(APPLE)
    target_link_libraries(HoldemLab PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
    set_target_properties(HoldemLab PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_BUNDLE_NAME "Hold'em Lab"
      MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.monoid13.holdemlab"
      MACOSX_BUNDLE_ICON_FILE HoldemLab.icns
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
      MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
  endif()
endif()
