# F's Plugins (bryful, MIT) under Snapper's stand-in for the After Effects
# host. Not Snapper's code: built without the project's warnings, and
# optimised in Debug too (the pixel loops are far too slow without).
# Only include/fsp_host.h is public.

find_package(Threads REQUIRED)

file(GLOB fsp_shim_sources CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/shim/src/*.cpp)
# units/<plugin>_unit.cpp, plus units/<plugin>_unit_<n>.cpp when a
# plugin's sources clash in one unit (see VENDORED.md).
file(GLOB fsp_unit_sources CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/units/*_unit*.cpp)

add_library(fsp_vendor STATIC
  ${fsp_shim_sources}
  ${fsp_unit_sources}
  ${CMAKE_CURRENT_SOURCE_DIR}/registry.cpp)
target_include_directories(fsp_vendor
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shim/include
          ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(fsp_vendor PRIVATE
  -w -fpermissive -fexceptions "$<$<CONFIG:Debug>:-O2>")
# F's hashes multiply signed ints past their range and count on them
# wrapping, as they do under MSVC. gcc at -O2 otherwise treats that
# overflow as impossible and folds NF_Spat_Data's hash_01 to one value
# for every grain, so Grain's accents all land on one spot.
target_compile_options(fsp_vendor PRIVATE -fwrapv)
target_link_libraries(fsp_vendor PRIVATE Threads::Threads)
set_target_properties(fsp_vendor PROPERTIES POSITION_INDEPENDENT_CODE ON)
