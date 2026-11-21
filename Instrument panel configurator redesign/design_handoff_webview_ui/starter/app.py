"""Starter entry point for the web-view config tool (replaces `python gui.py`).

    pip install pywebview
    python app.py

Loads web/index.html in a native window (Edge WebView2 on Windows, WebKit on
macOS) and exposes `Api` to JavaScript as `window.pywebview.api`.
"""
from __future__ import annotations

import os
import sys

import webview

from api import Api
from gui_model import TOOL_VERSION  # constants/helpers moved out of gui.py


def resource(*parts: str) -> str:
    base = getattr(sys, "_MEIPASS", os.path.dirname(os.path.abspath(__file__)))  # PyInstaller
    return os.path.join(base, *parts)


def main() -> None:
    api = Api()
    window = webview.create_window(
        f"Gauge V4 Configuration Tool v{TOOL_VERSION}",
        url=resource("web", "index.html"),
        js_api=api,
        width=1280, height=860, min_size=(800, 600),
        background_color="#1B1A17",
    )
    api.attach(window)
    window.events.closing += api.on_closing  # restore test sources, stop tach test, disconnect
    webview.start(debug="--debug" in sys.argv)


if __name__ == "__main__":
    main()
