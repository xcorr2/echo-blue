cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(m5stack_core2_haptics)

target_sources(app PRIVATE 
    src/main.c
    src/haptics.c
)

# Add include directories
target_include_directories(app PRIVATE src)