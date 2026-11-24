"""
build_firmware.py
=================
Compile the gauge firmware (``gauge_V4/``) into the ``.hex`` the config tool flashes.

Writes to ``gauge_V4/build/arduino.avr.mega/`` — the same folder the Arduino IDE's
"Sketch > Export Compiled Binary" uses — so build_exe.py and the config tool find it
in the usual place.

Run with:  python build_firmware.py
Requires:  arduino-cli with the arduino:avr core and the sketch's libraries installed
           (by default it shares the Arduino IDE's sketchbook libraries folder).
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SKETCH_DIR = os.path.normpath(os.path.join(HERE, "..", "gauge_V4"))
FQBN = "arduino:avr:mega:cpu=atmega2560"
OUTPUT_DIR = os.path.join(SKETCH_DIR, "build", "arduino.avr.mega")
HEX_PATH = os.path.join(OUTPUT_DIR, "gauge_V4.ino.hex")


def find_arduino_cli() -> str | None:
    """arduino-cli on PATH, else the usual Windows install locations."""
    found = shutil.which("arduino-cli")
    if found:
        return found
    candidates = [
        os.path.join(os.environ.get("ProgramFiles", r"C:\Program Files"),
                     "Arduino CLI", "arduino-cli.exe"),
        # Arduino IDE 2 ships its own copy.
        os.path.join(os.environ.get("LOCALAPPDATA", ""), "Programs", "Arduino IDE",
                     "resources", "app", "lib", "backend", "resources", "arduino-cli.exe"),
        os.path.join(os.environ.get("ProgramFiles", r"C:\Program Files"), "Arduino IDE",
                     "resources", "app", "lib", "backend", "resources", "arduino-cli.exe"),
    ]
    return next((path for path in candidates if os.path.isfile(path)), None)


def build() -> int:
    cli = find_arduino_cli()
    if cli is None:
        print("arduino-cli not found. Install it (https://arduino.github.io/arduino-cli/)\n"
              "or export the .hex from the Arduino IDE instead.", file=sys.stderr)
        return 1

    # --export-binaries writes the .hex into <sketch>/build/<fqbn as dots>/.
    cmd = [cli, "compile", "--fqbn", FQBN, "--export-binaries", SKETCH_DIR]
    print("> " + " ".join(cmd))
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print("\nFirmware build FAILED.", file=sys.stderr)
        return result.returncode
    if not os.path.isfile(HEX_PATH):
        print(f"\nCompile succeeded but {HEX_PATH} is missing.", file=sys.stderr)
        return 1
    print("\nFirmware:", HEX_PATH)
    return 0


if __name__ == "__main__":
    raise SystemExit(build())
