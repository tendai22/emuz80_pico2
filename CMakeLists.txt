# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
#set(PICO_BOARD pimoroni_pga2350 CACHE STRING "Board type")
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(rp2450a_test C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(rp2450a_test emuz80_pico2.c )

pico_set_program_name(rp2450a_test "rp2450a_test")
pico_set_program_version(rp2450a_test "0.1")

# copy_to_ram means bootloader copies binaries to RAM and execute it
pico_set_binary_type(rp2450a_test copy_to_ram)

# Generate PIO header
pico_generate_pio_header(rp2450a_test ${CMAKE_CURRENT_LIST_DIR}/blink.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(rp2450a_test 0)
pico_enable_stdio_usb(rp2450a_test 1)

# compiler options
target_compile_options(rp2450a_test PUBLIC -Ofast)

# Add the standard library to the build
target_link_libraries(rp2450a_test
        pico_stdlib
        pico_multicore)

# Add the standard include files to the build
target_include_directories(rp2450a_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(rp2450a_test 
        hardware_pio
        )

pico_add_extra_outputs(rp2450a_test)

