add_executable(client Kclient.cc ../user.pb.cc)
target_link_libraries(client keerpc_core ${LIBS})