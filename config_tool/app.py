"""
app.py
======
Entry point for the Gauge V4 Configuration Tool (web-view UI).

    pip install -r requirements.txt
    python app.py            # add --debug for the WebView developer tools

Loads web/index.html in a native window (Edge WebView2 on Windows, WebKit on macOS)
and exposes api.Api to the page as ``window.pywebview.api``.
"""

from __future__ import annotations

import mimetypes
import os
import sys

import webview

from api import Api
from gui_model import WINDOW_TITLE

DARK_BG = "#1B1A17"  # matches --color-bg in web/theme.css so the window never flashes white


def resource(*parts: str) -> str:
    """Path to a bundled file, in a source checkout or a PyInstaller build."""
    base = getattr(sys, "_MEIPASS", os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(base, *parts)


def main() -> None:
    # Windows can map .js to text/plain in the registry, and WebView2 refuses to run an
    # ES module served with that type. Pin the types the built-in server sends.
    mimetypes.add_type("text/javascript", ".js")
    mimetypes.add_type("text/css", ".css")
    mimetypes.add_type("font/woff2", ".woff2")
    api = Api()
    window = webview.create_window(
        WINDOW_TITLE,
        url=resource("web", "index.html"),
        js_api=api,
        width=1280, height=860, min_size=(800, 600),
        background_color=DARK_BG,
        text_select=True,
    )
    api._attach(window)
    window.events.closing += api._on_closing  # restore test sources, stop tach test, disconnect
    # The page uses ES modules, which WebView2 refuses to load from file:// URLs,
    # so serve web/ from pywebview's built-in local HTTP server.
    webview.start(http_server=True, debug="--debug" in sys.argv)


if __name__ == "__main__":
    main()
