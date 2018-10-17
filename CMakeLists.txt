cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(anf)

set(CMAKE_C_STANDARD 99)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(Python_ADDITIONAL_VERSIONS 3.0)
find_package(PythonInterp)

include(CTest)

add_subdirectory(src)
add_subdirectory(test)
