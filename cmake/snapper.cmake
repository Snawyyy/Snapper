# Shared target setup, so every module and test is built the same way.

# Every change relinks ~100 test programs, so cache objects (ccache)
# and link with mold when they are installed.
find_program(SNAPPER_CCACHE ccache)
if(SNAPPER_CCACHE)
  set(CMAKE_CXX_COMPILER_LAUNCHER ${SNAPPER_CCACHE})
endif()
if(NOT DEFINED CMAKE_LINKER_TYPE)
  # Not gold: it warns on every object GCC 16 writes.
  find_program(SNAPPER_MOLD mold)
  if(SNAPPER_MOLD)
    set(CMAKE_LINKER_TYPE MOLD)
  endif()
endif()

# The presets build outside the checkout (the source drive is slow);
# a link named like the build folder keeps paths like build/snapper.
cmake_path(GET PROJECT_BINARY_DIR FILENAME snapper_build_name)
cmake_path(IS_PREFIX PROJECT_SOURCE_DIR "${PROJECT_BINARY_DIR}"
  snapper_build_inside)
set(snapper_build_link ${PROJECT_SOURCE_DIR}/${snapper_build_name})
if(NOT snapper_build_inside AND NOT EXISTS ${snapper_build_link}
    AND NOT IS_SYMLINK ${snapper_build_link})
  file(CREATE_LINK ${PROJECT_BINARY_DIR} ${snapper_build_link} SYMBOLIC)
endif()

function(snapper_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
      -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Werror)
    # GCC's optimizer reports std::variant copies holding a QString as
    # "maybe uninitialized" (GCC bug 80635), a known false positive.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${target} PRIVATE -Wno-maybe-uninitialized)
    endif()
  endif()
endfunction()

# snapper_module(<name> MODULES <lower modules> LIBS <libs>)
# Builds every src/<name>/*.cpp as snapper_<name> and records which
# modules it may include, for the layer check. Globbed so adding a
# file never means editing (or reading) CMakeLists.txt.
function(snapper_module name)
  cmake_parse_arguments(ARG "" "" "MODULES;LIBS" ${ARGN})
  set(target snapper_${name})
  set(module_targets)
  foreach(module IN LISTS ARG_MODULES)
    list(APPEND module_targets snapper_${module})
  endforeach()
  file(GLOB sources CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/${name}/*.cpp)
  add_library(${target} STATIC ${sources})
  target_include_directories(${target} PUBLIC ${PROJECT_SOURCE_DIR}/src)
  target_link_libraries(${target} PUBLIC ${module_targets} ${ARG_LIBS})
  snapper_warnings(${target})
  string(JOIN "," joined ${ARG_MODULES})
  set_property(GLOBAL APPEND PROPERTY SNAPPER_LAYERS "${name}=${joined}")
endfunction()

# snapper_test(<module>) builds each tests/<module>/<name>_tests.cpp
# into its own test program, linked against the module.
function(snapper_test module)
  file(GLOB sources CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/tests/${module}/*_tests.cpp)
  foreach(source IN LISTS sources)
    get_filename_component(target ${source} NAME_WE)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE snapper_${module} Qt6::Test)
    snapper_warnings(${target})
    # -silent: only failures and the totals, so a run costs an agent a
    # few lines instead of one per test function.
    add_test(NAME ${target} COMMAND ${target} -silent)
    set_tests_properties(${target} PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endforeach()
endfunction()

# Writes the declared module graph and adds the checks that read it.
function(snapper_checks)
  get_property(layers GLOBAL PROPERTY SNAPPER_LAYERS)
  string(JOIN "\n" text ${layers})
  set(layer_file ${PROJECT_BINARY_DIR}/layers.txt)
  file(WRITE ${layer_file} "${text}\n")
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_test(NAME layers COMMAND Python3::Interpreter
    ${PROJECT_SOURCE_DIR}/tools/check_layers.py
    ${layer_file} ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/tests)
  add_test(NAME layers_self_test COMMAND Python3::Interpreter
    ${PROJECT_SOURCE_DIR}/tools/check_layers.py --self-test)
  file(GLOB_RECURSE code CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/src/*.h ${PROJECT_SOURCE_DIR}/src/*.cpp
    ${PROJECT_SOURCE_DIR}/tests/*.h ${PROJECT_SOURCE_DIR}/tests/*.cpp)
  set(SNAPPER_LINTER "" CACHE FILEPATH "Path to snawys_lint.py")
  if(EXISTS "${SNAPPER_LINTER}")
    add_test(NAME lint COMMAND Python3::Interpreter ${SNAPPER_LINTER}
      ${code})
  else()
    message(WARNING "SNAPPER_LINTER not set: the lint test is off")
  endif()
endfunction()
