cmake_minimum_required(VERSION 3.24)
project(SchedMesh VERSION 0.1.0 LANGUAGES CXX)

find_package(Threads)

option(SCHEDMESH_BUILD_LEGACY "Build the preserved legacy generator" ON)
option(SCHEDMESH_BUILD_TESTS "Build the next-generation test suite" ON)
option(SCHEDMESH_FETCH_DEPENDENCIES "Download pinned dependencies when unavailable" ON)
option(SCHEDMESH_ENABLE_CLANG_TIDY "Run clang-tidy on next-generation targets" OFF)

set(LEGACY_SOURCES
    src/schedmesh.cpp
    src/csvrow.cpp
    src/auxiliary.cpp
    src/printdata.cpp
)

function(schedmesh_enable_warnings target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
    )
endfunction()

function(schedmesh_copy_runtime_dependencies target)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target}>
                $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS
        )
    endif()
endfunction()

function(schedmesh_enable_clang_tidy target)
    if(SCHEDMESH_ENABLE_CLANG_TIDY)
        find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    endif()
endfunction()

if(SCHEDMESH_BUILD_LEGACY)
    add_executable(SchedMesh ${LEGACY_SOURCES})
    target_compile_features(SchedMesh PRIVATE cxx_std_17)
    schedmesh_enable_warnings(SchedMesh)
    target_link_libraries(SchedMesh PRIVATE Threads::Threads)
endif()

find_package(ortools 9.15 CONFIG QUIET)
if(NOT ortools_FOUND AND SCHEDMESH_FETCH_DEPENDENCIES)
    include(FetchContent)
    set(BUILD_DEPS ON CACHE BOOL "Build OR-Tools dependencies" FORCE)
    set(BUILD_SAMPLES OFF CACHE BOOL "Build OR-Tools samples" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "Build OR-Tools examples" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "Build dependency tests" FORCE)
    set(BUILD_FLATZINC OFF CACHE BOOL "Build the FlatZinc frontend" FORCE)
    set(BUILD_MATH_OPT ON CACHE BOOL "Build MathOpt required by the Windows runtime solver stubs" FORCE)
    set(USE_COINOR OFF CACHE BOOL "Enable COIN-OR" FORCE)
    set(USE_CPLEX OFF CACHE BOOL "Enable CPLEX" FORCE)
    set(USE_GLPK OFF CACHE BOOL "Enable GLPK" FORCE)
    # Keep the runtime Gurobi loader enabled: OR-Tools 9.15's shared Windows
    # library references its stubs even when SchedMesh only uses CP-SAT.
    set(USE_GUROBI ON CACHE BOOL "Enable the optional runtime Gurobi loader" FORCE)
    set(USE_HIGHS OFF CACHE BOOL "Enable HiGHS" FORCE)
    set(USE_SCIP OFF CACHE BOOL "Enable SCIP" FORCE)
    set(USE_XPRESS OFF CACHE BOOL "Enable Xpress" FORCE)
    FetchContent_Declare(
        ortools
        URL https://github.com/google/or-tools/releases/download/v9.15/or-tools-9.15.tar.gz
        URL_HASH SHA256=599c870319bb127441d92c452d8f79bca46ca6fd295c1deb8031ed303a361311
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(ortools)
endif()

if(NOT TARGET ortools::ortools)
    message(FATAL_ERROR "OR-Tools 9.15 is required. Install it or enable SCHEDMESH_FETCH_DEPENDENCIES.")
endif()

add_executable(schedmesh-next src/app/main.cpp src/solver/smoke_solver.cpp)
target_include_directories(schedmesh-next PRIVATE include)
target_compile_features(schedmesh-next PRIVATE cxx_std_20)
target_compile_definitions(schedmesh-next PRIVATE SCHEDMESH_VERSION="${PROJECT_VERSION}")
target_link_libraries(schedmesh-next PRIVATE ortools::ortools)
schedmesh_enable_warnings(schedmesh-next)
schedmesh_copy_runtime_dependencies(schedmesh-next)
schedmesh_enable_clang_tidy(schedmesh-next)

add_library(schedmesh-domain src/domain/calendar.cpp)
target_include_directories(schedmesh-domain PUBLIC include)
target_compile_features(schedmesh-domain PUBLIC cxx_std_20)
schedmesh_enable_warnings(schedmesh-domain)
schedmesh_enable_clang_tidy(schedmesh-domain)

if(SCHEDMESH_BUILD_TESTS)
    set(BUILD_TESTING ON CACHE BOOL "Build SchedMesh tests" FORCE)
    include(CTest)
    find_package(GTest 1.17 CONFIG QUIET)
    if(NOT GTest_FOUND AND SCHEDMESH_FETCH_DEPENDENCIES)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
            URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        set(gtest_force_shared_crt ON CACHE BOOL "Use the shared MSVC runtime" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(schedmesh-next-tests tests/solver/smoke_solver_test.cpp src/solver/smoke_solver.cpp)
    target_include_directories(schedmesh-next-tests PRIVATE include)
    target_compile_features(schedmesh-next-tests PRIVATE cxx_std_20)
    target_link_libraries(schedmesh-next-tests PRIVATE GTest::gtest_main ortools::ortools)
    schedmesh_enable_warnings(schedmesh-next-tests)
    schedmesh_copy_runtime_dependencies(schedmesh-next-tests)
    schedmesh_enable_clang_tidy(schedmesh-next-tests)

    add_executable(schedmesh-domain-tests tests/domain/domain_test.cpp)
    target_compile_features(schedmesh-domain-tests PRIVATE cxx_std_20)
    target_link_libraries(schedmesh-domain-tests PRIVATE GTest::gtest_main schedmesh-domain)
    schedmesh_enable_warnings(schedmesh-domain-tests)
    schedmesh_copy_runtime_dependencies(schedmesh-domain-tests)
    schedmesh_enable_clang_tidy(schedmesh-domain-tests)

    include(GoogleTest)
    gtest_discover_tests(schedmesh-next-tests)
    gtest_discover_tests(schedmesh-domain-tests)
endif()
