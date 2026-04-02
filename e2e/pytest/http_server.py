# (C) Copyright 2026, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

import http.server
import logging
import ssl
import threading

import os
import random
import string
import tarfile
import lz4.frame
from pathlib import Path

logger = logging.getLogger(__name__)

# Global references to manage the server lifecycle
_httpd = None
_server_thread = None

def start_server(port=8443, cert_file='192.0.2.2.pem', key_file='192.0.2.2.key', data_dir='.'):
    logger.info("Starting server with the following configuration:")
    logger.info(f"HTTP server port: {port}")
    logger.info(f"HTTP server certificate: {cert_file}")
    logger.info(f"HTTP server key: {key_file}")
    logger.info(f"HTTP server data directory: {data_dir}")

    global _httpd, _server_thread

    # Create a request handler that serves files from test data directory
    # Cast data_dir to a string in case a pathlib.Path object is passed
    class RequestHandler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=str(data_dir), **kwargs)

        def do_PUT(self):
            save_path = self.translate_path(self.path)

            try:
                os.makedirs(os.path.dirname(save_path), exist_ok=True)

                # Check how the data is being sent
                is_chunked = self.headers.get('Transfer-Encoding', '').lower() == 'chunked'
                content_length = int(self.headers.get('Content-Length', -1))

                with open(save_path, 'wb') as f:
                    if is_chunked:
                        # Handle Chunked Transfer Encoding
                        while True:
                            # Read the chunk size (sent in hexadecimal)
                            line = self.rfile.readline().strip()
                            if not line:
                                break

                            chunk_size = int(line, 16)

                            if chunk_size == 0:
                                # A chunk size of 0 indicates the end of the transfer
                                self.rfile.readline() # Consume the final trailing \r\n
                                break

                            # Read the actual chunk data
                            chunk_data = self.rfile.read(chunk_size)
                            f.write(chunk_data)

                            # Consume the \r\n that follows each chunk
                            self.rfile.readline()

                    elif content_length >= 0:
                        # Handle standard Content-Length payload
                        f.write(self.rfile.read(content_length))

                    else:
                        # Fallback: No Content-Length and not chunked.
                        # This relies on the client closing the connection immediately after sending.
                        logger.warning("No Content-Length or chunking specified. Reading until connection closes.")
                        f.write(self.rfile.read())

                if save_path.endswith('.tar.lz4'):
                    tar_path = save_path[:-4]
                    Path(tar_path).write_bytes(lz4.frame.decompress(Path(save_path).read_bytes()))
                    tarfile.open(tar_path).extractall(os.path.dirname(save_path))

                # Send success response
                response_msg = f"Successfully saved to {self.path}".encode('utf-8')
                self.send_response(201)
                self.send_header('Content-Length', str(len(response_msg)))
                self.send_header('Connection', 'close')
                self.end_headers()
                self.wfile.write(response_msg)
                logger.info(f"Saved PUT file to {save_path}")
                self.close_connection = True

            except PermissionError:
                response_msg = b"Permission denied.".encode('utf-8')
                self.send_response(403)
                self.send_header('Content-Length', str(len(response_msg)))
                self.send_header('Connection', 'close')
                self.end_headers()
                self.wfile.write(response_msg)
                logger.error(f"Permission denied to write to: {save_path}")
                self.close_connection = True
            except Exception as e:
                response_msg = b"Server error while saving the file.".encode('utf-8')
                self.send_response(500)
                self.send_header('Content-Length', str(len(response_msg)))
                self.send_header('Connection', 'close')
                self.end_headers()
                self.wfile.write(response_msg)
                logger.error(f"Failed to save file: {e}")
                self.close_connection = True

    # Initialize the HTTP server
    server_address = ('0.0.0.0', int(port))
    _httpd = http.server.HTTPServer(server_address, RequestHandler)

    # Establish the TLS context using the generated self-signed certificate
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)

    # Cast Path objects to strings and remove any accidental brackets at the end
    context.load_cert_chain(certfile=str(cert_file), keyfile=str(key_file))

    # Wrap the socket to serve HTTPS
    _httpd.socket = context.wrap_socket(_httpd.socket, server_side=True)

    # Start the server in a separate background thread
    _server_thread = threading.Thread(target=_httpd.serve_forever)
    _server_thread.daemon = True
    _server_thread.start()

    logger.info(f"Background HTTPS server started at https://localhost:{port}")

def stop_server():
    global _httpd, _server_thread
    if _httpd:
        logger.info("Shutting down HTTPS server...")
        # shutdown() blocks until the loop finishes, and MUST be called from
        # a different thread than the one running serve_forever()
        _httpd.shutdown()
        _httpd.server_close()

        # Wait for the background thread to safely exit
        if _server_thread:
            _server_thread.join()

        _httpd = None
        _server_thread = None
        logger.info("Server stopped.")

def generate_archives(dir: Path):
    tar_path = dir / 'archive.tar'
    lz4_path = dir / 'compressed_archive.tar.lz4'

    # Create the uncompressed TAR file
    logger.info(f"Creating uncompressed archive: {tar_path}")
    with tarfile.open(tar_path, mode='w', format=tarfile.USTAR_FORMAT) as tar:

        # 1. Create the empty directory in the archive
        empty_dir = tarfile.TarInfo('test_empty_directory')
        empty_dir.type = tarfile.DIRTYPE
        empty_dir.mode = 0o755
        tar.addfile(empty_dir)
        print(f" - Added empty directory 'test_empty_directory' to {tar_path.name}")

        # 2. Create 'test_directory' in the archive
        test_dir = tarfile.TarInfo('test_directory')
        test_dir.type = tarfile.DIRTYPE
        test_dir.mode = 0o755
        tar.addfile(test_dir)

        # 3. Map physical files to their desired locations inside the archive
        archive_layout = [
            (dir / 'test_data_0.txt', 'test_data_0.txt'),
            (dir / 'test_data_1.txt', 'test_data_1.txt'),
            (dir / 'test_directory' / 'test_data_3.txt', 'test_directory/test_data_3.txt')
        ]

        for file_path, arcname in archive_layout:
            if file_path.exists():
                tar.add(file_path, arcname=arcname)
                print(f" - Added {file_path.name} as {arcname} to {tar_path.name}")
            else:
                logger.error(f"File {file_path} not found, skipping.")

    # Compress the TAR file into LZ4
    logger.info(f"Compressing {tar_path.name} into {lz4_path.name}...")
    with open(tar_path, 'rb') as f_in:
        # Using your optimized low-RAM settings
        with lz4.frame.open(
            lz4_path,
            mode='wb',
            block_size=lz4.frame.BLOCKSIZE_MAX64KB,
            block_linked=False
        ) as f_out:
            # Efficiently stream the data from disk to the compressed file
            f_out.write(f_in.read())

    logger.info("Archiving and compression complete!")
