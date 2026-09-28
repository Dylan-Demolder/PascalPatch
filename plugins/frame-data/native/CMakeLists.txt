cmake_minimum_required(VERSION 3.20)
project(pascalpatch_frame_data CXX)

# NLOHMANN_JSON_INCLUDE is accepted (build_plugins.py passes it to every plugin) but unused.
set(NLOHMANN_JSON_INCLUDE "" CACHE PATH "unused")

add_library(frame-data SHARED frame_data.cpp)
set_target_properties(frame-data PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON PREFIX "")
target_include_directories(frame-data PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/include")
if(MSVC)
  target_compile_options(frame-data PRIVATE /W4 /utf-8)
  set_property(TARGET frame-data PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
