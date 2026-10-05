# Shared target setup, so every module and test is built the same way.

function(snapper_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
      -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Werror)
  endif()
endfunction()

# snapper_module(<name> SOURCES ... MODULES <lower modules> LIBS <libs>)
# Builds src/<name> as snapper_<name> and records which modules it may
# include, for the layer check.
function(snapper_module name)
  cmake_parse_arguments(ARG "" "" "SOURCES;MODULES;LIBS" ${ARGN})
  set(target snapper_${name})
  set(module_targets)
  foreach(module IN LISTS ARG_MODULES)
    list(APPEND module_targets snapper_${module})
  endforeach()
  list(TRANSFORM ARG_SOURCES PREPEND ${PROJECT_SOURCE_DIR}/src/${name}/)
  add_library(${target} STATIC ${ARG_SOURCES})
  target_include_directories(${target} PUBLIC ${PROJECT_SOURCE_DIR}/src)
  target_link_libraries(${target} PUBLIC ${module_targets} ${ARG_LIBS})
  snapper_warnings(${target})
  string(JOIN "," joined ${ARG_MODULES})
  set_property(GLOBAL APPEND PROPERTY SNAPPER_LAYERS "${name}=${joined}")
endfunction()

# snapper_test(<module> SOURCES ...) builds tests/<module> into one test.
function(snapper_test module)
  cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
  set(target ${module}_tests)
  list(TRANSFORM ARG_SOURCES PREPEND ${PROJECT_SOURCE_DIR}/tests/${module}/)
  add_executable(${target} ${ARG_SOURCES})
  target_link_libraries(${target} PRIVATE snapper_${module} Qt6::Test)
  snapper_warnings(${target})
  add_test(NAME ${target} COMMAND ${target})
  set_tests_properties(${target} PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
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
