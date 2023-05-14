cmake_minimum_required(VERSION 3.23)
project(Filesystem_in_Userspace C)

set(CMAKE_C_STANDARD 99)
include_directories(/usr/local/include/fuse3/)
add_executable(Filesystem_in_Userspace main.c)
target_link_libraries(Filesystem_in_Userspace fuse3 pthread)
