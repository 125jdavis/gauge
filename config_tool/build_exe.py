"""
build_exe.py
============
Build the standalone Windows executable customers run.

Produces ``dist/GaugeConfig.exe`` — a single file with Python, pywebview, pyserial
and the web UI (``web/``) inside, so customers install nothing. On Windows 10/11 the
window uses the Edge WebView2 runtime that ships with the OS. Alongside it the script creates a
``dist/firmware/`` folder; drop the ``.hex`` you built for a customer in there
and the tool pre-selects it on startup.

Before packaging, it compiles the firmware (build_firmware.py) so the .hex shipped
in ``dist/firmware/`` always matches the source in ``gauge_V4/``.

Run with:  python build_exe.py                  # compile firmware, then build the .exe
           python build_exe.py --skip-firmware  # ship the .hex already in gauge_V4/build/
Requires:  pip install pyinstaller, and arduino-cli (unless --skip-firmware)
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys

import build_firmware

HERE = os.path.dirname(os.path.abspath(__file__))
APP_NAME = "GaugeConfig"
ENTRY_POINT = os.path.join(HERE, "app.py")
WEB_DIR = os.path.join(HERE, "web")
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


def build(skip_firmware: bool = False) -> int:
    try:
        import PyInstaller  # noqa: F401
    except ImportError:
        print("PyInstaller is not installed.\n  pip install pyinstaller", file=sys.stderr)
        return 1

    if skip_firmware:
        print("Skipping firmware compile; shipping the .hex already in gauge_V4/build/.")
    else:
        status = build_firmware.build()
        if status != 0:
            print("Stopping: fix the firmware build, or run with --skip-firmware to ship "
                  "the existing .hex.", file=sys.stderr)
            return status

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
        # Name the source folder explicitly: when the build is started with a lowercase drive
        # letter (e.g. from VS Code), PyInstaller can miss it and leave out api/gui_model.
        "--paths", HERE,
    ]
    for module in HIDDEN_IMPORTS:
        cmd += ["--hidden-import", module]
    # The HTML/CSS/JS front end is loaded from disk at runtime (app.resource).
    cmd += ["--add-data", f"{WEB_DIR}{os.pathsep}web"]
    icon = os.path.join(HERE, "gauge.ico")
    if os.path.isfile(icon):
        # --icon sets the .exe's icon in Explorer; the bundled copy is what app.py
        # hands the window for its title bar and taskbar button.
        cmd += ["--icon", icon, "--add-data", f"{icon}{os.pathsep}."]
    else:
        print("gauge.ico not found - run make_icon.py first. Building without an icon.",
              file=sys.stderr)
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
    raise SystemExit(build(skip_firmware="--skip-firmware" in sys.argv))
