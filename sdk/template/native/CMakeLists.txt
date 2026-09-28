cmake_minimum_required(VERSION 3.20)
project(my_plugin CXX)

# Where the PascalPatch SDK headers are (the folder holding pascalpatch/plugin.h). Inside the
# PascalPatch repository the default works; for a copy elsewhere pass
#   -DPASCALPATCH_SDK=C:/path/to/PascalPatch/sdk/include
set(PASCALPATCH_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../include" CACHE PATH "folder containing pascalpatch/plugin.h")
if(NOT EXISTS "${PASCALPATCH_SDK}/pascalpatch/plugin.h")
  message(FATAL_ERROR "set PASCALPATCH_SDK to the folder containing pascalpatch/plugin.h")
endif()
# build_plugins.py passes this to every plugin; this one does not use nlohmann/json.
set(NLOHMANN_JSON_INCLUDE "" CACHE PATH "unused")

# The DLL's name must be <id>.dll, the id in plugin.json.
add_library(my-plugin SHARED my_plugin.cpp)
set_target_properties(my-plugin PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON PREFIX "")
target_include_directories(my-plugin PRIVATE "${PASCALPATCH_SDK}")
if(MSVC)
  target_compile_options(my-plugin PRIVATE /W4 /utf-8)
  # static runtime: the DLL needs nothing installed beside it
  set_property(TARGET my-plugin PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
