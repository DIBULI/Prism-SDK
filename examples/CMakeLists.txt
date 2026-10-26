cmake_minimum_required(VERSION 3.20)

if((CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" OR
    CMAKE_SYSTEM_NAME STREQUAL "Darwin") AND
   NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING
    "Minimum macOS version supported by the packaged SDK")
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(PrismSdkExamples VERSION 1.3.0 LANGUAGES C CXX)
  include(CTest)
endif()

get_filename_component(_prism_sdk_default_root
  "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(PRISM_SDK_ROOT "${_prism_sdk_default_root}" CACHE PATH
  "Root of the Prism Host SDK binary package")
unset(_prism_sdk_default_root)
option(PRISM_SDK_USE_STATIC
  "Link the packaged Linux static Host SDK instead of its shared runtime" OFF)
option(PRISM_SDK_BUILD_RAW12_EXAMPLE "Build the optional Zstandard RAW12 decoder example" OFF)

set(_prism_sdk_include "${PRISM_SDK_ROOT}/include")
if(NOT EXISTS "${_prism_sdk_include}/prism/usb_sdk.hpp")
  message(FATAL_ERROR
    "PRISM_SDK_ROOT does not contain include/prism/usb_sdk.hpp: "
    "${PRISM_SDK_ROOT}")
endif()

if(WIN32)
  if(PRISM_SDK_USE_STATIC)
    message(FATAL_ERROR "The packaged static Host SDK currently supports Linux only")
  endif()
  if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The packaged Windows runtime requires MSVC x64")
  endif()
  set(_prism_sdk_runtime
    "${PRISM_SDK_ROOT}/runtime/windows-x64/prism_usb_sdk.dll")
elseif(APPLE)
  if(PRISM_SDK_USE_STATIC)
    message(FATAL_ERROR "The packaged static Host SDK currently supports Linux only")
  endif()
  if(CMAKE_OSX_ARCHITECTURES AND
     NOT CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    message(FATAL_ERROR "The packaged macOS runtime supports arm64 only")
  endif()
  set(_prism_sdk_runtime
    "${PRISM_SDK_ROOT}/runtime/macos-arm64/libprism_usb_sdk.dylib")
  set(_prism_libusb_runtime
    "${PRISM_SDK_ROOT}/runtime/macos-arm64/libusb-1.0.0.dylib")
  add_library(PrismUsbSdkRuntime SHARED IMPORTED)
  set_target_properties(PrismUsbSdkRuntime PROPERTIES
    IMPORTED_LOCATION "${_prism_sdk_runtime}")
elseif(UNIX)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _prism_processor)
  if(_prism_processor MATCHES "^(x86_64|amd64)$")
    set(_prism_sdk_runtime_dir
      "${PRISM_SDK_ROOT}/runtime/linux-x64")
  elseif(_prism_processor MATCHES "^(aarch64|arm64)$")
    set(_prism_sdk_runtime_dir
      "${PRISM_SDK_ROOT}/runtime/linux-arm64")
  else()
    message(FATAL_ERROR
      "The packaged Linux runtime supports x86-64 and arm64 only")
  endif()
  unset(_prism_processor)
  if(PRISM_SDK_USE_STATIC)
    set(_prism_sdk_runtime
      "${_prism_sdk_runtime_dir}/libprism_usb_sdk.a")
    add_library(PrismUsbSdkRuntime STATIC IMPORTED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    find_package(Threads REQUIRED)
    set_target_properties(PrismUsbSdkRuntime PROPERTIES
      INTERFACE_LINK_LIBRARIES "PkgConfig::LIBUSB;OpenSSL::Crypto;Threads::Threads")
  else()
    set(_prism_sdk_runtime
      "${_prism_sdk_runtime_dir}/libprism_usb_sdk.so")
    add_library(PrismUsbSdkRuntime SHARED IMPORTED)
  endif()
  unset(_prism_sdk_runtime_dir)
  set_target_properties(PrismUsbSdkRuntime PROPERTIES
    IMPORTED_LOCATION "${_prism_sdk_runtime}")
else()
  message(FATAL_ERROR "Unsupported host platform")
endif()

if(NOT EXISTS "${_prism_sdk_runtime}")
  message(FATAL_ERROR "Missing packaged runtime: ${_prism_sdk_runtime}")
endif()
if(APPLE AND NOT EXISTS "${_prism_libusb_runtime}")
  message(FATAL_ERROR "Missing packaged libusb runtime: ${_prism_libusb_runtime}")
endif()

# All examples share one output directory.  Copying the same runtime from
# several POST_BUILD commands races under a parallel build, so stage it once
# through a shared dependency instead.
set(_prism_runtime_stage_commands)
if(CMAKE_CONFIGURATION_TYPES)
  set(_prism_runtime_stage_dir "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")
else()
  set(_prism_runtime_stage_dir "${CMAKE_CURRENT_BINARY_DIR}")
endif()
if(WIN32 OR APPLE OR (UNIX AND NOT PRISM_SDK_USE_STATIC))
  list(APPEND _prism_runtime_stage_commands
    COMMAND "${CMAKE_COMMAND}" -E make_directory
      "${_prism_runtime_stage_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${_prism_sdk_runtime}"
      "${_prism_runtime_stage_dir}")
endif()
if(APPLE)
  list(APPEND _prism_runtime_stage_commands
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${_prism_libusb_runtime}"
      "${_prism_runtime_stage_dir}")
endif()
add_custom_target(prism-stage-runtime
  ${_prism_runtime_stage_commands}
  COMMENT "Staging the Prism SDK runtime"
  VERBATIM)

function(prism_add_example target source)
  add_executable(${target} "${source}")
  add_dependencies(${target} prism-stage-runtime)
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_include_directories(${target} PRIVATE "${_prism_sdk_include}")

  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  if(WIN32)
    target_compile_definitions(${target} PRIVATE
      WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00
      _ITERATOR_DEBUG_LEVEL=0)
    set_target_properties(${target} PROPERTIES
      MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
  elseif(APPLE)
    target_link_libraries(${target} PRIVATE PrismUsbSdkRuntime)
    set_target_properties(${target} PROPERTIES
      BUILD_WITH_INSTALL_RPATH TRUE
      INSTALL_RPATH "@executable_path")
  else()
    target_link_libraries(${target} PRIVATE PrismUsbSdkRuntime)
    if(NOT PRISM_SDK_USE_STATIC)
      set_target_properties(${target} PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN")
    endif()
  endif()
endfunction()

set(_prism_cross_platform_example_sources
  device/device_info_time_sync.cpp)
set(_prism_direct_api_example_sources
  network/network_camera_imu_capture.cpp
  network/network_connect.cpp
  raw12/profile_raw12_decode.cpp
  camera_imu/camera_imu_capture.cpp
  api/client_api_examples.cpp
  api/configuration_api_examples.cpp
  lidar/lidar_capture.cpp
  api/parser_api_examples.cpp
  api/stream_api_examples.cpp
  gnss_rtk/gnss_rtk_status.cpp
  gnss_rtk/rtk_position.cpp
  gnss_rtk/rtk_module_control.cpp)
set(_prism_rklocal_example_sources rklocal/rklocal_capture.cpp rklocal/rklocal_gnss_status.cpp)
set(_prism_windows_runtime_example_sources
  windows/windows_runtime_api_examples.cpp)
set(_prism_registered_example_sources
  ${_prism_cross_platform_example_sources}
  ${_prism_direct_api_example_sources}
  ${_prism_windows_runtime_example_sources}
  ${_prism_rklocal_example_sources})

# Keep the source inventory strict: adding examples/**/*.cpp without registering a
# build target must fail configuration instead of silently escaping CI.
file(GLOB_RECURSE _prism_discovered_example_sources
  RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
  CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/*.c")
list(SORT _prism_discovered_example_sources)
list(SORT _prism_registered_example_sources)
if(NOT "${_prism_discovered_example_sources}" STREQUAL
       "${_prism_registered_example_sources}")
  message(FATAL_ERROR
    "Every examples/**/*.cpp file must be registered in examples/CMakeLists.txt. "
    "Discovered: ${_prism_discovered_example_sources}; "
    "registered: ${_prism_registered_example_sources}")
endif()

prism_add_example(prism-device-info-time-sync device/device_info_time_sync.cpp)
set(_prism_supported_example_targets prism-device-info-time-sync)

# These examples use the direct Client and stream helper classes. The packaged
# Windows SDK is consumed through RuntimeApi instead, so keep the direct examples
# off the Windows build rather than presenting unsupported linkage instructions.
if(WIN32)
  prism_add_example(prism-windows-runtime-api-examples
    windows/windows_runtime_api_examples.cpp)
  list(APPEND _prism_supported_example_targets
    prism-windows-runtime-api-examples)

  if(BUILD_TESTING)
    add_test(NAME prism-windows-runtime-api-smoke
      COMMAND prism-windows-runtime-api-examples)
  endif()
else()
  prism_add_example(prism-network-camera-imu-capture network/network_camera_imu_capture.cpp)
  list(APPEND _prism_supported_example_targets prism-network-camera-imu-capture)
  if(BUILD_TESTING)
    add_test(NAME prism-network-camera-imu-help COMMAND prism-network-camera-imu-capture --help)
    add_test(NAME prism-network-camera-imu-invalid-port
      COMMAND prism-network-camera-imu-capture --host 127.0.0.1 --output unused --port 65536)
    add_test(NAME prism-network-camera-imu-invalid-duration
      COMMAND prism-network-camera-imu-capture --host 127.0.0.1 --output unused --seconds 0)
    set_tests_properties(prism-network-camera-imu-invalid-port
      prism-network-camera-imu-invalid-duration PROPERTIES WILL_FAIL TRUE)
  endif()
  prism_add_example(prism-network-connect network/network_connect.cpp)
  list(APPEND _prism_supported_example_targets prism-network-connect)
  if(BUILD_TESTING)
    add_test(NAME prism-network-connect-help COMMAND prism-network-connect --help)
  endif()
  if(PRISM_SDK_BUILD_RAW12_EXAMPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)
    prism_add_example(prism-profile-raw12-decode raw12/profile_raw12_decode.cpp)
    target_link_libraries(prism-profile-raw12-decode PRIVATE PkgConfig::ZSTD)
    list(APPEND _prism_supported_example_targets prism-profile-raw12-decode)
    if(BUILD_TESTING)
      add_test(NAME prism-profile-raw12-self-test COMMAND prism-profile-raw12-decode --self-test)
    endif()
  endif()
  prism_add_example(prism-camera-imu-capture camera_imu/camera_imu_capture.cpp)
  prism_add_example(prism-client-api-examples api/client_api_examples.cpp)
  prism_add_example(prism-configuration-api-examples
    api/configuration_api_examples.cpp)
  prism_add_example(prism-lidar-capture lidar/lidar_capture.cpp)
  prism_add_example(prism-parser-api-examples api/parser_api_examples.cpp)
  prism_add_example(prism-stream-api-examples api/stream_api_examples.cpp)
  prism_add_example(prism-gnss-rtk-status gnss_rtk/gnss_rtk_status.cpp)
  prism_add_example(prism-rtk-position gnss_rtk/rtk_position.cpp)
  prism_add_example(prism-rtk-module-control gnss_rtk/rtk_module_control.cpp)
  list(APPEND _prism_supported_example_targets
    prism-camera-imu-capture
    prism-client-api-examples
    prism-configuration-api-examples
    prism-lidar-capture
    prism-parser-api-examples
    prism-stream-api-examples
    prism-gnss-rtk-status
    prism-rtk-position
    prism-rtk-module-control)

  if(BUILD_TESTING)
    add_test(NAME prism-camera-frame-tracker-self-test
      COMMAND prism-camera-imu-capture --self-test)
    add_test(NAME prism-client-api-examples-smoke
      COMMAND prism-client-api-examples)
    add_test(NAME prism-configuration-api-examples-smoke
      COMMAND prism-configuration-api-examples)
    add_test(NAME prism-parser-api-examples-smoke
      COMMAND prism-parser-api-examples)
    add_test(NAME prism-stream-api-examples-smoke
      COMMAND prism-stream-api-examples)
    add_test(NAME prism-gnss-rtk-status-help COMMAND prism-gnss-rtk-status --help)
    add_test(NAME prism-gnss-numeric-parser-self-test COMMAND prism-gnss-rtk-status --self-test)
    add_test(NAME prism-rtk-module-control-help COMMAND prism-rtk-module-control --help)
    add_test(NAME prism-rtk-position-help COMMAND prism-rtk-position --help)
    add_test(NAME prism-rtk-position-self-test COMMAND prism-rtk-position --self-test)
    add_test(NAME prism-rtk-position-invalid-duration COMMAND prism-rtk-position --seconds 0)
    set_tests_properties(prism-rtk-position-invalid-duration PROPERTIES WILL_FAIL TRUE)
  endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
  include("${PRISM_SDK_ROOT}/cmake/PrismRkLocalSdk.cmake")
  add_executable(prism-rklocal-rtk-module-control gnss_rtk/rtk_module_control.cpp)
  target_compile_definitions(prism-rklocal-rtk-module-control PRIVATE PRISM_EXAMPLE_RKLOCAL)
  target_link_libraries(prism-rklocal-rtk-module-control PRIVATE Prism::RkLocal)
  list(APPEND _prism_supported_example_targets prism-rklocal-rtk-module-control)
  add_executable(prism-rklocal-rtk-position gnss_rtk/rtk_position.cpp)
  target_compile_definitions(prism-rklocal-rtk-position PRIVATE PRISM_EXAMPLE_RKLOCAL)
  target_link_libraries(prism-rklocal-rtk-position PRIVATE Prism::RkLocal)
  target_compile_options(prism-rklocal-rtk-position PRIVATE -Wall -Wextra -Wpedantic -Werror)
  list(APPEND _prism_supported_example_targets prism-rklocal-rtk-position)
  if(BUILD_TESTING)
    add_test(NAME prism-rklocal-rtk-module-control-help COMMAND prism-rklocal-rtk-module-control --help)
    add_test(NAME prism-rklocal-rtk-position-help COMMAND prism-rklocal-rtk-position --help)
    add_test(NAME prism-rklocal-rtk-position-self-test COMMAND prism-rklocal-rtk-position --self-test)
  endif()
  if(BUILD_TESTING)
    add_subdirectory("${PRISM_SDK_ROOT}/rk-local-sdk/tests" rklocal-tests)
  endif()
  foreach(_example capture gnss_status)
    string(REPLACE "_" "-" _name "${_example}")
    add_executable(prism-rklocal-${_name} rklocal/rklocal_${_example}.cpp)
    target_link_libraries(prism-rklocal-${_name} PRIVATE Prism::RkLocal)
    target_compile_features(prism-rklocal-${_name} PRIVATE cxx_std_17)
    target_compile_options(prism-rklocal-${_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    list(APPEND _prism_supported_example_targets prism-rklocal-${_name})
    if(BUILD_TESTING)
      add_test(NAME prism-rklocal-${_name}-help COMMAND prism-rklocal-${_name} --help)
    endif()
  endforeach()
endif()

# CI builds this target explicitly. It represents every example supported by
# the current packaged runtime. Windows uses RuntimeApi examples, where
# direct Client linkage is intentionally unavailable.
add_custom_target(prism-all-examples
  DEPENDS ${_prism_supported_example_targets})
if(TARGET prism-rklocal-api-test)
  add_dependencies(prism-all-examples prism-rklocal-api-test prism-rklocal-api-parity-test
    prism-rklocal-hardware-test)
endif()

unset(_prism_sdk_include)
unset(_prism_sdk_runtime)
unset(_prism_libusb_runtime)
unset(_prism_runtime_stage_commands)
unset(_prism_runtime_stage_dir)
unset(_prism_cross_platform_example_sources)
unset(_prism_direct_api_example_sources)
unset(_prism_windows_runtime_example_sources)
unset(_prism_registered_example_sources)
unset(_prism_discovered_example_sources)
unset(_prism_supported_example_targets)
