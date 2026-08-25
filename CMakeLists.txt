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

    if (WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(${PROJECT_SANITIZE_OPTION})
    endif()
endif()

enable_language(ASM)

################################

add_subdirectory ("src")
string(REPLACE ".ktl" ".s" PROJECT_KETL_SOURCES_ASM "${PROJECT_KETL_SOURCES}")

set(PROJECT_INSTALLED_PATH "${CMAKE_INSTALL_PREFIX}/bin/ketl")
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PROJECT_INSTALLED_PATH "${PROJECT_INSTALLED_PATH}.exe")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(PROJECT_DEBUG_FLAG "-g")
endif()

add_custom_command(
    OUTPUT ${PROJECT_KETL_SOURCES_ASM}
    COMMAND ketl
    ARGS -S src ${PROJECT_DEBUG_FLAG}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS ${PROJECT_KETL_SOURCES} ${PROJECT_C_EXE_TARGET} "${PROJECT_INSTALLED_PATH}"
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

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(${PROJECT_EXE_TARGET} BEFORE PUBLIC ${PROJECT_SANITIZE_OPTION})
endif()

###############################

install(TARGETS ${PROJECT_EXE_TARGET})

###############################

string(REPLACE "src" "src_out" PROJECT_KETL_SOURCES_ASM_TEST "${PROJECT_KETL_SOURCES_ASM}")

get_target_property(PROJECT_KETL_COMPILER_NAME ${PROJECT_EXE_TARGET} OUTPUT_NAME)
get_target_property(PROJECT_KETL_COMPILER_DIR ${PROJECT_EXE_TARGET} RUNTIME_OUTPUT_DIRECTORY)

add_custom_command(
    OUTPUT ${PROJECT_KETL_SOURCES_ASM_TEST}
    COMMAND "${PROJECT_KETL_COMPILER_DIR}/${PROJECT_KETL_COMPILER_NAME}"
    ARGS -S src -o src_out -g
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS ${PROJECT_KETL_SOURCES} ${PROJECT_EXE_TARGET}
    COMMENT "Compiling ketl compiler"
)

################################

set(PROJECT_TEST_TARGET "${PROJECT_NAME}_test")

foreach(SOURCE_ASM IN LISTS PROJECT_KETL_SOURCES_ASM_TEST)
    set_property(SOURCE ${SOURCE_ASM} APPEND PROPERTY COMPILE_OPTIONS "-x" "assembler-with-cpp")
endforeach()

add_executable (${PROJECT_TEST_TARGET} ${PROJECT_KETL_SOURCES_ASM_TEST})
set_property(TARGET ${PROJECT_TEST_TARGET} PROPERTY OUTPUT_NAME ketl_test)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_TEST_TARGET})

add_dependencies(${PROJECT_TEST_TARGET} ${PROJECT_EXE_TARGET})

set_target_properties(${PROJECT_TEST_TARGET}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>"
    LINKER_LANGUAGE CXX
)

target_compile_definitions(${PROJECT_TEST_TARGET} PRIVATE _AMD64_)

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(${PROJECT_TEST_TARGET} BEFORE PUBLIC ${PROJECT_SANITIZE_OPTION})
endif()

target_link_directories(${PROJECT_TEST_TARGET} PRIVATE "lib")
target_link_libraries(${PROJECT_TEST_TARGET} 
    #${PROJECT_LIB_TARGET} 
    ${PROJECT_LIBRARIES}
    )
if (WIN32)
    target_link_libraries(${PROJECT_TEST_TARGET})
endif()
