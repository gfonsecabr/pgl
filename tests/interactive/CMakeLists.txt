cmake_minimum_required(VERSION 3.16)
project(pgl_interactive LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
  # pgl treats degenerate geometric input as UB and guards it with asserts;
  # a live editor constantly produces such input, so build without asserts.
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

add_executable(pgl_interactive main.cpp)

target_include_directories(pgl_interactive PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_link_libraries(pgl_interactive PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# Keep asserts off even for a Debug configure: see the note above.
target_compile_definitions(pgl_interactive PRIVATE NDEBUG)
