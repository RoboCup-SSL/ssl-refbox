cmake_minimum_required(VERSION 2.8.2)
project(ssl-refbox)

find_package(PkgConfig)
pkg_check_modules(GTKMM gtkmm-2.4)

include_directories(${GTKMM_INCLUDE_DIRS})

set(SOURCES
    refereemm.cc
    gamecontrol.cc
    gameinfo.cc
    udp_broadcast.cc
    dialog_gameover.cc
    settings.cc
    logging.cc
    frame_log.cc
)

set_source_files_properties(sound.c PROPERTIES LANGUAGE CXX)
file(COPY ${CMAKE_SOURCE_DIR}/referee.conf DESTINATION ${CMAKE_BINARY_DIR})

add_executable(sslrefbox WIN32 ${SOURCES})
target_link_libraries(sslrefbox ${GTKMM_LIBRARIES})
