cmake_minimum_required(VERSION 3.20)
project(pascalpatch_unlock_all CXX)

# NLOHMANN_JSON_INCLUDE is accepted (build_plugins.py passes it to every plugin) but unused.
set(NLOHMANN_JSON_INCLUDE "" CACHE PATH "unused")

add_library(unlock-all SHARED unlock_all.cpp)
set_target_properties(unlock-all PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON PREFIX "")
target_include_directories(unlock-all PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/include")
if(MSVC)
  target_compile_options(unlock-all PRIVATE /W4 /utf-8)
  set_property(TARGET unlock-all PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
