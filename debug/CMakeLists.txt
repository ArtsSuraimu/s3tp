cmake_minimum_required(VERSION 3.4)
project(s3tp)

# packages
find_package(moveio REQUIRED)
include_directories(${MOVEIO_INCLUDE_DIRS})

find_package(trctrl REQUIRED)
include_directories(${TRCTRL_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

#core
set(DEBUG_CORE_FILES
        ../core/S3tpShared.h
        ../core/S3tpShared.cpp
        ../core/Logger.h
        ../core/Constants.h
        ../core/PriorityQueue.h
        ../core/CommonTypes.h
        ../core/TxModule.cpp
        ../core/TxModule.h
        ../core/RxModule.cpp
        ../core/RxModule.h
        ../core/Buffer.cpp
        ../core/Buffer.h
        ../core/S3TP.cpp
        ../core/S3TP.h
        ../core/SimpleQueue.h
        ../core/TransportDaemon.cpp
        ../core/TransportDaemon.h
        ../core/utilities.h
        ../core/utilities.cpp
        ../core/Client.cpp
        ../core/Client.h
        ../core/ClientInterface.h
        ../core/StatusInterface.h
        ../core/TransportInterface.h
        ../core/PolicyActor.h
        ../test/s3tp_start.cpp)

add_executable(s3tpd ${DEBUG_CORE_FILES})

target_link_libraries(s3tpd ${MOVEIO_LIBRARY})
target_link_libraries(s3tpd ${TRCTRL_LIBRARY})
target_link_libraries(s3tpd pthread)

#connector
set(DEBUG_CONNECTOR_SRC
        ../core/S3tpShared.h
        ../core/S3tpShared.cpp
        ../core/Logger.h
        ../connector/S3tpCallback.h
        ../connector/S3tpConnector.h
        ../connector/S3tpConnector.cpp
        ../test/connector_main.cpp)

add_executable(s3tp_conn ${DEBUG_CONNECTOR_SRC})

target_link_libraries(s3tp_conn pthread)

install(TARGETS s3tp_conn RUNTIME DESTINATION bin)