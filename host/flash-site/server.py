#!/usr/bin/env python3
"""Static flash-site server for Lumentree firmware."""

from __future__ import annotations

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class FlashSiteHandler(SimpleHTTPRequestHandler):
    """Serve the flash site with conservative cache headers."""

    def end_headers(self) -> None:
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        super().end_headers()

    def log_message(self, fmt: str, *args) -> None:
        print(f"[flash-site] {self.address_string()} - {fmt % args}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8790)
    parser.add_argument(
        "--root",
        default=str(Path(__file__).resolve().parent / "public"),
    )
    args = parser.parse_args()

    root = Path(args.root).resolve()
    handler = partial(FlashSiteHandler, directory=str(root))
    httpd = ThreadingHTTPServer((args.host, args.port), handler)
    print(f"[flash-site] serving {root} on http://{args.host}:{args.port}")
    httpd.serve_forever()


if __name__ == "__main__":
    main()
