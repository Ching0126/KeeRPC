cmake_minimum_required(VERSION 3.0)

project(KeeRPC)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)

include_directories(${Protobuf_INCLUDE_DIRS})

# 把后续参数全部装进第一个参数中
set(LIBS
    protobuf
    pthread
    zookeeper_mt
    muduo_net
    muduo_base
    glog)

add_subdirectory(src)
add_subdirectory(example)


