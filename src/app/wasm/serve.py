#!/usr/bin/env python3
# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT
#
# Tiny static HTTP server that serves the WebAssembly build dir with the
# Cross-Origin headers required for SharedArrayBuffer / Emscripten pthreads
# to work, and the right MIME type for .wasm files.
#
# Usage:
#     python src/app/wasm/serve.py [port] [--dir <path>]
# Defaults to port 8765 and ./build-wasm relative to the repo root.

import argparse
import functools
import http.server
import os
import sys


class CrossOriginIsolatedHandler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".wasm": "application/wasm",
        ".js": "application/javascript",
        ".mjs": "application/javascript",
    }

    def end_headers(self) -> None:
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Resource-Policy", "cross-origin")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", nargs="?", type=int, default=8765)
    parser.add_argument(
        "--dir",
        default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "build-wasm"),
        help="Directory to serve.",
    )
    args = parser.parse_args()

    serve_dir = os.path.abspath(args.dir)
    if not os.path.isdir(serve_dir):
        print(f"Serve directory not found: {serve_dir}", file=sys.stderr)
        return 1

    handler = functools.partial(CrossOriginIsolatedHandler, directory=serve_dir)
    address = ("127.0.0.1", args.port)
    httpd = http.server.ThreadingHTTPServer(address, handler)
    print(
        f"Serving {serve_dir} at http://{address[0]}:{address[1]}/ with COEP+COOP headers."
    )
    print("Press Ctrl+C to stop.")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        httpd.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
