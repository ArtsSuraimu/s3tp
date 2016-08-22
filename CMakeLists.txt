cmake_minimum_required(VERSION 3.4)
project(s3tp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

#set(SOURCE_FILES
#    core/constants.h
#    core/debug.c
#    core/debug.h
#    core/init.c
#    core/init.h
#    core/MessageSampler.c
#    core/MessageSampler.h
#    core/queue.c
#    core/queue.h
#    core/recording.c
#    core/s3tp.c
#    core/s3tp.h
#    core/s3tp_types.h
#    core/utilities.c
#    core/utilities.h
#    drivers/eth.c
#    drivers/eth.h
#    LICENSE
#    README.md)
set(SOURCE_FILES
        core/constants.h
        core/debug.c
        core/debug.h
        core/queue.c
        core/queue.h
        core/s3tp_types.h)

add_executable(s3tp ${SOURCE_FILES})