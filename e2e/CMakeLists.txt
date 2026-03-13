# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

function(concat_if_exists FILE_PATH STRING_NAME)
    # Check if the file exists
    file(GLOB FILE_TO_ADD ${FILE_PATH})

    # If the file exists, add it to the list
    if(EXISTS ${FILE_TO_ADD})
        set(${STRING_NAME} "${${STRING_NAME}};${FILE_PATH}" PARENT_SCOPE)
    endif()
endfunction()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# add some custom functions from the sample directory
set(SAMPLE_DIR "${CMAKE_CURRENT_LIST_DIR}/../samples/edgehog_app")

# add the sample specific private configuration
concat_if_exists(${CMAKE_SOURCE_DIR}/private.conf EXTRA_CONF_FILE)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(e2e_test)

# add sources and includes from the sample dir
target_sources(app PRIVATE ${SAMPLE_DIR}/src/eth.c)
target_include_directories(app PRIVATE ${SAMPLE_DIR}/src/include)

# handle generating certificate inc file
include(certificate.cmake)

# add astarte private headers
target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/../lib/edgehog_device/include)

# add sources
FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE include)
