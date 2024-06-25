cmake_minimum_required(VERSION 3.0)

project(A12_group_chat LANGUAGES C)

include_directories(include)

add_executable(server src/server.c)
add_executable(client src/client.c)

target_link_libraries(client pthread)
