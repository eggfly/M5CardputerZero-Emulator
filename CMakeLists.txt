cmake_minimum_required(VERSION 3.16)
project(cardputer-emu LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Paths
set(VENDOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/M5CardputerZero-UserDemo)
set(APPLAUNCH  ${VENDOR_DIR}/projects/APPLaunch/main)
set(USERDEMO   ${VENDOR_DIR}/projects/UserDemo/main)

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
# Freetype
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
# LVGL 9.5
# ---------------------------------------------------------------------------
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG OFF CACHE BOOL "" FORCE)
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

if(FREETYPE_INCLUDE_DIRS)
    include_directories(${FREETYPE_INCLUDE_DIRS})
endif()

# Build LVGL as static, apps export symbols via -rdynamic / POSITION_INDEPENDENT_CODE
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(lib/lvgl)

# ---------------------------------------------------------------------------
# Helper: common link libraries for app targets
# ---------------------------------------------------------------------------
macro(emu_link_deps target)
    target_link_libraries(${target} PRIVATE lvgl)
    if(TARGET SDL2::SDL2)
        target_link_libraries(${target} PRIVATE SDL2::SDL2)
    else()
        target_link_libraries(${target} PRIVATE ${SDL2_LIBRARIES})
    endif()
    if(FREETYPE_FOUND AND FREETYPE_LINK_LIBRARIES)
        target_link_libraries(${target} PRIVATE ${FREETYPE_LINK_LIBRARIES})
        target_link_directories(${target} PRIVATE ${FREETYPE_LIBRARY_DIRS})
    elseif(FREETYPE_LIBRARIES)
        target_link_libraries(${target} PRIVATE ${FREETYPE_LIBRARIES})
    else()
        target_link_directories(${target} PRIVATE /opt/homebrew/lib)
        target_link_libraries(${target} PRIVATE freetype)
    endif()
    find_package(Threads REQUIRED)
    target_link_libraries(${target} PRIVATE Threads::Threads)
endmacro()

# ---------------------------------------------------------------------------
# APP: APPLaunch.dylib
# ---------------------------------------------------------------------------
file(GLOB_RECURSE APPLAUNCH_UI_C   ${APPLAUNCH}/ui/*.c)
file(GLOB_RECURSE APPLAUNCH_UI_CPP ${APPLAUNCH}/ui/*.cpp)
file(GLOB         APPLAUNCH_KBD    ${APPLAUNCH}/src/keyboard_input.c)

add_library(APPLaunch SHARED
    ${APPLAUNCH_UI_C}
    ${APPLAUNCH_UI_CPP}
    ${APPLAUNCH_KBD}
)
target_include_directories(APPLaunch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${APPLAUNCH}/ui
    ${APPLAUNCH}/include
    ${APPLAUNCH}
)
target_compile_options(APPLaunch PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/src/emu_compat.h
    -Wno-c++11-narrowing
)
# App dylibs resolve LVGL symbols at load time from the emulator process
target_link_options(APPLaunch PRIVATE -undefined dynamic_lookup)

# ---------------------------------------------------------------------------
# APP: UserDemo.dylib
# ---------------------------------------------------------------------------
file(GLOB_RECURSE USERDEMO_UI_C ${USERDEMO}/ui/*.c)

add_library(UserDemo SHARED ${USERDEMO_UI_C})
target_include_directories(UserDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${USERDEMO}/ui
    ${USERDEMO}/include
    ${USERDEMO}
)
target_compile_options(UserDemo PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/src/emu_compat.h
)
target_link_options(UserDemo PRIVATE -undefined dynamic_lookup)

# ---------------------------------------------------------------------------
# Emulator executable (thin shell — loads app via dlopen)
# ---------------------------------------------------------------------------
add_executable(cardputer-emu src/main.cpp src/device_skin.cpp)
# Export all symbols so dlopen'd apps can resolve LVGL functions
if(APPLE)
    set_target_properties(cardputer-emu PROPERTIES ENABLE_EXPORTS ON)
endif()

target_include_directories(cardputer-emu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)
target_compile_options(cardputer-emu PRIVATE
    -Wno-c++11-narrowing
)

# Force-load entire LVGL so all symbols are available to dlopen'd apps
if(APPLE)
    target_link_libraries(cardputer-emu PRIVATE
        -Wl,-force_load,$<TARGET_FILE:lvgl>
    )
    # Also force-load sub-libraries if they exist
    target_link_libraries(cardputer-emu PRIVATE
        -Wl,-force_load,$<TARGET_FILE:lvgl_thorvg>
    )
else()
    target_link_libraries(cardputer-emu PRIVATE
        -Wl,--whole-archive lvgl lvgl_thorvg -Wl,--no-whole-archive
    )
endif()

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

# Freetype (needed by LVGL)
if(FREETYPE_FOUND AND FREETYPE_LINK_LIBRARIES)
    target_link_libraries(cardputer-emu PRIVATE ${FREETYPE_LINK_LIBRARIES})
    target_link_directories(cardputer-emu PRIVATE ${FREETYPE_LIBRARY_DIRS})
else()
    target_link_directories(cardputer-emu PRIVATE /opt/homebrew/lib)
    target_link_libraries(cardputer-emu PRIVATE freetype)
endif()

# dl (for dlopen)
target_link_libraries(cardputer-emu PRIVATE ${CMAKE_DL_LIBS})

find_package(Threads REQUIRED)
target_link_libraries(cardputer-emu PRIVATE Threads::Threads)

# ---------------------------------------------------------------------------
# Post-build: copy apps and resources into build/apps/
# ---------------------------------------------------------------------------
add_custom_command(TARGET cardputer-emu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cardputer-emu>/apps
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:APPLaunch>
        $<TARGET_FILE_DIR:cardputer-emu>/apps/
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:UserDemo>
        $<TARGET_FILE_DIR:cardputer-emu>/apps/
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cardputer-emu>/share/images
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${APPLAUNCH}/ui/images
        $<TARGET_FILE_DIR:cardputer-emu>/share/images
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cardputer-emu>/assets
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/device_skin.png
        $<TARGET_FILE_DIR:cardputer-emu>/assets/device_skin.png
    COMMENT "Copying apps and resources"
)
