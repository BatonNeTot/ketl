cmake_minimum_required (VERSION 3.8)

project ("ketljit")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROJECT_TARGET "ketljit")

###############################

get_filename_component(PROJECT_GEN_OUTPUT_DIR "src/generated" REALPATH BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

get_filename_component(PROJECT_GEN_LEXER_OUTPUT "compiler/token.h" REALPATH BASE_DIR ${PROJECT_GEN_OUTPUT_DIR})
get_filename_component(PROJECT_GEN_LEXER_TEMPLATE "pygen/token_template.h" REALPATH BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(PROJECT_GEN_LEXER_GENERATOR "pygen/lexgen.py" REALPATH BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_custom_command(
    COMMAND python ${PROJECT_GEN_LEXER_GENERATOR} ${PROJECT_GEN_LEXER_TEMPLATE} ${PROJECT_GEN_LEXER_OUTPUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT ${PROJECT_GEN_LEXER_OUTPUT}
    DEPENDS ${PROJECT_GEN_LEXER_GENERATOR} ${PROJECT_GEN_LEXER_TEMPLATE})

get_filename_component(PROJECT_GEN_PARSER_OUTPUT "compiler/parser.c" REALPATH BASE_DIR ${PROJECT_GEN_OUTPUT_DIR})
get_filename_component(PROJECT_GEN_PARSER_TEMPLATE "pygen/parser_template.c" REALPATH BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(PROJECT_GEN_PARSER_GENERATOR "pygen/lalrgen.py" REALPATH BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_custom_command(
    COMMAND python ${PROJECT_GEN_PARSER_GENERATOR} ${PROJECT_GEN_PARSER_TEMPLATE} ${PROJECT_GEN_PARSER_OUTPUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT ${PROJECT_GEN_PARSER_OUTPUT}
    DEPENDS ${PROJECT_GEN_PARSER_GENERATOR} ${PROJECT_GEN_PARSER_TEMPLATE})

set(PROJECT_GEN_OUTPUT ${PROJECT_GEN_LEXER_OUTPUT} ${PROJECT_GEN_PARSER_OUTPUT})

###############################

if (NOT WIN32)
    set(CMAKE_C_COMPILER "/usr/bin/clang")
endif()

if(MSVC)
    add_compile_options(/we4013 /we4020 /we4029 /we4047 /we4098 /we4133 /we4244 /we4715 /we4716)
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic)

    if(CMAKE_BUILD_TYPE MATCHES "Debug")
        add_compile_options(-fsanitize=address)
    endif()
endif()

add_subdirectory ("src")

file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS include/*.h)

add_library (${PROJECT_TARGET} STATIC ${PROJECT_SOURCES} ${PROJECT_GEN_OUTPUT} ${PROJECT_HEADERS})
set_property(TARGET ${PROJECT_TARGET} PROPERTY C_STANDARD 11)

set_target_properties(${PROJECT_TARGET}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/$<CONFIG>"
    LINKER_LANGUAGE C
)

target_compile_definitions(${PROJECT_TARGET} PRIVATE _AMD64_)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_link_options(${PROJECT_TARGET} BEFORE PUBLIC -fsanitize=address)
endif()

target_include_directories(${PROJECT_TARGET} PUBLIC "include")
target_include_directories(${PROJECT_TARGET} PRIVATE "src" ${PROJECT_GEN_OUTPUT_DIR})

##################################

set(PROJECT_EXE_TARGET "${PROJECT_TARGET}_exe")

add_executable (${PROJECT_EXE_TARGET} "src/ketl.cpp")
set_property(TARGET ${PROJECT_EXE_TARGET} PROPERTY CXX_STANDARD 20)

set_property(TARGET ${PROJECT_EXE_TARGET} PROPERTY OUTPUT_NAME ketl)
target_compile_definitions(${PROJECT_EXE_TARGET} PUBLIC KETL_LOG_BREAK_LEVEL=6)

add_dependencies(${PROJECT_EXE_TARGET} ${PROJECT_TARGET})

set_target_properties(${PROJECT_EXE_TARGET}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>"
    LINKER_LANGUAGE CXX
)

target_compile_definitions(${PROJECT_EXE_TARGET} PRIVATE _AMD64_)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_link_options(${PROJECT_EXE_TARGET} BEFORE PUBLIC -fsanitize=address)
endif()

target_link_directories(${PROJECT_EXE_TARGET} PRIVATE "lib")
target_link_libraries(${PROJECT_EXE_TARGET} ${PROJECT_TARGET} ${PROJECT_LIBRARIES})
target_include_directories(${PROJECT_EXE_TARGET} PUBLIC "include")
target_include_directories(${PROJECT_EXE_TARGET} PRIVATE "src")

###############################

set(PROJECT_TEST_TARGET "${PROJECT_TARGET}_test")

add_subdirectory ("tests")

add_executable (${PROJECT_TEST_TARGET} ${PROJECT_TEST_SOURCES})
set_property(TARGET ${PROJECT_TEST_TARGET} PROPERTY CXX_STANDARD 20)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_TEST_TARGET})

add_dependencies(${PROJECT_TEST_TARGET} ${PROJECT_TARGET})

add_subdirectory ("dependencies_tests")

set_target_properties(${PROJECT_TEST_TARGET}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>"
    LINKER_LANGUAGE CXX
)

target_compile_definitions(${PROJECT_TEST_TARGET} PRIVATE _AMD64_)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_link_options(${PROJECT_TEST_TARGET} BEFORE PUBLIC -fsanitize=address)
endif()

target_link_directories(${PROJECT_TEST_TARGET} PRIVATE "lib")
target_link_libraries(${PROJECT_TEST_TARGET} ${PROJECT_TARGET} ${PROJECT_LIBRARIES} luajit)
if (WIN32)
    target_link_libraries(${PROJECT_TEST_TARGET} lua51)
endif()
target_include_directories(${PROJECT_TEST_TARGET} PUBLIC "include")
target_include_directories(${PROJECT_TEST_TARGET} PRIVATE "src")
target_include_directories(${PROJECT_TEST_TARGET} PRIVATE "tests")
