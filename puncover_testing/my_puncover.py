import os
from pathlib import Path

import subprocess
from pprint import pprint

from puncover.builders import ElfBuilder
from puncover.backtrace_helper import BacktraceHelper
from puncover.collector import Collector
from puncover.collector import NAME, TYPE, TYPE_FUNCTION, CALLEES, CALLERS, ASM, ADDRESS
from puncover.gcc_tools import GCCTools

class MyBuilder:

    def __init__(self, gcc_base_filename, elf_file, su_dir=None, src_root=None):
        self.files = {}
        self.collector = Collector(GCCTools(gcc_base_filename))
        self.elf_file = elf_file
        self.su_dir = su_dir
        self.src_root = src_root

    def store_file_time(self, path, store_empty=False):
        self.files[path] = 0 if store_empty else os.path.getmtime(path)

    def needs_build(self):
        return any([os.path.getmtime(f) > t for f,t in self.files.items()])

    def populate_collection(self):
        for f in self.files.keys():
            self.store_file_time(f)
        self.collector.reset()
        self.collector.parse_elf(self.elf_file)
        self.add_missing_links()
        self.collector.enhance(self.src_root)
        self.collector.parse_su_dir(self.su_dir)

        backtrace_helper = BacktraceHelper(self.collector)
        for f in self.collector.all_functions():
            backtrace_helper.deepest_callee_tree(f)
            backtrace_helper.deepest_caller_tree(f)

    # caller, callee, priority (smaller-higher)
    missing_links = [
        # # Links internal to the Astarte device
        ("handle_publish_event", "astarte_device_rx_on_incoming_handler", 0),
        ("mqtt_evt_handler", "astarte_device_connection_on_subscribed_handler", 3),
        ("mqtt_evt_handler", "astarte_device_rx_on_incoming_handler", 2),
        ("mqtt_evt_handler", "astarte_device_connection_on_disconnected_handler", 1),
        ("mqtt_evt_handler", "astarte_device_connection_on_connected_handler", 0),
        ("astarte_mqtt_connect", "refresh_client_cert_handler", 0),
        ("mqtt_caching_check_message_expiry", "mqtt_caching_retransmit_out_msg_handler", 2),
        # Links internal to the Edgehog device
        ("http_download_cb", "http_download_payload_cbk", 0),
        # Links for sample callbacks
        ("astarte_device_connection_poll", "astarte_device_connection_callback", 0),
        ("astarte_device_rx_on_incoming_handler", "astarte_device_datastream_object_callback", 3),
        ("astarte_device_rx_on_incoming_handler", "astarte_device_datastream_individual_callback", 2),
        ("astarte_device_rx_on_incoming_handler", "astarte_device_property_set_callback", 1),
        ("astarte_device_rx_on_incoming_handler", "astarte_device_property_unset_callback", 0),
        # ("astarte_device_connection_on_disconnected_handler", "astarte_device_disconnection_callback", 0),
    ]


    def add_missing_links(self):
        for caller, callee, priority in self.missing_links:
            caller_symbol = [s for _, s in self.collector.symbols.items() if s[NAME] == caller][0]
            callee_symbol = [s for _, s in self.collector.symbols.items() if s[NAME] == callee][0]

            blx_lines = [(i, l) for i, l in enumerate(caller_symbol[ASM]) if "blx" in l]
            idx, blx_line = blx_lines[priority]

            caller_symbol[ASM][idx] = (
                f"{blx_line.split(":")[0]}:	f027 fce9 	bl	{callee_symbol[ADDRESS]} <{callee_symbol[NAME]}>")

    def build_if_needed(self):
        if self.needs_build():
            self.populate_collection()

def find_all_blx_and_bx_instructions(collector):

    symbols_with_fn_ptr_call = []
    for _, symbol in collector.symbols.items():
        if (("astarte-device-sdk-zephyr" in str(symbol["path"])) or
            ("edgehog-zephyr-device" in str(symbol["path"]))):
            for asm_line in symbol.get("asm", []):
                # if ("blx" in asm_line) or ("bl" in asm_line):
                if "blx" in asm_line:
                    symbols_with_fn_ptr_call.append(symbol)

    print("Found functions containing jumps:")
    for symbol in symbols_with_fn_ptr_call:
        print(symbol[NAME])

def generate_graph(collector, start_symbol_name, out_file, max_depth=100):

    # Find the start symbol name
    start_symbols = [s for _, s in collector.symbols.items() if s[NAME] == start_symbol_name]
    if len(start_symbols) != 1:
        raise IndexError("Multiple symbols with the same name!")
    start_symbol = start_symbols[0]

    if start_symbol[TYPE] != TYPE_FUNCTION:
        raise ValueError("Graph can be created only for a function!")

    traversed_symbols = []
    transitions = []
    generate_transitions(start_symbol, traversed_symbols, transitions, 1, max_depth)

    graph_lines = ["graph TD;\n"]
    for source, destination in transitions:
        graph_lines.append(f"    {source[NAME]}-->{destination[NAME]};\n")

    with open(out_file, 'w') as f:
        f.writelines(graph_lines)

def generate_transitions(start_symbol, traversed_symbols, transitions, depth, max_depth):
    # print(f"Traversing: {start_symbol[NAME]}")
    traversed_symbols.append(start_symbol)
    if depth + 1 <= max_depth:
        for called_symbol in start_symbol[CALLEES]:
            transitions.append((start_symbol, called_symbol))
            if called_symbol not in traversed_symbols:
                generate_transitions(called_symbol, traversed_symbols, transitions, depth + 1, max_depth)
    else:
        print("Maximum depth reached.")


if __name__ == '__main__':

    script_path = Path(__file__).parent
    edgehog_device_sdk_path = script_path.parent
    zephyr_project_path = edgehog_device_sdk_path.parent
    # NB: The build folder is expected in the project directory
    build_dir_path = zephyr_project_path.joinpath("build", "edgehog_app")
    zephyr_sdk_path = zephyr_project_path.parent.joinpath("zephyr-sdk-0.16.8")
    gcc_tools_path = zephyr_sdk_path.joinpath("arm-zephyr-eabi", "bin")
    gcc_tools_prefix = "arm-zephyr-eabi-"
    src_root_path = zephyr_project_path.joinpath("zephyr")
    elf_file = build_dir_path.joinpath("zephyr", "sample-edgehog-app.elf")

    builder = MyBuilder(str(gcc_tools_path.joinpath(gcc_tools_prefix)), elf_file,
                            src_root=src_root_path, su_dir=build_dir_path)
    builder.populate_collection()

    find_all_blx_and_bx_instructions(builder.collector)

    generate_graph(builder.collector, "edgehog_device_thread_entry_point", script_path.joinpath("generated.mermaid"))
