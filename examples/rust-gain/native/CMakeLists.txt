cmake_minimum_required(VERSION 3.25)
project(PortableClackGain VERSION 0.1.0 LANGUAGES C CXX)
if(NOT APPLE)
  message(FATAL_ERROR "This native wrapper recipe is currently qualified on macOS only")
endif()
enable_language(OBJC OBJCXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES ON CACHE BOOL "Fetch wrapper dependencies")
set(CLAP_WRAPPER_COPY_AFTER_BUILD OFF CACHE BOOL "Keep qualification builds local" FORCE)
include(FetchContent)
FetchContent_Declare(clap_wrapper GIT_REPOSITORY https://github.com/free-audio/clap-wrapper.git
  GIT_TAG 1cca996e96f29ab2be7ae9f8cfe532bbc92e1dd6)
FetchContent_MakeAvailable(clap_wrapper)
find_program(CARGO cargo REQUIRED)
get_filename_component(PLUGIN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(RUST_ARCHIVE "${CMAKE_BINARY_DIR}/cargo/release/libportable_clack_gain.a")
add_custom_target(rust-plugin
  COMMAND ${CMAKE_COMMAND} -E env "CARGO_TARGET_DIR=${CMAKE_BINARY_DIR}/cargo"
    "${CARGO}" build --manifest-path "${PLUGIN_ROOT}/Cargo.toml" --release --locked
  BYPRODUCTS "${RUST_ARCHIVE}" VERBATIM)
add_library(clack_gain_impl STATIC IMPORTED GLOBAL)
set_target_properties(clack_gain_impl PROPERTIES IMPORTED_LOCATION "${RUST_ARCHIVE}")
add_dependencies(clack_gain_impl rust-plugin)
# Keep the Rust-exported CLAP entry in the plain CLAP bundle as well as wrappers.
# Rust owns the entry; there is no second C++ implementation of its ABI.
target_link_options(clack_gain_impl INTERFACE "LINKER:-u,_clap_entry")
target_link_libraries(clack_gain_impl INTERFACE clap clap-wrapper-extensions "-framework Security" iconv)
make_clapfirst_plugins(TARGET_NAME portable_clack_gain IMPL_TARGET clack_gain_impl
  ENTRY_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/entry.cpp"
  OUTPUT_NAME "Portable Clack Gain" BUNDLE_IDENTIFIER "org.example.portable-clack-gain"
  BUNDLE_VERSION ${PROJECT_VERSION} COPY_AFTER_BUILD FALSE
  PLUGIN_FORMATS CLAP VST3 AUV2 ASSET_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
  AUV2_MANUFACTURER_NAME "Example" AUV2_MANUFACTURER_CODE "Exmp"
  AUV2_SUBTYPE_CODE "ClGn" AUV2_INSTRUMENT_TYPE "aufx")
enable_testing()
find_program(CLAP_VALIDATOR clap-validator)
find_program(VST3_VALIDATOR vst3-validator)
if(CLAP_VALIDATOR)
  add_test(NAME clap-validator COMMAND "${CLAP_VALIDATOR}" -v quiet validate --json "$<TARGET_BUNDLE_DIR:portable_clack_gain_clap>")
endif()
if(VST3_VALIDATOR)
  add_test(NAME vst3-validator COMMAND "${VST3_VALIDATOR}" "$<TARGET_BUNDLE_DIR:portable_clack_gain_vst3>")
endif()
add_executable(au-host "${PLUGIN_ROOT}/../../tests/au-host.cpp")
target_compile_options(au-host PRIVATE -UNDEBUG)
target_link_libraries(au-host PRIVATE "-framework AudioToolbox" "-framework CoreFoundation")
add_dependencies(au-host portable_clack_gain_auv2)
add_test(NAME au-numerical-state COMMAND au-host "$<TARGET_FILE:portable_clack_gain_auv2>" gain)
