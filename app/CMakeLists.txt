project(app)
file(GLOB_RECURSE SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)
add_executable(server server.cc)
add_executable(client client.cc)

target_include_directories(server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(server PUBLIC rdma-lib ${PUBLIC_LIB})

target_include_directories(client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(client PUBLIC rdma-lib ${PUBLIC_LIB})