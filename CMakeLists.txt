cmake_minimum_required(VERSION 2.6)
project(ssl_refbox)

add_definitions(-std=gnu++0x -Wall -Wextra -Wundef -O2 -g)
add_definitions(-DPROTOBUF_INLINE_NOT_IN_HEADERS=0)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})

find_package(PkgConfig)
pkg_check_modules(GTKMM gtkmm-2.4)
link_directories(${GTKMM_LIBRARY_DIRS})
include_directories(${GTKMM_INCLUDE_DIRS})

find_package(Protobuf REQUIRED)
if (EXISTS ${PROTOBUF_PROTOC_EXECUTABLE})
    message(STATUS "Found PROTOBUF Compiler: ${PROTOBUF_PROTOC_EXECUTABLE}")
else ()
    message(FATAL_ERROR "Could not find PROTOBUF Compiler")
endif ()
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS game_event.proto rcon.proto referee.proto savestate.proto)

include_directories(
        ${PROJECT_BINARY_DIR}
        ${PROTOBUF_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}
)

set(SOURCE_FILES
        addrinfolist.cc
        configuration.cc
        exception.cc
        gamecontroller.cc
        legacypublisher.cc
        logger.cc
        main.cc
        mainwindow.cc
        protobufpublisher.cc
        rconsrv.cc
        savegame.cc
        socket.cc
        teams.cc
        timing.cc
        udpbroadcast.cc)

add_executable(sslrefbox ${SOURCE_FILES} ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(sslrefbox ${GTKMM_LIBRARIES} ${PROTOBUF_LIBRARIES})