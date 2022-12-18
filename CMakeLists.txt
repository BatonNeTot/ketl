cmake_minimum_required (VERSION 3.8)

project ("Ketl")

# Include sub-projects.
add_subdirectory ("src")

file(GLOB_RECURSE KETL_HEADERS CONFIGURE_DEPENDS include/*.h include/*.hpp)

add_library (ketl STATIC ${KETL_SOURCES} ${KETL_HEADERS} "NatvisFile.natvis")
set_property(TARGET ketl PROPERTY CXX_STANDARD 20)
set_source_files_properties("NatvisFile.natvis" PROPERTIES HEADER_FILE_ONLY true)

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

add_subdirectory ("dependencies")

#add_dependencies(ketl ebnf)

#target_link_libraries(ketl ebnf)

target_include_directories(ketl PUBLIC "include")
target_include_directories(ketl PRIVATE "src")

###############################

add_subdirectory ("tests")

add_executable (ketl_test ${KETL_TEST_SOURCES})
set_property(TARGET ketl_test PROPERTY CXX_STANDARD 20)

add_dependencies(ketl_test ketl lua)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ketl_test)

target_link_libraries(ketl_test lua)

target_compile_definitions(ketl_test PRIVATE _AMD64_)

target_link_directories(ketl_test PRIVATE "lib")
target_link_libraries(ketl_test ketl lua)
target_include_directories(ketl_test PUBLIC "include")
target_include_directories(ketl_test PRIVATE "src")
target_include_directories(ketl_test PRIVATE "tests")
