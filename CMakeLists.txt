cmake_minimum_required(VERSION 3.4)
project(s3tp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
        connector/s3tp_shared.h
        connector/s3tp_shared.cpp
        core/constants.h
        core/debug.cpp
        core/debug.h
        core/PriorityQueue.cpp
        core/PriorityQueue.h
        core/s3tp_types.h
        core/TxModule.cpp
        core/TxModule.h
        core/RxModule.cpp
        core/RxModule.h
        core/Buffer.cpp
        core/Buffer.h
        core/s3tp_main.cpp
        core/s3tp_main.h
        core/SimpleQueue.h
        core/s3tp_daemon.cpp
        core/s3tp_daemon.h
        core/utilities.h
        core/utilities.cpp
        core/client.cpp
        core/client.h
        core/connection_listener.h)

set(CONNECTOR_SRC_FILES
        connector/s3tp_connector.cpp
        connector/s3tp_connector.h
        connector/s3tp_shared.h
        connector/s3tp_shared.cpp
        connector/connector_main.cpp)


add_executable(s3tp ${SOURCE_FILES})
add_executable(s3tp_conn ${CONNECTOR_SRC_FILES})