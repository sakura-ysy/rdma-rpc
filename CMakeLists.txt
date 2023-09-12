cmake_minimum_required(VERSION 3.10)
project(rdma-example)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")

find_package(verbs)
find_package(rdmacm)
find_package(Threads)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libevent REQUIRED IMPORTED_TARGET libevent)

add_subdirectory(src)
add_subdirectory(app)
