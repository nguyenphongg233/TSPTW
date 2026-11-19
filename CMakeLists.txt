cmake_minimum_required(VERSION 3.16)
project(TSPTW LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra)

add_library(tsptw_core
    src/problem.cpp
    src/evaluator.cpp
    src/heuristic.cpp
    src/local_search.cpp
    src/alns.cpp
    src/utils.cpp
)
target_include_directories(tsptw_core PUBLIC include)

file(SIZE "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp" MAIN_FILE_SIZE)
if(MAIN_FILE_SIZE GREATER 0)
    add_executable(tsptw src/main.cpp)
    target_link_libraries(tsptw PRIVATE tsptw_core)
endif()

enable_testing()

add_executable(evaluator_test tests/evaluator_test.cpp)
target_link_libraries(evaluator_test PRIVATE tsptw_core)
add_test(NAME EvaluatorTest COMMAND evaluator_test)

file(SIZE "${CMAKE_CURRENT_SOURCE_DIR}/tests/heuristic_test.cpp" HEURISTIC_TEST_SIZE)
if(HEURISTIC_TEST_SIZE GREATER 0)
    add_executable(heuristic_test tests/heuristic_test.cpp)
    target_link_libraries(heuristic_test PRIVATE tsptw_core)
    add_test(NAME HeuristicTest COMMAND heuristic_test)
endif()

file(SIZE "${CMAKE_CURRENT_SOURCE_DIR}/tests/alns_test.cpp" ALNS_TEST_SIZE)
if(ALNS_TEST_SIZE GREATER 0)
    add_executable(alns_test tests/alns_test.cpp)
    target_link_libraries(alns_test PRIVATE tsptw_core)
    add_test(NAME ALNSTest COMMAND alns_test)
endif()
