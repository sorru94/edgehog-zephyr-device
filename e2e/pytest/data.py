# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

from datetime import datetime, timezone

from interface_testing import InterfaceTesting
from interface_testing_aggregate import (
    InterfaceTestingAggregate,
    InterfaceTestingAggregateTestElement,
)
from interface_testing_datastream import (
    InterfaceTestingDatastream,
    InterfaceTestingDatastreamTestElement,
)
from interface_testing_property import (
    InterfaceTestingPropertySet,
    InterfaceTestingPropertyUnsetTest,
    InterfaceTestingPropertySetTestElement,
    InterfaceTestingPropertyUnSetTestElement,
)

data: list[InterfaceTesting] = [
    InterfaceTestingDatastream(
        interface_name="org.astarte-platform.zephyr.e2etest.DeviceDatastream",
        test_elements=[
            InterfaceTestingDatastreamTestElement(
                path="/binaryblob_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=b"SGVsbG8=",
            ),
            InterfaceTestingDatastreamTestElement(
                path="/binaryblobarray_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=[b"SGVsbG8=", b"dDk5Yg=="],
            ),
            InterfaceTestingDatastreamTestElement(
                path="/boolean_endpoint", timestamp=datetime.now(tz=timezone.utc), value=True
            ),
            InterfaceTestingDatastreamTestElement(
                path="/booleanarray_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=[True, False, True],
            ),
            InterfaceTestingDatastreamTestElement(
                path="/datetime_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=datetime.fromtimestamp(1710940988, tz=timezone.utc),
            ),
            InterfaceTestingDatastreamTestElement(
                path="/datetimearray_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=[
                    datetime.fromtimestamp(17109409814, tz=timezone.utc),
                    datetime.fromtimestamp(1710940988, tz=timezone.utc),
                ],
            ),
            InterfaceTestingDatastreamTestElement(
                path="/double_endpoint", timestamp=datetime.now(tz=timezone.utc), value=15.42
            ),
            InterfaceTestingDatastreamTestElement(
                path="/doublearray_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=[1542.25, 88852.6],
            ),
            InterfaceTestingDatastreamTestElement(
                path="/integer_endpoint", timestamp=datetime.now(tz=timezone.utc), value=42
            ),
            InterfaceTestingDatastreamTestElement(
                path="/integerarray_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=[4525, 0, 11],
            ),
            InterfaceTestingDatastreamTestElement(
                path="/longinteger_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=8589934592,
            ),
            InterfaceTestingDatastreamTestElement(
                path="/longintegerarray_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=[8589930067, 42, 8589934592],
            ),
            InterfaceTestingDatastreamTestElement(
                path="/string_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value="Hello world!",
            ),
            InterfaceTestingDatastreamTestElement(
                path="/stringarray_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=["Hello ", "world!"],
            ),
        ],
    ),
    InterfaceTestingDatastream(
        interface_name="org.astarte-platform.zephyr.e2etest.ServerDatastream",
        test_elements=[
            InterfaceTestingDatastreamTestElement(
                path="/binaryblob_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=b"SGVsbG8=",
            ),
            InterfaceTestingDatastreamTestElement(
                path="/binaryblobarray_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=[b"SGVsbG8=", b"dDk5Yg=="],
            ),
            InterfaceTestingDatastreamTestElement(
                path="/boolean_endpoint", timestamp=datetime.now(tz=timezone.utc), value=True
            ),
            InterfaceTestingDatastreamTestElement(
                path="/booleanarray_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=[True, False, True],
            ),
            InterfaceTestingDatastreamTestElement(
                path="/datetime_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=datetime.fromtimestamp(1710940988, tz=timezone.utc),
            ),
            InterfaceTestingDatastreamTestElement(
                path="/datetimearray_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=[
                    datetime.fromtimestamp(17109409814, tz=timezone.utc),
                    datetime.fromtimestamp(1710940988, tz=timezone.utc),
                ],
            ),
            InterfaceTestingDatastreamTestElement(
                path="/double_endpoint", timestamp=datetime.now(tz=timezone.utc), value=15.42
            ),
            InterfaceTestingDatastreamTestElement(
                path="/doublearray_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=[1542.25, 88852.6],
            ),
            InterfaceTestingDatastreamTestElement(
                path="/integer_endpoint", timestamp=datetime.now(tz=timezone.utc), value=42
            ),
            InterfaceTestingDatastreamTestElement(
                path="/integerarray_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=[4525, 0, 11],
            ),
            InterfaceTestingDatastreamTestElement(
                path="/longinteger_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=8589934592,
            ),
            InterfaceTestingDatastreamTestElement(
                path="/longintegerarray_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=[8589930067, 42, 8589934592],
            ),
            InterfaceTestingDatastreamTestElement(
                path="/string_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value="Hello world!",
            ),
            InterfaceTestingDatastreamTestElement(
                path="/stringarray_endpoint",
                timestamp=datetime.now(tz=timezone.utc),
                value=["Hello ", "world!"],
            ),
        ],
    ),
    InterfaceTestingAggregate(
        interface_name="org.astarte-platform.zephyr.e2etest.DeviceAggregate",
        common_path="/sensor42",
        test_elements=[
            InterfaceTestingAggregateTestElement(
                entries={
                    "binaryblob_endpoint": b"SGVsbG8=",
                    "binaryblobarray_endpoint": [b"SGVsbG8=", b"dDk5Yg=="],
                    "boolean_endpoint": True,
                    "booleanarray_endpoint": [True, False, True],
                    "datetime_endpoint": datetime.fromtimestamp(1710940988, tz=timezone.utc),
                    "datetimearray_endpoint": [
                        datetime.fromtimestamp(17109409814, tz=timezone.utc),
                        datetime.fromtimestamp(1710940988, tz=timezone.utc),
                    ],
                    "double_endpoint": 15.42,
                    "doublearray_endpoint": [1542.25, 88852.6],
                    "integer_endpoint": 42,
                    "integerarray_endpoint": [4525, 0, 11],
                    "longinteger_endpoint": 8589934592,
                    "longintegerarray_endpoint": [8589930067, 42, 8589934592],
                    "string_endpoint": "Hello world!",
                    "stringarray_endpoint": ["Hello ", "world!"],
                }
            )
        ],
    ),
    InterfaceTestingAggregate(
        interface_name="org.astarte-platform.zephyr.e2etest.ServerAggregate",
        common_path="/path37",
        timestamp=datetime.now(tz=timezone.utc),
        test_elements=[
            InterfaceTestingAggregateTestElement(
                entries={
                    "binaryblob_endpoint": b"SGVsbG8=",
                    "binaryblobarray_endpoint": [b"SGVsbG8=", b"dDk5Yg=="],
                    "boolean_endpoint": True,
                    "booleanarray_endpoint": [True, False, True],
                    "datetime_endpoint": datetime.fromtimestamp(1710940988, tz=timezone.utc),
                    "datetimearray_endpoint": [
                        datetime.fromtimestamp(17109409814, tz=timezone.utc),
                        datetime.fromtimestamp(1710940988, tz=timezone.utc),
                    ],
                    "double_endpoint": 15.42,
                    "doublearray_endpoint": [1542.25, 88852.6],
                    "integer_endpoint": 42,
                    "integerarray_endpoint": [4525, 0, 11],
                    "longinteger_endpoint": 8589934592,
                    "longintegerarray_endpoint": [8589930067, 42, 8589934592],
                    "string_endpoint": "Hello world!",
                    "stringarray_endpoint": ["Hello ", "world!"],
                }
            )
        ],
    ),
    InterfaceTestingPropertySet(
        interface_name="org.astarte-platform.zephyr.e2etest.DeviceProperty",
        test_elements=[
            InterfaceTestingPropertySetTestElement(
                path="/sensor36/binaryblob_endpoint", value=b"SGVsbG8="
            ),
            InterfaceTestingPropertySetTestElement(
                path="/sensor36/binaryblobarray_endpoint", value=[b"SGVsbG8=", b"dDk5Yg=="]
            ),
            InterfaceTestingPropertySetTestElement(path="/sensor36/boolean_endpoint", value=True),
            InterfaceTestingPropertySetTestElement(
                path="/sensor36/booleanarray_endpoint", value=[True, False, True]
            ),
            InterfaceTestingPropertySetTestElement(
                path="/sensor36/datetime_endpoint",
                value=datetime.fromtimestamp(1710940988, tz=timezone.utc),
            ),
            InterfaceTestingPropertySetTestElement(
                path="/sensor36/datetimearray_endpoint",
                value=[
                    datetime.fromtimestamp(1710940988, tz=timezone.utc),
                    datetime.fromtimestamp(17109409814, tz=timezone.utc),
                ],
            ),
            InterfaceTestingPropertySetTestElement(path="/sensor36/double_endpoint", value=15.42),
            InterfaceTestingPropertySetTestElement(
                path="/sensor36/doublearray_endpoint", value=[1542.25, 88852.6]
            ),
            InterfaceTestingPropertySetTestElement(path="/sensor36/integer_endpoint", value=42),
            InterfaceTestingPropertySetTestElement(
                path="/sensor36/integerarray_endpoint", value=[4525, 0, 11]
            ),
            InterfaceTestingPropertySetTestElement(
                path="/sensor36/longinteger_endpoint", value=8589934592
            ),
            InterfaceTestingPropertySetTestElement(
                path="/sensor36/longintegerarray_endpoint", value=[8589930067, 42, 8589934592]
            ),
            InterfaceTestingPropertySetTestElement(
                path="/sensor36/string_endpoint", value="Hello world!"
            ),
            InterfaceTestingPropertySetTestElement(
                path="/sensor36/stringarray_endpoint", value=["Hello ", "world!"]
            ),
        ],
    ),
    InterfaceTestingPropertySet(
        interface_name="org.astarte-platform.zephyr.e2etest.ServerProperty",
        test_elements=[
            InterfaceTestingPropertySetTestElement(
                path="/path84/binaryblob_endpoint", value=b"SGVsbG8="
            ),
            InterfaceTestingPropertySetTestElement(
                path="/path84/binaryblobarray_endpoint", value=[b"SGVsbG8=", b"dDk5Yg=="]
            ),
            InterfaceTestingPropertySetTestElement(path="/path84/boolean_endpoint", value=True),
            InterfaceTestingPropertySetTestElement(
                path="/path84/booleanarray_endpoint", value=[True, False, True]
            ),
            InterfaceTestingPropertySetTestElement(
                path="/path84/datetime_endpoint",
                value=datetime.fromtimestamp(1710940988, tz=timezone.utc),
            ),
            InterfaceTestingPropertySetTestElement(
                path="/path84/datetimearray_endpoint",
                value=[
                    datetime.fromtimestamp(1710940988, tz=timezone.utc),
                    datetime.fromtimestamp(17109409814, tz=timezone.utc),
                ],
            ),
            InterfaceTestingPropertySetTestElement(path="/path84/double_endpoint", value=15.42),
            InterfaceTestingPropertySetTestElement(
                path="/path84/doublearray_endpoint", value=[1542.25, 88852.6]
            ),
            InterfaceTestingPropertySetTestElement(path="/path84/integer_endpoint", value=42),
            InterfaceTestingPropertySetTestElement(
                path="/path84/integerarray_endpoint", value=[4525, 0, 11]
            ),
            InterfaceTestingPropertySetTestElement(
                path="/path84/longinteger_endpoint", value=8589934592
            ),
            InterfaceTestingPropertySetTestElement(
                path="/path84/longintegerarray_endpoint", value=[8589930067, 42, 8589934592]
            ),
            InterfaceTestingPropertySetTestElement(
                path="/path84/string_endpoint", value="Hello world!"
            ),
            InterfaceTestingPropertySetTestElement(
                path="/path84/stringarray_endpoint", value=["Hello ", "world!"]
            ),
        ],
    ),
    InterfaceTestingPropertyUnsetTest(
        interface_name="org.astarte-platform.zephyr.e2etest.DeviceProperty",
        test_elements=[
            InterfaceTestingPropertyUnSetTestElement(path="/sensor36/binaryblob_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/sensor36/binaryblobarray_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/sensor36/boolean_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/sensor36/booleanarray_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/sensor36/datetime_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/sensor36/datetimearray_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/sensor36/double_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/sensor36/doublearray_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/sensor36/integer_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/sensor36/integerarray_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/sensor36/longinteger_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/sensor36/longintegerarray_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/sensor36/string_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/sensor36/stringarray_endpoint"),
        ],
    ),
    InterfaceTestingPropertyUnsetTest(
        interface_name="org.astarte-platform.zephyr.e2etest.ServerProperty",
        test_elements=[
            InterfaceTestingPropertyUnSetTestElement(path="/path84/binaryblob_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/path84/binaryblobarray_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/path84/boolean_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/path84/booleanarray_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/path84/datetime_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/path84/datetimearray_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/path84/double_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/path84/doublearray_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/path84/integer_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/path84/integerarray_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/path84/longinteger_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/path84/longintegerarray_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/path84/string_endpoint"),
            InterfaceTestingPropertyUnSetTestElement(path="/path84/stringarray_endpoint"),
        ],
    ),
]
