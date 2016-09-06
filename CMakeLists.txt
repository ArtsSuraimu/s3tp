cmake_minimum_required(VERSION 3.4)
project(s3tp)

# packages
find_package(moveio REQUIRED)
include_directories(${MOVEIO_INCLUDE_DIRS})

find_package(trctrl REQUIRED)
include_directories(${TRCTRL_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
        connector/s3tp_shared.h
        connector/s3tp_shared.cpp
        core/constants.h
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
        core/Client.cpp
        core/Client.h
        core/ClientInterface.h
        core/S3tpStatusInterface.h
        core/PriorityComparator.h
        test/s3tp_start.cpp)

add_executable(s3tp ${SOURCE_FILES})

target_link_libraries(s3tp ${MOVEIO_LIBRARY})
target_link_libraries(s3tp ${TRCTRL_LIBRARY})
target_link_libraries(s3tp pthread)

set(CONNECTOR_SRC_FILES
        connector/s3tp_connector.cpp
        connector/s3tp_connector.h
        connector/s3tp_shared.h
        connector/s3tp_shared.cpp
        connector/connector_main.cpp)

add_executable(s3tp_conn ${CONNECTOR_SRC_FILES})

target_link_libraries(s3tp_conn pthread)