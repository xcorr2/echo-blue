#REF: https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/basic/threads/prj.conf 
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gps)


target_sources(app PRIVATE 
    src/main.c
    src/gps.c
)

target_include_directories(app PRIVATE src)