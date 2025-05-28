#REF: https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/basic/threads/prj.conf 
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(m5stack_gui_with_haptics)

# Add source files from both gui and haptics modules
target_sources(app PRIVATE 
    src/ui.c
    ../haptics/src/haptics.c
    ../gui/src/display.c
)

# Add include directories for both modules
target_include_directories(app PRIVATE 
    src
    ../haptics/src
    ../gui/src
)