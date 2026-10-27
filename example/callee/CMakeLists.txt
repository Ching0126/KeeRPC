add_executable(server Kserver.cc ../user.pb.cc)
target_link_libraries(server keerpc_core ${LIBS})