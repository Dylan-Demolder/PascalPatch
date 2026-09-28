cmake_minimum_required(VERSION 3.20)
project(pascalpatch_frame_data CXX)

# The PascalPatch SDK: the same headers community plugins build against. First-party plugins take
# nothing else from this repository, so whatever they need from the game goes into the SDK first.
set(PASCALPATCH_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/include" CACHE PATH "folder containing pascalpatch/plugin.h")
if(NOT EXISTS "${PASCALPATCH_SDK}/pascalpatch/plugin.h")
  message(FATAL_ERROR "set PASCALPATCH_SDK to the folder containing pascalpatch/plugin.h")
endif()

add_library(frame-data SHARED frame_data.cpp)
set_target_properties(frame-data PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON PREFIX "")
target_include_directories(frame-data PRIVATE "${PASCALPATCH_SDK}")
if(MSVC)
  target_compile_options(frame-data PRIVATE /W4 /utf-8)
  set_property(TARGET frame-data PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
