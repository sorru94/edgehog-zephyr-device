# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

from datetime import datetime
from typing import Any
import logging

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


class InterfaceTestingAggregateTestElement:
    def __init__(self, entries: dict[str, Any]):
        self.entries = entries


class InterfaceTestingAggregate(InterfaceTesting[InterfaceTestingAggregateTestElement]):
    def __init__(
        self,
        interface_name: str,
        common_path: str,
        test_elements: list[InterfaceTestingAggregateTestElement],
        timestamp: datetime | None = None,
    ):
        super().__init__(interface_name)
        self.common_path = common_path
        self.test_elements = test_elements
        self.timestamp = timestamp

    def _send_data_to_the_server(
        self, cfg: Configuration, test_element: InterfaceTestingAggregateTestElement
    ):
        payload = {}
        for key, value in test_element.entries.items():
            payload[key] = http_prepare_transmit_data(
                self.interface, self.common_path + "/" + key, value
            )
        http_post_server_data(cfg, self.interface_name, self.common_path, payload)

    def _check_data_received_by_the_server(
        self, cfg: Configuration, test_element: InterfaceTestingAggregateTestElement
    ) -> bool:
        received_payload = http_get_server_data(cfg, self.interface_name, limit=1)
        logger.info(f"Server aggregate data {received_payload}")

        try:
            for key, value in test_element.entries.items():
                full_element_path = self.common_path + "/" + key
                unpacked_payload = received_payload[self.common_path.split("/")[1]][0][key]
                received_value = http_decode_received_data(
                    self.interface, full_element_path, unpacked_payload
                )

                logger.info(
                    f"Interface '{self.interface_name}', path '{full_element_path}':"
                    + f"expected {value} got {received_value}"
                )
                if (not cfg.log_only) and (not (received_value == value)):
                    return False
        except KeyError as e:
            logger.error(f"KeyError while accessing the object {e}")
            return False

        return True

    def _get_command_for_the_device(
        self, base_command: str, test_element: InterfaceTestingAggregateTestElement
    ) -> str:
        command = (
            f"{base_command} object {self.interface_name} {self.common_path}"
            + f" {encode_shell_bson(test_element.entries)}"
        )
        if self.timestamp:
            unix_t = int(self.timestamp.timestamp() * 1000)
            command += f" {unix_t}"
        return command

    def _get_individual_test_elements(self) -> list[InterfaceTestingAggregateTestElement]:
        return self.test_elements
