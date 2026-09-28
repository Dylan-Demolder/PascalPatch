cmake_minimum_required(VERSION 3.20)
project(pascalpatch_hitbox_viewer CXX)

# NLOHMANN_JSON_INCLUDE is accepted (build_plugins.py passes it to every plugin) but unused.
set(NLOHMANN_JSON_INCLUDE "" CACHE PATH "unused")

add_library(hitbox-viewer SHARED hitbox_viewer.cpp)
set_target_properties(hitbox-viewer PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON PREFIX "")
target_include_directories(hitbox-viewer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/include")
if(MSVC)
  target_compile_options(hitbox-viewer PRIVATE /W4 /utf-8)
  set_property(TARGET hitbox-viewer PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
