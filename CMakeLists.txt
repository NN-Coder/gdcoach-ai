cmake_minimum_required(VERSION 3.21)

# Set the project metadata
project(GDCoachAI VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Find Geode SDK ────────────────────────────────────────────────────────────
if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK in your environment.")
else()
    message(STATUS "Found Geode SDK at: $ENV{GEODE_SDK}")
endif()

# Include the Geode SDK — this defines setup_geode_mod()
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# ── Define the Mod Target ─────────────────────────────────────────────────────
# add_library creates the shared DLL that Geode loads
add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/TelemetryManager.cpp
    src/CoachLayer.cpp
)

# Disable precompiled headers for our mod target — the complex arc/async
# template instantiations from Geode 5.x can trigger MSVC internal compiler
# errors (ICE) when used through a PCH. Build time will be a bit longer
# but compilation will be stable.
set_target_properties(${PROJECT_NAME} PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)

# setup_geode_mod links geode-sdk, validates mod.json, and creates the
# packaging target that produces the .geode file.
setup_geode_mod(${PROJECT_NAME})