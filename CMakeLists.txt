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
set(PICO_BOARD pimoroni_pga2350 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(emuz80_pico2 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(emuz80_pico2 emuz80_pico2.c )

pico_set_program_name(emuz80_pico2 "emuz80_pico2")
pico_set_program_version(emuz80_pico2 "0.1")

# copy_to_ram means bootloader copies binaries to RAM and execute it
pico_set_binary_type(emuz80_pico2 copy_to_ram)

# Generate PIO header
pico_generate_pio_header(emuz80_pico2 ${CMAKE_CURRENT_LIST_DIR}/blink.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(emuz80_pico2 0)
pico_enable_stdio_usb(emuz80_pico2 1)

# compiler options
target_compile_options(emuz80_pico2 PUBLIC -Ofast)

# Add the standard library to the build
target_link_libraries(emuz80_pico2
        pico_stdlib
        pico_multicore)

# Add the standard include files to the build
target_include_directories(emuz80_pico2 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(emuz80_pico2 
        hardware_pio
        )

pico_add_extra_outputs(emuz80_pico2)

