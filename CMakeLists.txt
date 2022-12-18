cmake_minimum_required (VERSION 3.8)

project ("Ketl")

# Include sub-projects.
add_subdirectory ("src")

file(GLOB_RECURSE EEl_HEADERS CONFIGURE_DEPENDS include/*.h include/*.hpp)

add_library (ketl STATIC ${KETL_SOURCES} ${EEl_HEADERS})
set_property(TARGET ketl PROPERTY CXX_STANDARD 20)

set_target_properties(ketl
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/$<CONFIG>"
)

target_compile_definitions(ketl PRIVATE _AMD64_)

#if(MSVC)
#    string(REGEX REPLACE "/W[1-3]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
#else()
#    add_compile_options(-Wall -Wextra -pedantic -Werror)
#endif()

target_include_directories(ketl PUBLIC "include")
target_include_directories(ketl PRIVATE "src")

###############################

add_subdirectory ("tests")

add_executable (test ${KETL_TEST_SOURCES})
set_property(TARGET test PROPERTY CXX_STANDARD 20)

add_dependencies(test ketl)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT test)

target_compile_definitions(test PRIVATE _AMD64_)

target_link_directories(test PRIVATE "lib")
target_link_libraries(test ketl)
target_include_directories(test PUBLIC "include")
target_include_directories(test PRIVATE "src")
target_include_directories(test PRIVATE "tests")
