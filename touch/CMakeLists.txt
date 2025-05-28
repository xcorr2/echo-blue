#REF: https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/basic/threads/prj.conf 
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(touchtest)

# Add source files from both gui and haptics modules
target_sources(app PRIVATE 
    src/main.c
)

# Add include directories for both modules
target_include_directories(app PRIVATE 
    src
)