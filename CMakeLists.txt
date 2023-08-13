cmake_minimum_required (VERSION 3.8)

project ("ketl")

set(PROJECT_TARGET "ketl")

if(MSVC)
    add_compile_options(/we4013 /we4020 /we4029 /we4047 /we4133 /we4244 /we4715 /we4716)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

add_subdirectory ("src")

file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS include/*.h include/*.hpp)

add_library (${PROJECT_TARGET} STATIC ${PROJECT_SOURCES} ${PROJECT_HEADERS})
set_property(TARGET ${PROJECT_TARGET} PROPERTY C_STANDARD 99)
set_source_files_properties("NatvisFile.natvis" PROPERTIES HEADER_FILE_ONLY true)

set_target_properties(${PROJECT_TARGET}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/$<CONFIG>"
)

add_subdirectory ("dependencies")
target_link_libraries(${PROJECT_TARGET} ${PROJECT_LIBRARIES})

target_compile_definitions(${PROJECT_TARGET} PRIVATE _AMD64_)

target_include_directories(${PROJECT_TARGET} PUBLIC "include")
target_include_directories(${PROJECT_TARGET} PRIVATE "src")

###############################

set(PROJECT_TEST_TARGET "${PROJECT_TARGET}_test")

add_subdirectory ("tests")

add_executable (${PROJECT_TEST_TARGET} ${PROJECT_TEST_SOURCES})
set_property(TARGET ${PROJECT_TEST_TARGET} PROPERTY CXX_STANDARD 20)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_TEST_TARGET})

add_dependencies(${PROJECT_TEST_TARGET} ${PROJECT_TARGET})

add_subdirectory ("dependencies_tests")

target_compile_definitions(${PROJECT_TEST_TARGET} PRIVATE _AMD64_)

target_link_directories(${PROJECT_TEST_TARGET} PRIVATE "lib")
target_link_libraries(${PROJECT_TEST_TARGET} ${PROJECT_TARGET} ${PROJECT_LIBRARIES} lua51 luajit)
target_include_directories(${PROJECT_TEST_TARGET} PUBLIC "include")
target_include_directories(${PROJECT_TEST_TARGET} PRIVATE "src")
target_include_directories(${PROJECT_TEST_TARGET} PRIVATE "tests")
