# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

set(CERTIFICATE_PATH_VAR "CONFIG_TLS_CERTIFICATE_PATH")

if(NOT DEFINED ${CERTIFICATE_PATH_VAR})
  message(WARNING "The certificate path variable '${CERTIFICATE_PATH_VAR}' is not defined no certificate include file will be generated")
  return()
endif()

get_filename_component(certificate_file ${CMAKE_SOURCE_DIR}/${${CERTIFICATE_PATH_VAR}} ABSOLUTE)

if(NOT EXISTS ${certificate_file})
  message(ERROR "The file path '${certificate_file}' specified in '${CERTIFICATE_PATH_VAR}' does not exist, provide a valid file or remove the option")
  return()
endif()

generate_inc_file_for_target(
    app
    ${certificate_file}
    ${ZEPHYR_BINARY_DIR}/include/generated/ca_certificate.inc)
