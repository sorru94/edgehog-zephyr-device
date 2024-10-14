import os
from pathlib import Path
import webbrowser
from threading import Timer

from flask import Flask
from puncover import renderers
from puncover.builders import ElfBuilder
from puncover.middleware import BuilderMiddleware

from my_puncover import MyBuilder

app = Flask(__name__)

# Default listening port. Fallback to 8000 if the default port is already in use.
DEFAULT_PORT = 5000
DEFAULT_PORT_FALLBACK = 8000

def is_port_in_use(port: int) -> bool:
    import socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        return s.connect_ex(('localhost', port)) == 0


def get_default_port():
    return DEFAULT_PORT if not is_port_in_use(DEFAULT_PORT) else DEFAULT_PORT_FALLBACK

def open_browser(host, port):
    webbrowser.open("http://{}:{}/".format(host, port))



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

    renderers.register_jinja_filters(app.jinja_env)
    renderers.register_urls(app, builder.collector)
    app.wsgi_app = BuilderMiddleware(app.wsgi_app, builder)

    if is_port_in_use(get_default_port()):
        print("Port {} is already in use, please choose a different port.".format(get_default_port()))
        exit(1)

    # Open a browser window, only if this is the first instance of the server
    # from https://stackoverflow.com/a/63216793
    if not os.environ.get("WERKZEUG_RUN_MAIN"):
        # wait one second before starting, so the flask server is ready and we
        # don't see a 404 for a moment first
        Timer(1, open_browser, kwargs={"host":'127.0.0.1', "port":get_default_port()}).start()

    app.run(host='127.0.0.1', port=get_default_port())
