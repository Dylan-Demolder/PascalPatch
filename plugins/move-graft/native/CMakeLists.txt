cmake_minimum_required(VERSION 3.20)
project(pascalpatch_move_graft CXX)

# nlohmann/json (MIT): point NLOHMANN_JSON_INCLUDE at the folder holding nlohmann/json.hpp.
# melee-unlocked ships one under port/third_party.
set(NLOHMANN_JSON_INCLUDE "" CACHE PATH "folder containing nlohmann/json.hpp")
if(NOT EXISTS "${NLOHMANN_JSON_INCLUDE}/nlohmann/json.hpp")
  message(FATAL_ERROR "set NLOHMANN_JSON_INCLUDE to a folder containing nlohmann/json.hpp")
endif()

add_library(move-graft SHARED move_graft.cpp)
set_target_properties(move-graft PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON PREFIX "")
target_include_directories(move-graft PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/include" "${NLOHMANN_JSON_INCLUDE}")
if(MSVC)
  target_compile_options(move-graft PRIVATE /W4 /utf-8)
  set_property(TARGET move-graft PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
