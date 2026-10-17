cmake_minimum_required(VERSION 3.20)

if (NOT DEFINED OUTPUT_DIR OR OUTPUT_DIR STREQUAL "")
    message(FATAL_ERROR "OUTPUT_DIR must name the runtime assets directory")
endif()

get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
file(COPY "${PROJECT_ROOT}/lib/recomp-theme/assets/" DESTINATION "${OUTPUT_DIR}")
file(COPY "${PROJECT_ROOT}/assets/" DESTINATION "${OUTPUT_DIR}")
