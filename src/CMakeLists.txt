file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)
file(GLOB PROTO_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.pb.cc)

add_library(keerpc_core STATIC ${SRC_FILES} ${PROTO_SRCS})

target_link_libraries(keerpc_core PUBLIC protobuf)

target_include_directories(keerpc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_options(keerpc_core PRIVATE -Wall)