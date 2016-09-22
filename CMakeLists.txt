cmake_minimum_required(VERSION 3.4)
project(s3tp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CONNECTOR_SRC_FILES
        connector/S3tpConnector.cpp
        connector/S3tpConnector.h
        core/S3tpShared.h
        core/S3tpShared.cpp
        core/Logger.h
        connector/S3tpCallback.h)

add_library(s3tp SHARED ${CONNECTOR_SRC_FILES})

target_link_libraries(s3tp pthread)

install(DIRECTORY connector/ DESTINATION include/s3tp/s3tp/connector)
install(DIRECTORY core/ DESTINATION include/s3tp/s3tp/core)
install(TARGETS s3tp LIBRARY DESTINATION lib/s3tp)
install(FILES s3tp-config.cmake DESTINATION lib/s3tp)