
project(ingler)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_VERBOSE_MAKEFILE true)
set(CMAKE_C_COMPILER clang)
#set(CMAKE_C_FLAGS "-Wall -Wextra -Werror -O3")
set(CMAKE_C_FLAGS "-Wall -Werror -O3")

subdirs(src rc.d)

# vim: tabstop=4 shiftwidth=4 expandtab softtabstop=4
