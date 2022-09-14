cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
project(wiggly VERSION 0.1.0 LANGUAGES C CXX)

# C17 standard, no extensions
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# C++17 standard, no extensions
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# include(CTest)
enable_testing()

find_package(OpenGL REQUIRED)

add_executable(wiggly WIN32 main.cpp)

target_include_directories(wiggly PUBLIC third_party/include)
target_link_libraries(wiggly PRIVATE gdi32 user32 opengl32)
target_compile_definitions(wiggly PUBLIC KHRONOS_STATIC)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
