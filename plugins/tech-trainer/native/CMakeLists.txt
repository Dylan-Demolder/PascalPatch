cmake_minimum_required(VERSION 3.20)
project(pascalpatch_tech_trainer CXX)

# NLOHMANN_JSON_INCLUDE is accepted (build_plugins.py passes it to every plugin) but unused.
set(NLOHMANN_JSON_INCLUDE "" CACHE PATH "unused")

add_library(tech-trainer SHARED tech_trainer.cpp)
set_target_properties(tech-trainer PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON PREFIX "")
target_include_directories(tech-trainer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/include")
if(MSVC)
  target_compile_options(tech-trainer PRIVATE /W4 /utf-8)
  set_property(TARGET tech-trainer PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
