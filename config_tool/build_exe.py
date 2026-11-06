"""
build_exe.py
============
Build the standalone Windows executable customers run.

Produces ``dist/GaugeConfig.exe`` — a single file with Python, Tk and pyserial
inside, so customers install nothing. Alongside it the script creates a
``dist/firmware/`` folder; drop the ``.hex`` you built for a customer in there
and the tool pre-selects it on startup.

Run with:  python build_exe.py
Requires:  pip install pyinstaller
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
APP_NAME = "GaugeConfig"
ENTRY_POINT = os.path.join(HERE, "gui.py")
DIST_DIR = os.path.join(HERE, "dist")

# pyserial picks its backend at runtime, so name every platform module here
# rather than trusting the dependency scan to follow the conditional imports.
HIDDEN_IMPORTS = [
    "serial.tools.list_ports",
    "serial.tools.list_ports_windows",
    "serial.tools.list_ports_posix",
    "serial.tools.list_ports_linux",
    "serial.tools.list_ports_osx",
]


def build() -> int:
    try:
        import PyInstaller  # noqa: F401
    except ImportError:
        print("PyInstaller is not installed.\n  pip install pyinstaller", file=sys.stderr)
        return 1

    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--noconfirm",
        "--clean",
        "--onefile",            # one .exe the customer can copy anywhere
        "--windowed",           # GUI app: no console window behind it
        "--name", APP_NAME,
        "--distpath", DIST_DIR,
        "--workpath", os.path.join(HERE, "build_pyinstaller"),
        "--specpath", os.path.join(HERE, "build_pyinstaller"),
    ]
    for module in HIDDEN_IMPORTS:
        cmd += ["--hidden-import", module]
    icon = os.path.join(HERE, "gauge.ico")
    if os.path.isfile(icon):
        cmd += ["--icon", icon]
    cmd.append(ENTRY_POINT)

    print("> " + " ".join(cmd))
    result = subprocess.run(cmd)
    if result.returncode != 0:
        return result.returncode

    # The tool looks here first, so give the customer an obvious place to put
    # the .hex — and ship one if a build is sitting in the repo.
    firmware_dir = os.path.join(DIST_DIR, "firmware")
    os.makedirs(firmware_dir, exist_ok=True)
    staged = _stage_firmware(firmware_dir)

    exe = os.path.join(DIST_DIR, APP_NAME + (".exe" if os.name == "nt" else ""))
    print("\nBuilt:", exe)
    print("Firmware folder:", firmware_dir)
    if staged:
        print("Staged firmware:", ", ".join(staged))
    else:
        print("No .hex found to stage — copy the customer's firmware into that folder.")
    return 0


def _stage_firmware(firmware_dir: str) -> list:
    """Copy any exported .hex from the sketch build folder next to the exe."""
    build_root = os.path.normpath(os.path.join(HERE, "..", "gauge_V4", "build"))
    staged = []
    for root, _dirs, files in os.walk(build_root):
        for name in files:
            if name.lower().endswith(".hex") and "with_bootloader" not in name.lower():
                shutil.copy2(os.path.join(root, name), os.path.join(firmware_dir, name))
                staged.append(name)
    return staged


if __name__ == "__main__":
    raise SystemExit(build())
