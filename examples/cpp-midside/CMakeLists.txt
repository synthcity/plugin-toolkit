cmake_minimum_required(VERSION 3.20)
project(ModularClapExample LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/prepare.py
  COMMAND_ERROR_IS_FATAL ANY)

# One CLAP implementation and one DSP source for every target. A production pack
# can rename its entry symbol when linking several plugins into the same runtime.
add_library(midside-code OBJECT src/webclap.cpp)
target_include_directories(midside-code PUBLIC include build)
set_target_properties(midside-code PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(EMSCRIPTEN)
  add_executable(midside $<TARGET_OBJECTS:midside-code>)
  set_target_properties(midside PROPERTIES SUFFIX ".wasm")
  target_link_options(midside PRIVATE --no-entry -sSTANDALONE_WASM=1
    -sALLOW_MEMORY_GROWTH=0 -sINITIAL_MEMORY=2097152 -sSTACK_SIZE=65536
    "-sEXPORTED_FUNCTIONS=['_malloc','_free']"
    -Wl,--export=clap_entry -Wl,--export-table -Wl,--growable-table)
  # Upstream WebCLAP directory bundle: no host-specific manifest is required.
  install(FILES $<TARGET_FILE:midside> DESTINATION midside.wclap RENAME module.wasm)
  install(FILES LICENSE DESTINATION midside.wclap RENAME LICENSE-adapter.txt)
  install(FILES upstream/LICENSE.txt DESTINATION midside.wclap RENAME LICENSE-DSP.txt)
  install(FILES include/CLAP-LICENSE DESTINATION midside.wclap RENAME LICENSE-CLAP.txt)
else()
  add_library(midside MODULE $<TARGET_OBJECTS:midside-code>)
  set_target_properties(midside PROPERTIES PREFIX "" SUFFIX ".clap")
  if(APPLE)
    # CLAP on macOS is a bundle, not a dylib with a renamed suffix.
    set_target_properties(midside PROPERTIES BUNDLE TRUE BUNDLE_EXTENSION clap)
  endif()
endif()

# Portable example checks; no product engine or host implementation.
if(NOT EMSCRIPTEN)
  enable_testing()
  add_executable(dsp-test src/probe.cpp)
  target_compile_definitions(dsp-test PRIVATE NATIVE_TEST)
  target_include_directories(dsp-test PRIVATE include build)
  target_compile_options(dsp-test PRIVATE -UNDEBUG)
  add_test(NAME original-dsp COMMAND dsp-test)
  add_executable(modular-test tests/modular.cpp $<TARGET_OBJECTS:midside-code>)
  target_include_directories(modular-test PRIVATE include)
  target_compile_options(modular-test PRIVATE -UNDEBUG)
  add_test(NAME modular-ports COMMAND modular-test)
endif()
