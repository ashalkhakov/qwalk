cmake_minimum_required(VERSION 3.10)
project(qwalk C)
file(GLOB SOURCES *.c *.h)
file(GLOB SOURCES_EXCLUDE viewer.c v_font.c)
list(REMOVE_ITEM SOURCES ${SOURCES_EXCLUDE})

add_executable(qwalk_converter ${SOURCES})
#target_link_libraries(qwalk_converter PRIVATE BspcLib)
target_link_libraries(qwalk_converter PRIVATE -lm -ldl)
