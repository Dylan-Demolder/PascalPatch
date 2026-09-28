cmake_minimum_required(VERSION 3.20)
project(pascalpatch_info_display CXX)

# NLOHMANN_JSON_INCLUDE is accepted (build_plugins.py passes it to every plugin) but unused.
set(NLOHMANN_JSON_INCLUDE "" CACHE PATH "unused")

add_library(info-display SHARED info_display.cpp)
set_target_properties(info-display PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON PREFIX "")
target_include_directories(info-display PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/include")
if(MSVC)
  target_compile_options(info-display PRIVATE /W4 /utf-8)
  set_property(TARGET info-display PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
