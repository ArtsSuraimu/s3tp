cmake_minimum_required(VERSION 3.4)
project(s3tp)

# packages
find_package(moveio REQUIRED)
include_directories(${MOVEIO_INCLUDE_DIRS})

find_package(trctrl REQUIRED)
include_directories(${TRCTRL_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
        core/s3tp_shared.h
        core/s3tp_shared.cpp
        core/Constants.h
        core/PriorityQueue.h
        core/CommonTypes.h
        core/TxModule.cpp
        core/TxModule.h
        core/RxModule.cpp
        core/RxModule.h
        core/Buffer.cpp
        core/Buffer.h
        core/S3TP.cpp
        core/S3TP.h
        core/SimpleQueue.h
        core/TransportDaemon.cpp
        core/TransportDaemon.h
        core/utilities.h
        core/utilities.cpp
        core/Client.cpp
        core/Client.h
        core/ClientInterface.h
        core/StatusInterface.h
        core/PriorityComparator.h
        test/s3tp_start.cpp
        core/Logger.h)

add_executable(s3tp ${SOURCE_FILES})

target_link_libraries(s3tp ${MOVEIO_LIBRARY})
target_link_libraries(s3tp ${TRCTRL_LIBRARY})
target_link_libraries(s3tp pthread)

set(CONNECTOR_SRC_FILES
        connector/S3tpConnector.cpp
        connector/S3tpConnector.h
        core/s3tp_shared.h
        core/s3tp_shared.cpp
        test/connector_main.cpp
        core/Logger.h
        connector/S3tpCallback.h)

add_executable(s3tp_conn ${CONNECTOR_SRC_FILES})

target_link_libraries(s3tp_conn pthread)