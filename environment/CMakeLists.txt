cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(adxl345_driver)

# Add the local headers directory to the include path
zephyr_include_directories(headers)
zephyr_include_directories(.)

# Link the source files
target_sources(app PRIVATE 
    main.c
    adxl345_setup.c
    adxl345_read.c
    mock_sensor.c
)