cmake_minimum_required(VERSION 3.24)

project(Snapper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
  Concurrent Core Gui Multimedia Widgets Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
  libavcodec libavformat libavutil libswresample libswscale)

include(cmake/snapper.cmake)
include(CTest)

# Modules, lowest first. See ARCHITECTURE.md for what each one owns.
# Each builds every src/<module>/*.cpp; a new file needs no line here.
snapper_module(base
  LIBS Qt6::Core)
snapper_module(model
  MODULES base
  LIBS Qt6::Gui)
# F's Plugins: the real plugin code (third_party/fsp) under a stand-in
# for the After Effects host, behind src/fsp/fsp_effects.h.
add_subdirectory(third_party/fsp)
snapper_module(fsp
  MODULES base
  LIBS Qt6::Gui)
target_link_libraries(snapper_fsp PRIVATE fsp_vendor)
# Runs one plugin over a PNG, to look at a port without the app.
add_executable(fsp_render EXCLUDE_FROM_ALL tools/fsp_render.cpp)
target_link_libraries(fsp_render PRIVATE snapper_fsp)
snapper_warnings(fsp_render)
# anim and io read a plugin's parameters (a Comp effect's knobs and how
# they are saved).
snapper_module(anim
  MODULES model fsp)
snapper_module(io
  MODULES model fsp)
snapper_module(media
  MODULES base
  LIBS Qt6::Gui Qt6::Multimedia PkgConfig::FFMPEG)
snapper_module(render
  MODULES anim fsp)
# The poses, springs and per-pixel warp loops (anim and render) are far
# too slow unoptimised, and a debug build is what gets used while
# editing: they stay -O2 there (asserts stay on, so it still checks).
target_compile_options(snapper_anim PRIVATE "$<$<CONFIG:Debug>:-O2>")
target_compile_options(snapper_render PRIVATE "$<$<CONFIG:Debug>:-O2>")
snapper_module(edit
  MODULES io media render
  LIBS Qt6::Concurrent)
snapper_module(ui
  MODULES edit
  LIBS Qt6::Widgets)

qt_add_executable(snapper src/app/main.cpp src/app/app_context.cpp)
target_link_libraries(snapper PRIVATE snapper_ui)
snapper_warnings(snapper)
set_property(GLOBAL APPEND PROPERTY SNAPPER_LAYERS "app=ui")
target_include_directories(snapper PRIVATE src)
set_target_properties(snapper PROPERTIES WIN32_EXECUTABLE TRUE)

if(BUILD_TESTING)
  snapper_test(base)
  snapper_test(model)
  snapper_test(anim)
  snapper_test(io)
  snapper_test(media)
  snapper_test(render)
  snapper_test(edit)
  snapper_test(ui)
  snapper_checks()
endif()
