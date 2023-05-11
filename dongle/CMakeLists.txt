set(BOARD nrf52840dongle_nrf52840)
set(DTC_OVERLAY_FILE dtc_shell.overlay)

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dongle)

#Add Include Directories
include_directories(
                        ../dongle_drivers/
                        )

#Add Target Sources
target_sources(app PRIVATE 
                    src/main.c 
                    ../dongle_drivers/dongle.c)