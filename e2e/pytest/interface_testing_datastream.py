# (C) Copyright 2026, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

import logging
from datetime import datetime
from typing import Any

from configuration import Configuration
from interface_testing import InterfaceTesting
from http_requests import (
    http_prepare_transmit_data,
    http_post_server_data,
    http_decode_received_data,
    http_get_server_data,
)
from test_utilities import encode_shell_bson

logger = logging.getLogger(__name__)


class InterfaceTestingDatastreamTestElement:
    def __init__(self, path: str, value: Any, timestamp: datetime | None = None):
        self.path = path
        self.timestamp = timestamp
        self.value = value


class InterfaceTestingDatastream(InterfaceTesting[InterfaceTestingDatastreamTestElement]):

    def __init__(
        self, interface_name: str, test_elements: list[InterfaceTestingDatastreamTestElement]
    ):
        super().__init__(interface_name)
        self.test_elements = test_elements

    def _send_data_to_the_server(
        self, cfg: Configuration, test_element: InterfaceTestingDatastreamTestElement
    ):
        payload = http_prepare_transmit_data(self.interface, test_element.path, test_element.value)
        http_post_server_data(cfg, self.interface_name, test_element.path, payload)

    def _check_data_received_by_the_server(
        self, cfg: Configuration, test_element: InterfaceTestingDatastreamTestElement
    ) -> bool:
        received_payload = http_get_server_data(cfg, self.interface_name, limit=1)
        logger.info(f"Server individual data {received_payload}")

        try:
            last_path_segment = test_element.path.split("/")[-1]
            unpacked_payload = received_payload[last_path_segment]["value"]
            received_value = http_decode_received_data(
                self.interface, test_element.path, unpacked_payload
            )
        except KeyError as e:
            logger.error(f"KeyError while accessing the individual {e}")
            return False

        logger.info(
            f"Interface '{self.interface_name}', path '{test_element.path}': expected {test_element.value} got {received_value}"
        )
        return cfg.log_only or (received_value == test_element.value)

    def _get_command_for_the_device(
        self, base_command: str, test_element: InterfaceTestingDatastreamTestElement
    ) -> str:
        command = (
            f"{base_command} individual {self.interface_name} {test_element.path}"
            + f" {encode_shell_bson(test_element.value)}"
        )
        if test_element.timestamp:
            unix_t = int(test_element.timestamp.timestamp() * 1000)
            command += f" {unix_t}"
        return command

    def _get_individual_test_elements(self) -> list[InterfaceTestingDatastreamTestElement]:
        return self.test_elements
