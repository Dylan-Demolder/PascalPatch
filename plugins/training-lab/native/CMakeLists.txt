cmake_minimum_required(VERSION 3.20)
project(pascalpatch_training_lab CXX)

# NLOHMANN_JSON_INCLUDE is accepted (build_plugins.py passes it to every plugin) but unused.
set(NLOHMANN_JSON_INCLUDE "" CACHE PATH "unused")

add_library(training-lab SHARED training_lab.cpp)
set_target_properties(training-lab PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON PREFIX "")
target_include_directories(training-lab PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/include")
if(MSVC)
  target_compile_options(training-lab PRIVATE /W4 /utf-8)
  set_property(TARGET training-lab PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
