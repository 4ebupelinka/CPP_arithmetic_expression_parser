cmake_minimum_required(VERSION 3.16)
project(expression_parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(expression_parser_lib
    src/ast.cpp
    src/tokenizer.cpp
    src/parser.cpp
    src/evaluator.cpp
    src/csv_writer.cpp
    src/thread_pool.cpp)

target_include_directories(expression_parser_lib PUBLIC include)

find_package(Threads REQUIRED)
target_link_libraries(expression_parser_lib PRIVATE Threads::Threads)

add_executable(expression_parser
    src/main.cpp
    src/console.cpp
    src/file_utils.cpp
    src/progress_bar.cpp
    src/user_input.cpp
    src/generate_mode.cpp)

target_link_libraries(expression_parser PRIVATE expression_parser_lib)
