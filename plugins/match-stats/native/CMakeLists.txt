cmake_minimum_required(VERSION 3.20)
project(pascalpatch_match_stats CXX)

# NLOHMANN_JSON_INCLUDE is accepted (build_plugins.py passes it to every plugin) but unused.
set(NLOHMANN_JSON_INCLUDE "" CACHE PATH "unused")

add_library(match-stats SHARED match_stats.cpp)
set_target_properties(match-stats PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON PREFIX "")
target_include_directories(match-stats PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/include")
if(MSVC)
  target_compile_options(match-stats PRIVATE /W4 /utf-8)
  set_property(TARGET match-stats PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
