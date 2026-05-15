cmake_minimum_required(VERSION 3.12)
project(Controller LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CONFIGURATION_TYPES "Release;Debug" CACHE STRING "Restricted build types" FORCE)

include(FetchContent)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_directory_properties(PROPERTIES VS_SOLUTION_ITEMS "CMakeLists.txt;LICENSE.md;README.md;.gitignore")
source_group("solution items" FILES CMakeLists.txt LICENSE.md README.md .gitignore)

#LOGIC TO INCLUDE SFML
set(SFML_ROOT_HINT "" CACHE PATH "Path to SFML installation")
if(SFML_ROOT_HINT)
    list(APPEND CMAKE_PREFIX_PATH "${SFML_ROOT_HINT}")
endif()

find_package(SFML 3 COMPONENTS System Window Graphics Network Main QUIET)
if (NOT SFML_FOUND)
    message(FATAL_ERROR "SFML library not found. Please set SFML_ROOT_HINT to the correct path.")
else()
    message(STATUS "Found SFML: ${SFML_VERSION} at ${SFML_DIR}")
endif()

#LOGIC TO INCLUDE SQLite3
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    FetchContent_Declare(
        sqlite3_source
        URL https://www.sqlite.org/2026/sqlite-amalgamation-3530100.zip
    )
    FetchContent_MakeAvailable(sqlite3_source)
    
    add_library(sqlite3 STATIC "${sqlite3_source_SOURCE_DIR}/sqlite3.c")
    target_include_directories(sqlite3 PUBLIC "${sqlite3_source_SOURCE_DIR}")
    
    find_package(Threads REQUIRED)
    target_link_libraries(sqlite3 PRIVATE Threads::Threads)
    
    if(UNIX AND NOT APPLE)
        target_link_libraries(sqlite3 PRIVATE ${CMAKE_DL_LIBS})
    endif()

    set_target_properties(sqlite3 PROPERTIES 
        FOLDER "external"
    )
    
    add_library(SQLite3::SQLite3 ALIAS sqlite3)
endif()

#LOGIC TO INCLUDE FTXUI
set(BUILD_SERVER_ONLY True CACHE BOOL "Whether to only build the Server component (without Attacker and Victim)")
if (NOT BUILD_SERVER_ONLY)
    FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG v6.1.9
    )
    FetchContent_MakeAvailable(ftxui)
    set_target_properties(
        component
        dom
        screen
        PROPERTIES FOLDER "external/ftxui"
    )
endif()

add_subdirectory(Server)
if (NOT BUILD_SERVER_ONLY)
    add_subdirectory(Attacker)
    add_subdirectory(Victim)
endif()