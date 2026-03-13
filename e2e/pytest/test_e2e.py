# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

import time
import logging

from configuration import Configuration
from data import data

logger = logging.getLogger(__name__)

SHELL_IS_READY = "dvcshellcmd Device shell ready$"
SHELL_IS_CLOSING = "dvcshellcmd Device shell closing$"
SHELL_CMD_DISCONNECT = "dvcshellcmd_disconnect"


def test_device(end_to_end_configuration: Configuration):
    logger.info("Launching the device")

    end_to_end_configuration.dut.launch()
    end_to_end_configuration.dut.readlines_until(SHELL_IS_READY, timeout=60)

    # Wait a couple of seconds
    time.sleep(1)

    for interface in data:
        interface.execute_tests(end_to_end_configuration)

    # Wait a couple of seconds
    time.sleep(1)

    end_to_end_configuration.shell.exec_command(SHELL_CMD_DISCONNECT)
    end_to_end_configuration.dut.readlines_until(SHELL_IS_CLOSING, timeout=60)
