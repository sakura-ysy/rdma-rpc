project(rdma-lib)
file(GLOB_RECURSE SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)
add_library(${PROJECT_NAME} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(${PROJECT_NAME} PUBLIC IBVerbs::verbs RDMA::RDMAcm pthread PkgConfig::libevent)
