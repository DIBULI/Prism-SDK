include_guard(GLOBAL)

if(TARGET Prism::RkLocal)
  return()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "Prism::RkLocal supports Linux only")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _prism_rklocal_processor)
if(NOT _prism_rklocal_processor MATCHES "^(aarch64|arm64)$")
  message(FATAL_ERROR
    "Prism::RkLocal requires Linux ARM64; got ${CMAKE_SYSTEM_PROCESSOR}")
endif()

get_filename_component(_prism_rklocal_root
  "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_prism_rklocal_include "${_prism_rklocal_root}/include")
set(_prism_rklocal_archive
  "${_prism_rklocal_root}/runtime/linux-arm64/libprism_rklocal_sdk.a")

if(NOT EXISTS "${_prism_rklocal_include}/prism/rklocal_sdk.hpp")
  message(FATAL_ERROR "Missing Prism RK-local public header")
endif()
if(NOT EXISTS "${_prism_rklocal_archive}")
  message(FATAL_ERROR "Missing Prism RK-local ARM64 static library")
endif()

find_package(Threads REQUIRED)
add_library(Prism::RkLocal STATIC IMPORTED GLOBAL)
set_target_properties(Prism::RkLocal PROPERTIES
  IMPORTED_LOCATION "${_prism_rklocal_archive}"
  INTERFACE_INCLUDE_DIRECTORIES "${_prism_rklocal_include}"
  INTERFACE_COMPILE_FEATURES cxx_std_17
  INTERFACE_LINK_LIBRARIES "Threads::Threads;${CMAKE_DL_LIBS}")

unset(_prism_rklocal_processor)
unset(_prism_rklocal_root)
unset(_prism_rklocal_include)
unset(_prism_rklocal_archive)
