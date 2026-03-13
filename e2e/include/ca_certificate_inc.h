/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CA_CERTIFICATE_INC_H
#define CA_CERTIFICATE_INC_H

// ca_certificate.inc is generated at build time by certificate.cmake
static const unsigned char ca_certificate_root[] = {
#include "ca_certificate.inc"
    0x00
};

#endif /* CA_CERTIFICATE_INC_H */
