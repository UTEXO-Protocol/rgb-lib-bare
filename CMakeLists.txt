cmake_minimum_required(VERSION 3.25)

project(rgb_lib_bare CXX C)

find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

bare_target(target)

# Create the bare module
add_bare_module(rgb_lib_bare)

target_sources(
  ${rgb_lib_bare}
  PRIVATE
    binding.cc
)

target_include_directories(
  ${rgb_lib_bare}
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Link precompiled Rust static library for current target
add_library(rgblibcffi STATIC IMPORTED)
set_target_properties(rgblibcffi PROPERTIES
  IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/${target}/librgblibcffi.a"
)

target_link_libraries(
  ${rgb_lib_bare}
  PRIVATE
    rgblibcffi
)

# Platform-specific system library linkage
if(target MATCHES "ios")
  target_link_libraries(
    ${rgb_lib_bare}
    PRIVATE
      "-framework Security"
      "-framework SystemConfiguration"
      "-framework CoreFoundation"
      "-lc++"
      "-lz"
      "-lsqlite3"
      "-lresolv"
  )
elseif(target MATCHES "darwin")
  target_link_libraries(
    ${rgb_lib_bare}
    PRIVATE
      "-framework Security"
      "-framework SystemConfiguration"
      "-framework CoreFoundation"
      "-lc++"
      "-lz"
      "-lsqlite3"
      "-lresolv"
  )
elseif(target MATCHES "android")
  target_link_libraries(
    ${rgb_lib_bare}
    PRIVATE
      "-llog"
      "-lz"
      "-ldl"
      "-lm"
  )
elseif(target MATCHES "linux")
  target_link_libraries(
    ${rgb_lib_bare}
    PRIVATE
      "-lpthread"
      "-ldl"
      "-lm"
      "-lz"
      "-lsqlite3"
  )
endif()
