cmake_minimum_required(VERSION 3.20)

if((CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" OR
    CMAKE_SYSTEM_NAME STREQUAL "Darwin") AND
   NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING
    "Minimum macOS version supported by the packaged SDK")
endif()

project(PrismSdkExamples VERSION 1.3.0 LANGUAGES C CXX)

include(CTest)

add_subdirectory(examples)

if(BUILD_TESTING)
  add_executable(device-model-names-test tests/device_model_names_test.cpp)
  target_include_directories(device-model-names-test PRIVATE include)
  target_compile_features(device-model-names-test PRIVATE cxx_std_17)
  add_dependencies(prism-all-examples device-model-names-test)
  add_test(NAME device-model-names COMMAND device-model-names-test)
  add_executable(raw8-header-test tests/raw8_header_test.cpp)
  target_include_directories(raw8-header-test PRIVATE include)
  target_compile_features(raw8-header-test PRIVATE cxx_std_17)
  add_dependencies(prism-all-examples raw8-header-test)
  add_test(NAME raw8-group-header COMMAND raw8-header-test)
  add_executable(camera-membership-test tests/camera_membership_test.cpp)
  target_include_directories(camera-membership-test PRIVATE include)
  target_compile_features(camera-membership-test PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_definitions(camera-membership-test PRIVATE NOMINMAX)
  endif()
  add_dependencies(prism-all-examples camera-membership-test)
  add_test(NAME camera-membership COMMAND camera-membership-test)
endif()
