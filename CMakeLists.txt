cmake_minimum_required(VERSION 3.16)
project(SchedMesh)

find_package(Threads)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCES
    src/schedmesh.cpp
    src/csvrow.cpp
    src/auxiliary.cpp
    src/printdata.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
