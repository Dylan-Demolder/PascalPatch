cmake_minimum_required(VERSION 3.20)
project(pascalpatch_match_stats CXX)

# The PascalPatch SDK: the same headers community plugins build against. First-party plugins take
# nothing else from this repository, so whatever they need from the game goes into the SDK first.
set(PASCALPATCH_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/include" CACHE PATH "folder containing pascalpatch/plugin.h")
if(NOT EXISTS "${PASCALPATCH_SDK}/pascalpatch/plugin.h")
  message(FATAL_ERROR "set PASCALPATCH_SDK to the folder containing pascalpatch/plugin.h")
endif()

add_library(match-stats SHARED match_stats.cpp)
set_target_properties(match-stats PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON PREFIX "")
target_include_directories(match-stats PRIVATE "${PASCALPATCH_SDK}")
if(MSVC)
  target_compile_options(match-stats PRIVATE /W4 /utf-8)
  set_property(TARGET match-stats PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
