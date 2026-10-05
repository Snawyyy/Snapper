cmake_minimum_required(VERSION 3.24)

project(Snapper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Test)

include(cmake/snapper.cmake)
include(CTest)

# Modules, lowest first. See ARCHITECTURE.md for what each one owns.
snapper_module(base
  SOURCES frame.cpp
  LIBS Qt6::Core)
if(BUILD_TESTING)
  snapper_test(base SOURCES base_tests.cpp)
  snapper_checks()
endif()
