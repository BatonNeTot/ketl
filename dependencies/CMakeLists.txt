cmake_minimum_required (VERSION 3.8)

file(GLOB DEPENDENCIES_DIRECTORIES LIST_DIRECTORIES true *)
file(GLOB DEPENDENCIES_FILES LIST_DIRECTORIES false *)

foreach(DEPENDENCIES_FILE ${DEPENDENCIES_FILES})
    list(REMOVE_ITEM DEPENDENCIES_DIRECTORIES ${DEPENDENCIES_FILE})
endforeach()

foreach(DEPENDENCIES_DIRECTORY ${DEPENDENCIES_DIRECTORIES})
    message("${DEPENDENCIES_DIRECTORY}")
    add_subdirectory(${DEPENDENCIES_DIRECTORY})
    if(EXISTS "${DEPENDENCIES_DIRECTORY}/include")
        target_include_directories(ketl PRIVATE "${DEPENDENCIES_DIRECTORY}/include")
    endif()
    if(EXISTS "${DEPENDENCIES_DIRECTORY}/lib")
        target_link_directories(ketl PRIVATE "${DEPENDENCIES_DIRECTORY}/lib")
    endif()
endforeach()
