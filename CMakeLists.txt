cmake_minimum_required (VERSION 3.10)

project ("ketl")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROJECT_NAME "ketl")
set(PROJECT_LIB_TARGET "${PROJECT_NAME}_lib")

set(PROJECT_SANITIZE_OPTION -fsanitize=address)

###############################

if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/we4013 /we4020 /we4029 /we4047 /we4098 /we4133 /we4244 /we4715 /we4716)
else()
	add_compile_options(-Wall -Wextra -Werror -pedantic)

    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
	    add_compile_options(-Wno-switch-unreachable)
    endif() 

    if (WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE MATCHES "Debug")
        add_compile_options(${PROJECT_SANITIZE_OPTION})
    endif()
endif()

enable_language(ASM)

################################

file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS include/*.h)

add_subdirectory ("src")

foreach(SOURCE_ASM IN LISTS PROJECT_SOURCES_ASM)
    set_property(SOURCE ${SOURCE_ASM} APPEND PROPERTY COMPILE_OPTIONS "-x" "assembler-with-cpp")
endforeach()

add_library (${PROJECT_LIB_TARGET} STATIC ${PROJECT_SOURCES} ${PROJECT_HEADERS})
set_property(TARGET ${PROJECT_LIB_TARGET} PROPERTY C_STANDARD 11)

set_property(TARGET ${PROJECT_LIB_TARGET} PROPERTY OUTPUT_NAME ${PROJECT_NAME})

set_target_properties(${PROJECT_LIB_TARGET}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/$<CONFIG>"
    LINKER_LANGUAGE C
)

target_compile_definitions(${PROJECT_LIB_TARGET} PRIVATE _AMD64_)

if (WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE MATCHES "Debug")
    target_link_options(${PROJECT_LIB_TARGET} BEFORE PUBLIC ${PROJECT_SANITIZE_OPTION})
endif()

target_include_directories(${PROJECT_LIB_TARGET} PUBLIC "include")
target_include_directories(${PROJECT_LIB_TARGET} PRIVATE "src")

##################################

set(PROJECT_C_EXE_TARGET "${PROJECT_NAME}_c_exe")

add_executable (${PROJECT_C_EXE_TARGET} "src/ketl.cpp")
set_property(TARGET ${PROJECT_C_EXE_TARGET} PROPERTY CXX_STANDARD 20)

set_property(TARGET ${PROJECT_C_EXE_TARGET} PROPERTY OUTPUT_NAME cketl)

add_dependencies(${PROJECT_C_EXE_TARGET} ${PROJECT_LIB_TARGET})

set_target_properties(${PROJECT_C_EXE_TARGET}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>"
    LINKER_LANGUAGE CXX
)

target_compile_definitions(${PROJECT_C_EXE_TARGET} PRIVATE _AMD64_)

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE MATCHES "Debug")
    target_link_options(${PROJECT_C_EXE_TARGET} BEFORE PUBLIC ${PROJECT_SANITIZE_OPTION})
endif()

target_link_directories(${PROJECT_C_EXE_TARGET} PRIVATE "lib")
target_link_libraries(${PROJECT_C_EXE_TARGET} ${PROJECT_LIB_TARGET} ${PROJECT_LIBRARIES})
target_include_directories(${PROJECT_C_EXE_TARGET} PUBLIC "include")
target_include_directories(${PROJECT_C_EXE_TARGET} PRIVATE "src")

##################################

add_subdirectory ("src_ketl")
string(REPLACE ".ktl" ".s" PROJECT_KETL_SOURCES_ASM "${PROJECT_KETL_SOURCES}")

get_target_property(PROJECT_C_KETL_COMPILER_NAME ${PROJECT_C_EXE_TARGET} OUTPUT_NAME)
get_target_property(PROJECT_C_KETL_COMPILER_DIR ${PROJECT_C_EXE_TARGET} RUNTIME_OUTPUT_DIRECTORY)

add_custom_command(
    OUTPUT ${PROJECT_KETL_SOURCES_ASM}
    COMMAND "${PROJECT_C_KETL_COMPILER_DIR}/${PROJECT_C_KETL_COMPILER_NAME}"
    ARGS -S src_ketl
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS ${PROJECT_KETL_SOURCES} ${PROJECT_C_EXE_TARGET}
    COMMENT "Compiling ketl compiler"
)

##################################

set(PROJECT_EXE_TARGET "${PROJECT_NAME}_exe")

foreach(SOURCE_ASM IN LISTS PROJECT_KETL_SOURCES_ASM)
    set_property(SOURCE ${SOURCE_ASM} APPEND PROPERTY COMPILE_OPTIONS "-x" "assembler-with-cpp")
endforeach()

add_executable (${PROJECT_EXE_TARGET} ${PROJECT_KETL_SOURCES_ASM})
set_property(TARGET ${PROJECT_EXE_TARGET} PROPERTY OUTPUT_NAME ketl)

set_target_properties(${PROJECT_EXE_TARGET}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>"
    LINKER_LANGUAGE CXX
)

target_compile_definitions(${PROJECT_EXE_TARGET} PRIVATE _AMD64_)

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE MATCHES "Debug")
    target_link_options(${PROJECT_EXE_TARGET} BEFORE PUBLIC ${PROJECT_SANITIZE_OPTION})
endif()

target_link_directories(${PROJECT_EXE_TARGET} PRIVATE "lib")
target_link_libraries(${PROJECT_EXE_TARGET} ${PROJECT_LIBRARIES})
target_include_directories(${PROJECT_EXE_TARGET} PUBLIC "include")
target_include_directories(${PROJECT_EXE_TARGET} PRIVATE "src")

###############################

set(PROJECT_TEST_TARGET "${PROJECT_NAME}_test")

add_subdirectory ("tests")

add_executable (${PROJECT_TEST_TARGET} ${PROJECT_TEST_SOURCES}
    )
set_property(TARGET ${PROJECT_TEST_TARGET} PROPERTY CXX_STANDARD 20)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_TEST_TARGET})

add_dependencies(${PROJECT_TEST_TARGET} ${PROJECT_LIB_TARGET})

set_target_properties(${PROJECT_TEST_TARGET}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>"
    LINKER_LANGUAGE CXX
)

target_compile_definitions(${PROJECT_TEST_TARGET} PRIVATE _AMD64_)

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE MATCHES "Debug")
    target_link_options(${PROJECT_TEST_TARGET} BEFORE PUBLIC ${PROJECT_SANITIZE_OPTION})
endif()

target_link_directories(${PROJECT_TEST_TARGET} PRIVATE "lib")
target_link_libraries(${PROJECT_TEST_TARGET} ${PROJECT_LIB_TARGET} ${PROJECT_LIBRARIES})
if (WIN32)
    target_link_libraries(${PROJECT_TEST_TARGET})
endif()
target_include_directories(${PROJECT_TEST_TARGET} PUBLIC "include")
target_include_directories(${PROJECT_TEST_TARGET} PRIVATE "src")
target_include_directories(${PROJECT_TEST_TARGET} PRIVATE "tests")
