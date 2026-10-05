cmake_minimum_required(VERSION 3.24)

project(Snapper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Test)

include(cmake/snapper.cmake)
include(CTest)

# Modules, lowest first. See ARCHITECTURE.md for what each one owns.
snapper_module(base
  SOURCES frame.cpp
  LIBS Qt6::Core)
snapper_module(model
  SOURCES doll.cpp layer.cpp project.cpp shot.cpp
  MODULES base
  LIBS Qt6::Gui)
snapper_module(anim
  SOURCES doll_pose.cpp ik.cpp master_timeline.cpp mirror.cpp presets.cpp
          sampler.cpp warp.cpp
  MODULES model)
snapper_module(edit
  SOURCES history_manager.cpp edit_scope.cpp
  MODULES model)
snapper_module(ui
  SOURCES main_window.cpp
  MODULES edit
  LIBS Qt6::Widgets)

qt_add_executable(snapper src/app/main.cpp src/app/app_context.cpp)
target_link_libraries(snapper PRIVATE snapper_ui)
snapper_warnings(snapper)
set_property(GLOBAL APPEND PROPERTY SNAPPER_LAYERS "app=ui")
set_target_properties(snapper PROPERTIES WIN32_EXECUTABLE TRUE)

if(BUILD_TESTING)
  snapper_test(base SOURCES base_tests.cpp)
  snapper_test(model SOURCES model_tests.cpp)
  snapper_test(anim SOURCES sampler_tests.cpp pose_tests.cpp)
  snapper_test(edit SOURCES history_tests.cpp)
  snapper_checks()
endif()
