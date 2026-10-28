cmake_minimum_required(VERSION 3.22)
project(parlayhash_upsert_resize_repro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)

# Pinned to upstream HEAD at the time of the report. To test a local checkout
# (e.g. with the fix applied) configure with
#   -DFETCHCONTENT_SOURCE_DIR_PARLAYHASH=/path/to/parlayhash
# parlayhash is header-only; SOURCE_SUBDIR points at a directory without a
# CMakeLists.txt so its benchmarks/examples are not configured.
FetchContent_Declare(
  parlayhash
  GIT_REPOSITORY https://github.com/cmuparlay/parlayhash.git
  GIT_TAG        081408ba6111ce78b5add9129f8d902fa2fea58f
  SOURCE_SUBDIR  no_cmake
)
FetchContent_MakeAvailable(parlayhash)

find_package(Threads REQUIRED)

add_executable(upsert_resize_repro upsert_resize_repro.cpp)
target_include_directories(upsert_resize_repro SYSTEM PRIVATE "${parlayhash_SOURCE_DIR}/include")
target_link_libraries(upsert_resize_repro PRIVATE Threads::Threads)

enable_testing()
add_test(NAME upsert_resize COMMAND upsert_resize_repro)
