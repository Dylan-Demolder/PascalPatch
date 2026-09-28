cmake_minimum_required(VERSION 3.20)
project(pascalpatch_combo_counter CXX)

# NLOHMANN_JSON_INCLUDE is accepted (build_plugins.py passes it to every plugin) but unused.
set(NLOHMANN_JSON_INCLUDE "" CACHE PATH "unused")

add_library(combo-counter SHARED combo_counter.cpp)
set_target_properties(combo-counter PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON PREFIX "")
target_include_directories(combo-counter PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/include")
if(MSVC)
  target_compile_options(combo-counter PRIVATE /W4 /utf-8)
  set_property(TARGET combo-counter PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
