cmake_minimum_required(VERSION 3.20)
project(pascalpatch_quick_match CXX)

# The PascalPatch SDK: the same headers community plugins build against. First-party plugins take
# nothing else from this repository, so whatever they need from the game goes into the SDK first.
set(PASCALPATCH_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/include" CACHE PATH "folder containing pascalpatch/plugin.h")
if(NOT EXISTS "${PASCALPATCH_SDK}/pascalpatch/plugin.h")
  message(FATAL_ERROR "set PASCALPATCH_SDK to the folder containing pascalpatch/plugin.h")
endif()

# nlohmann/json (MIT): point NLOHMANN_JSON_INCLUDE at the folder holding nlohmann/json.hpp.
# PascalPatch ships one under runtime/third_party.
set(NLOHMANN_JSON_INCLUDE "" CACHE PATH "folder containing nlohmann/json.hpp")
if(NOT EXISTS "${NLOHMANN_JSON_INCLUDE}/nlohmann/json.hpp")
  message(FATAL_ERROR "set NLOHMANN_JSON_INCLUDE to a folder containing nlohmann/json.hpp")
endif()

add_library(quick-match SHARED quick_match.cpp)
set_target_properties(quick-match PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON PREFIX "")
target_include_directories(quick-match PRIVATE "${PASCALPATCH_SDK}" "${NLOHMANN_JSON_INCLUDE}")
if(MSVC)
  target_compile_options(quick-match PRIVATE /W4 /utf-8)
  set_property(TARGET quick-match PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
