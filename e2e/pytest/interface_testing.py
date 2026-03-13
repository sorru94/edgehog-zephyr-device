# (C) Copyright 2026, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

from abc import ABC, abstractmethod
from typing import Generic, TypeVar
import time
import pytest
from math import ceil

from configuration import Configuration

SHELL_CMD_SEND = "dvcshellcmd_send"
SHELL_CMD_EXPECT = "dvcshellcmd_expect"

T = TypeVar("T", covariant=False)


class InterfaceTesting(ABC, Generic[T]):
    """
    Base interface data class, defines a generic test method that handles
    sending and receiving data for both the server and the client.
    Implementors should redefine the "private" methods
    """

    def __init__(self, interface_name: str):
        self.interface_name = interface_name
        self.interface = None

    @abstractmethod
    def _send_data_to_the_server(self, cfg: Configuration, test_element: T):
        """
        Abstract method that handles sending the data passed from the server to the device
        """

    @abstractmethod
    def _check_data_received_by_the_server(self, cfg: Configuration, test_element: T) -> bool:
        """
        Abstract method that checks the data received by the server.
        This is the data that was sent the device using a "send" shell command
        """

    @abstractmethod
    def _get_command_for_the_device(self, base_command: str, test_element: T) -> str:
        """
        Gets the command that encodes the send/set or unset of this interface and the passed data.
        This comes in the form of '{base_command} <interface_name> <path> <bson_base64_data> <timestamp>'
        """

    @abstractmethod
    def _get_individual_test_elements(self) -> list[T]:
        """
        Get a list of element each corresponding to a single astarte send/set or unset command.
        """

    def execute_tests(self, cfg: Configuration):
        """
        Test reception and transmission of this interface
        """

        # Match the configuration with an Astarte interface
        for interface in cfg.interfaces:
            if interface.name == self.interface_name:
                self.interface = interface
                break

        # If match was impssible fail the test
        if self.interface is None:
            pytest.fail(f"Test failed, no interface matches the provided name.")

        # Depending on the interface ownership run the tests for server or device interfaces
        for test_element in self._get_individual_test_elements():
            if interface.is_server_owned():
                # Send the expect command to the device
                device_command = self._get_command_for_the_device(SHELL_CMD_EXPECT, test_element)
                cfg.shell.exec_command(device_command)
                # Send the data to the Astarte server
                self._send_data_to_the_server(cfg, test_element)
            else:
                # Send the send request to the device
                device_command = self._get_command_for_the_device(SHELL_CMD_SEND, test_element)
                cfg.shell.exec_command(device_command)

                # Poll for up to 5 seconds to handle Astarte's eventual consistency
                timeout = 5
                retry_delay = 0.5
                data_received = False

                for _ in range(ceil(timeout / retry_delay)):
                    try:
                        if self._check_data_received_by_the_server(cfg, test_element):
                            data_received = True
                            break
                    except Exception as _:
                        # Suppress KeyError or other parsing exceptions while data is incomplete
                        pass

                    time.sleep(retry_delay)

                assert data_received

            # Limit the rate of each test element
            time.sleep(1)
