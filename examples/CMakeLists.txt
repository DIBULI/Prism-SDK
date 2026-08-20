cmake_minimum_required(VERSION 3.20)

if((CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" OR
    CMAKE_SYSTEM_NAME STREQUAL "Darwin") AND
   NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING
    "Minimum macOS version supported by the packaged SDK")
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(PrismDeviceInfoTimeSync VERSION 0.11.0 LANGUAGES CXX)
endif()

get_filename_component(_prism_sdk_default_root
  "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(PRISM_SDK_ROOT "${_prism_sdk_default_root}" CACHE PATH
  "Root of the Prism Host SDK binary package")
unset(_prism_sdk_default_root)

set(_prism_sdk_include "${PRISM_SDK_ROOT}/include")
if(NOT EXISTS "${_prism_sdk_include}/prism/usb_sdk.hpp")
  message(FATAL_ERROR
    "PRISM_SDK_ROOT does not contain include/prism/usb_sdk.hpp: "
    "${PRISM_SDK_ROOT}")
endif()

add_executable(prism-device-info-time-sync device_info_time_sync.cpp)
target_compile_features(prism-device-info-time-sync PRIVATE cxx_std_17)
target_include_directories(prism-device-info-time-sync PRIVATE
  "${_prism_sdk_include}")

if(MSVC)
  target_compile_options(prism-device-info-time-sync PRIVATE /W4 /permissive-)
else()
  target_compile_options(prism-device-info-time-sync PRIVATE
    -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
  if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The packaged Windows runtime requires MSVC x64")
  endif()
  set(_prism_sdk_runtime
    "${PRISM_SDK_ROOT}/runtime/windows-x64/prism_usb_sdk.dll")
  target_compile_definitions(prism-device-info-time-sync PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00
    _ITERATOR_DEBUG_LEVEL=0)
  set_target_properties(prism-device-info-time-sync PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
  add_custom_command(TARGET prism-device-info-time-sync POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${_prism_sdk_runtime}"
      "$<TARGET_FILE_DIR:prism-device-info-time-sync>"
    VERBATIM)
elseif(APPLE)
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
  target_link_libraries(prism-device-info-time-sync PRIVATE PrismUsbSdkRuntime)
  set_target_properties(prism-device-info-time-sync PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@executable_path")
  add_custom_command(TARGET prism-device-info-time-sync POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${_prism_sdk_runtime}"
      "$<TARGET_FILE_DIR:prism-device-info-time-sync>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${_prism_libusb_runtime}"
      "$<TARGET_FILE_DIR:prism-device-info-time-sync>"
    VERBATIM)
elseif(UNIX)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _prism_processor)
  if(NOT _prism_processor MATCHES "^(x86_64|amd64)$")
    message(FATAL_ERROR "The packaged Linux runtime supports x86-64 only")
  endif()
  unset(_prism_processor)
  set(_prism_sdk_runtime
    "${PRISM_SDK_ROOT}/runtime/linux-x64/libprism_usb_sdk.so")
  add_library(PrismUsbSdkRuntime SHARED IMPORTED)
  set_target_properties(PrismUsbSdkRuntime PROPERTIES
    IMPORTED_LOCATION "${_prism_sdk_runtime}")
  target_link_libraries(prism-device-info-time-sync PRIVATE PrismUsbSdkRuntime)
  set_target_properties(prism-device-info-time-sync PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN")
  add_custom_command(TARGET prism-device-info-time-sync POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${_prism_sdk_runtime}"
      "$<TARGET_FILE_DIR:prism-device-info-time-sync>"
    VERBATIM)
else()
  message(FATAL_ERROR "Unsupported host platform")
endif()

if(NOT EXISTS "${_prism_sdk_runtime}")
  message(FATAL_ERROR "Missing packaged runtime: ${_prism_sdk_runtime}")
endif()

unset(_prism_sdk_include)
unset(_prism_sdk_runtime)
unset(_prism_libusb_runtime)
