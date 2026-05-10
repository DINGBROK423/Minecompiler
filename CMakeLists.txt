cmake_minimum_required(VERSION 3.16)

project(cmmc_frontend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(COMPILER_IR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/compiler_ir")

file(GLOB COMPILER_IR_SOURCES CONFIGURE_DEPENDS
    "${COMPILER_IR_DIR}/src/*.cpp"
)

add_library(compiler_ir STATIC ${COMPILER_IR_SOURCES})
target_include_directories(compiler_ir PUBLIC
    "${COMPILER_IR_DIR}/include"
)
target_compile_options(compiler_ir PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

file(GLOB FRONTEND_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(cmmc ${FRONTEND_SOURCES})
target_include_directories(cmmc PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(cmmc PRIVATE compiler_ir)
target_compile_options(cmmc PRIVATE
    -Wall
    -Wextra
)

set_target_properties(cmmc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_custom_target(run_tests
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_tests.sh"
    DEPENDS cmmc
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_custom_target(demo
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CMAKE_CURRENT_SOURCE_DIR}/build/out"
    COMMAND "$<TARGET_FILE:cmmc>" --dump-all tests/valid/functions.sy -o build/out
    COMMAND "$<TARGET_FILE:cmmc>" --lex tests/valid/basic.sy -o build/out/basic.tokens
    COMMAND "$<TARGET_FILE:cmmc>" --parse tests/valid/basic.sy -o build/out/basic.parse
    COMMAND "$<TARGET_FILE:cmmc>" --ast tests/valid/if_else.sy -o build/out/if_else.ast
    COMMAND "$<TARGET_FILE:cmmc>" --ir tests/ir/simple.sy -o build/out/simple.ll
    DEPENDS cmmc
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
