cmake_minimum_required (VERSION 3.8)

project ("ketl_core")

set(PROJECT_TARGET "ketl_core")

if(MSVC)
    add_compile_options(/we4013 /we4020 /we4029 /we4047 /we4133 /we4244 /we4715 /we4716)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

add_subdirectory ("src")

file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS include/*.h include/*.hpp)

add_library (${PROJECT_TARGET} STATIC ${PROJECT_SOURCES} ${PROJECT_HEADERS})
set_property(TARGET ${PROJECT_TARGET} PROPERTY C_STANDARD 11)

set_target_properties(${PROJECT_TARGET}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/$<CONFIG>"
)

target_compile_definitions(${PROJECT_TARGET} PRIVATE _AMD64_)

target_include_directories(${PROJECT_TARGET} PUBLIC "include")
target_include_directories(${PROJECT_TARGET} PRIVATE "src")
