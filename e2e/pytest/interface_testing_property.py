# (C) Copyright 2026, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

from typing import Any
import logging

from configuration import Configuration
from interface_testing import InterfaceTesting
from http_requests import (
    http_prepare_transmit_data,
    http_post_server_data,
    http_decode_received_data,
    http_get_server_data,
    http_delete_server_data,
)
from test_utilities import encode_shell_bson

logger = logging.getLogger(__name__)


class InterfaceTestingPropertySetTestElement:
    def __init__(self, path: str, value: Any):
        self.path = path
        self.value = value


class InterfaceTestingPropertySet(InterfaceTesting[InterfaceTestingPropertySetTestElement]):
    def __init__(
        self, interface_name: str, test_elements: list[InterfaceTestingPropertySetTestElement]
    ):
        super().__init__(interface_name)
        self.test_elements = test_elements

    def _send_data_to_the_server(
        self, cfg: Configuration, test_element: InterfaceTestingPropertySetTestElement
    ):
        payload = http_prepare_transmit_data(self.interface, test_element.path, test_element.value)
        http_post_server_data(cfg, self.interface_name, test_element.path, payload)

    def _check_data_received_by_the_server(
        self, cfg: Configuration, test_element: InterfaceTestingPropertySetTestElement
    ):
        received_payload = http_get_server_data(cfg, self.interface_name)
        logger.info(f"Server property data {received_payload}")

        # Retrieve and check properties
        try:
            first_path_segment = test_element.path.split("/")[1]
            last_path_segment = test_element.path.split("/")[-1]
            unpacked_payload = received_payload[first_path_segment][last_path_segment]
            received_value = http_decode_received_data(
                self.interface, test_element.path, unpacked_payload
            )
        except KeyError as e:
            logger.error(f"KeyError while accessing the property {e}")
            return False

        logger.info(
            f"Interface '{self.interface_name}', path '{test_element.path}': "
            + f"expected {test_element.value} got {received_value}"
        )
        return cfg.log_only or (received_value == test_element.value)

    def _get_command_for_the_device(
        self, base_command: str, test_element: InterfaceTestingPropertySetTestElement
    ) -> str:
        return (
            f"{base_command} property set {self.interface_name} {test_element.path}"
            + f" {encode_shell_bson(test_element.value)}"
        )

    def _get_individual_test_elements(self) -> list[InterfaceTestingPropertySetTestElement]:
        return self.test_elements


class InterfaceTestingPropertyUnSetTestElement:
    def __init__(self, path: str):
        self.path = path


class InterfaceTestingPropertyUnsetTest(InterfaceTesting[InterfaceTestingPropertyUnSetTestElement]):
    def __init__(
        self, interface_name: str, test_elements: list[InterfaceTestingPropertyUnSetTestElement]
    ):
        super().__init__(interface_name)
        self.test_elements = test_elements

    def _send_data_to_the_server(
        self, cfg: Configuration, test_element: InterfaceTestingPropertyUnSetTestElement
    ):
        http_delete_server_data(cfg, self.interface_name, test_element.path)

    def _check_data_received_by_the_server(
        self, cfg: Configuration, test_element: InterfaceTestingPropertyUnSetTestElement
    ) -> bool:
        received_data = http_get_server_data(cfg, self.interface_name)
        logger.info(f"Server property data {received_data}")

        if cfg.log_only:
            return True

        first_path_segment = test_element.path.split("/")[1]
        if first_path_segment not in received_data:
            logger.info(
                f"The property is not accessible as expected: "
                + f"{first_path_segment} not in {received_data}"
            )
            return True

        last_path_segment = test_element.path.split("/")[-1]
        if last_path_segment not in received_data[first_path_segment]:
            logger.info(
                f"The property is not accessible as expected: "
                + f"{last_path_segment} not in {received_data[first_path_segment]}"
            )
            return True

        logger.error(f"The property is still accessible: {last_path_segment} in {received_data}")
        return False

    def _get_command_for_the_device(
        self, base_command: str, test_element: InterfaceTestingPropertyUnSetTestElement
    ) -> str:
        return f"{base_command} property unset {self.interface_name} {test_element.path}"

    def _get_individual_test_elements(self) -> list[InterfaceTestingPropertyUnSetTestElement]:
        return self.test_elements
