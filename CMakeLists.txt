cmake_minimum_required(VERSION 3.16)
project(cardputer-emu LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Paths
set(VENDOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/M5CardputerZero-UserDemo)
set(APPLAUNCH  ${VENDOR_DIR}/projects/APPLaunch/main)

# ---------------------------------------------------------------------------
# SDL2
# ---------------------------------------------------------------------------
if(APPLE)
    if(EXISTS "/opt/homebrew/opt/sdl2")
        list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/sdl2")
    elseif(EXISTS "/usr/local/opt/sdl2")
        list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/sdl2")
    endif()
endif()

find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
endif()

# ---------------------------------------------------------------------------
# Freetype (for LVGL freetype font support)
# ---------------------------------------------------------------------------
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FREETYPE freetype2)
endif()
if(NOT FREETYPE_FOUND)
    find_package(Freetype QUIET)
    if(Freetype_FOUND)
        set(FREETYPE_INCLUDE_DIRS ${FREETYPE_INCLUDE_DIRS})
        set(FREETYPE_LIBRARIES Freetype::Freetype)
    endif()
endif()

# ---------------------------------------------------------------------------
# LVGL 9.5 (git submodule at lib/lvgl/)
# ---------------------------------------------------------------------------
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_BUILD_CONF_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "" FORCE)
add_definitions(-DLV_CONF_INCLUDE_SIMPLE)

# SDL2 parent include for <SDL2/SDL.h>
if(SDL2_FOUND AND TARGET SDL2::SDL2)
    get_target_property(_sdl2_inc SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
    if(_sdl2_inc)
        foreach(_dir ${_sdl2_inc})
            get_filename_component(_parent "${_dir}" DIRECTORY)
            include_directories("${_parent}")
        endforeach()
    endif()
else()
    foreach(_dir ${SDL2_INCLUDE_DIRS})
        get_filename_component(_parent "${_dir}" DIRECTORY)
        include_directories("${_parent}")
    endforeach()
endif()

# Freetype includes (for LVGL)
if(FREETYPE_INCLUDE_DIRS)
    include_directories(${FREETYPE_INCLUDE_DIRS})
endif()

add_subdirectory(lib/lvgl)

# ---------------------------------------------------------------------------
# APPLaunch sources (excluding its main.cpp — we provide our own)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE APPLAUNCH_UI_C   ${APPLAUNCH}/ui/*.c)
file(GLOB_RECURSE APPLAUNCH_UI_CPP ${APPLAUNCH}/ui/*.cpp)
file(GLOB         APPLAUNCH_KBD    ${APPLAUNCH}/src/keyboard_input.c)

# ---------------------------------------------------------------------------
# Emulator sources
# ---------------------------------------------------------------------------
set(EMU_SOURCES
    src/main.cpp
    src/device_skin.cpp
    ${APPLAUNCH_UI_C}
    ${APPLAUNCH_UI_CPP}
    ${APPLAUNCH_KBD}
)

add_executable(cardputer-emu ${EMU_SOURCES})

target_include_directories(cardputer-emu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}           # lv_conf.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib       # lvgl/lvgl.h
    ${APPLAUNCH}/ui                       # ui/ui.h
    ${APPLAUNCH}/include                  # keyboard_input.h, compat/
    ${APPLAUNCH}                          # for includes like "ui/ui.h"
)

# Force-include compat header + suppress narrowing errors in vendor code
target_compile_options(cardputer-emu PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/src/emu_compat.h
    -Wno-c++11-narrowing
)

target_link_libraries(cardputer-emu PRIVATE lvgl)

# SDL2
if(TARGET SDL2::SDL2)
    target_link_libraries(cardputer-emu PRIVATE SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    target_link_libraries(cardputer-emu PRIVATE SDL2::SDL2-static)
else()
    target_include_directories(cardputer-emu PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(cardputer-emu PRIVATE ${SDL2_LIBRARIES})
endif()
if(APPLE AND TARGET SDL2::SDL2main)
    target_link_libraries(cardputer-emu PRIVATE SDL2::SDL2main)
endif()

# SDL2_image
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2_IMAGE SDL2_image)
endif()
if(SDL2_IMAGE_FOUND)
    target_include_directories(cardputer-emu PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_link_libraries(cardputer-emu PRIVATE ${SDL2_IMAGE_LINK_LIBRARIES})
    target_link_directories(cardputer-emu PRIVATE ${SDL2_IMAGE_LIBRARY_DIRS})
else()
    target_link_directories(cardputer-emu PRIVATE /opt/homebrew/lib)
    target_link_libraries(cardputer-emu PRIVATE SDL2_image)
endif()

# Freetype
if(FREETYPE_FOUND AND FREETYPE_LINK_LIBRARIES)
    target_link_libraries(cardputer-emu PRIVATE ${FREETYPE_LINK_LIBRARIES})
    target_link_directories(cardputer-emu PRIVATE ${FREETYPE_LIBRARY_DIRS})
elseif(FREETYPE_LIBRARIES)
    target_link_libraries(cardputer-emu PRIVATE ${FREETYPE_LIBRARIES})
else()
    # Fallback for macOS Homebrew
    target_link_directories(cardputer-emu PRIVATE /opt/homebrew/lib)
    target_link_libraries(cardputer-emu PRIVATE freetype)
endif()

# Threads
find_package(Threads REQUIRED)
target_link_libraries(cardputer-emu PRIVATE Threads::Threads)

# Copy resources to build output
add_custom_command(TARGET cardputer-emu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cardputer-emu>/share/images
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${APPLAUNCH}/ui/images
        $<TARGET_FILE_DIR:cardputer-emu>/share/images
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cardputer-emu>/assets
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/device_skin.png
        $<TARGET_FILE_DIR:cardputer-emu>/assets/device_skin.png
    COMMENT "Copying resources"
)
