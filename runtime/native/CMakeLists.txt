cmake_minimum_required(VERSION 3.20)
project(pascalpatch_runtime CXX RC)

# Folder holding nlohmann/json.hpp (melee-unlocked's port/third_party has it; build_plugins.py passes it).
set(NLOHMANN_JSON_INCLUDE "" CACHE PATH "folder containing nlohmann/json.hpp")
if(NOT EXISTS "${NLOHMANN_JSON_INCLUDE}/nlohmann/json.hpp")
  message(FATAL_ERROR "set NLOHMANN_JSON_INCLUDE to the folder containing nlohmann/json.hpp")
endif()

set(IMGUI "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/imgui")
set(IMGUI_SOURCES "${IMGUI}/imgui.cpp" "${IMGUI}/imgui_draw.cpp" "${IMGUI}/imgui_tables.cpp" "${IMGUI}/imgui_widgets.cpp"
    "${IMGUI}/backends/imgui_impl_win32.cpp" "${IMGUI}/backends/imgui_impl_dx12.cpp")

# pascalpatch_runtime.dll: injected into melee_port.exe; hosts native plugins, draws the F2 overlay
# and keeps the port offline.
add_library(pascalpatch_runtime SHARED runtime.cpp overlay.cpp ${IMGUI_SOURCES})
set_target_properties(pascalpatch_runtime PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON PREFIX "")
target_include_directories(pascalpatch_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../design/dist" "${IMGUI}" "${NLOHMANN_JSON_INCLUDE}")
target_link_libraries(pascalpatch_runtime PRIVATE ws2_32 shell32 d3d12 dxgi dxguid d3dcompiler)

# pascalpatch-launch.exe: starts the unmodified port suspended and injects the runtime.
add_executable(pascalpatch-launch launch.cpp)
set_target_properties(pascalpatch-launch PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

# PascalPatch.exe: the desktop app's tray icon and window (it runs the Python app server hidden).
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/pp_runtime.h" PP_VERSION_LINE REGEX "VERSION = \"[0-9]+\\.[0-9]+\\.[0-9]+\"")
string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ "${PP_VERSION_LINE}")
add_executable(pascalpatch-app WIN32 app.cpp app.rc)
set_target_properties(pascalpatch-app PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON OUTPUT_NAME PascalPatch)
target_include_directories(pascalpatch-app PRIVATE "${NLOHMANN_JSON_INCLUDE}")
target_compile_definitions(pascalpatch-app PRIVATE PP_VERSION_MAJOR=${CMAKE_MATCH_1} PP_VERSION_MINOR=${CMAKE_MATCH_2}
    PP_VERSION_PATCH=${CMAKE_MATCH_3})
target_link_libraries(pascalpatch-app PRIVATE winhttp shell32 user32 advapi32)

if(MSVC)
  foreach(t pascalpatch_runtime pascalpatch-launch pascalpatch-app)
    target_compile_options(${t} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /utf-8>)
    target_compile_definitions(${t} PRIVATE UNICODE _UNICODE NOMINMAX)
    set_property(TARGET ${t} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
  endforeach()
  set_source_files_properties(${IMGUI_SOURCES} PROPERTIES COMPILE_OPTIONS "/W1")   # vendored
endif()
