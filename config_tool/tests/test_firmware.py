"""
tests/test_firmware.py
======================
Unit tests for intel_hex and firmware_uploader
(CONFIG_TOOL_SPECIFICATION.md §13).

Run with: python -m pytest config_tool/tests/
"""

import os
import sys

# Allow importing from the config_tool package regardless of how the tests are invoked
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.dirname(__file__))

import pytest

import firmware_uploader as fu
import intel_hex
import stk500v2
from firmware_uploader import FirmwareError, FirmwareUploader
from intel_hex import HexError, parse_hex
from fake_bootloader import FakeBootloader, FakeSerial


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def hex_record(rectype: int, offset: int, payload: bytes) -> str:
    """Build one Intel HEX record with a correct checksum."""
    body = bytes([len(payload), (offset >> 8) & 0xFF, offset & 0xFF, rectype]) + payload
    checksum = (-sum(body)) & 0xFF
    return ":" + (body + bytes([checksum])).hex().upper()


def make_hex(image: bytes, start: int = 0) -> str:
    """Render *image* as Intel HEX text, emitting type-04 records past 64 KB."""
    lines = []
    base = -1
    for offset in range(0, len(image), 16):
        address = start + offset
        upper = address >> 16
        if upper != base:
            base = upper
            lines.append(hex_record(0x04, 0, bytes([(upper >> 8) & 0xFF, upper & 0xFF])))
        lines.append(hex_record(0x00, address & 0xFFFF, image[offset:offset + 16]))
    lines.append(":00000001FF")
    return "\n".join(lines) + "\n"


def write(tmp_path, name: str, text: str) -> str:
    path = tmp_path / name
    path.write_text(text)
    return str(path)


SMALL_IMAGE = bytes((i * 3 + 1) & 0xFF for i in range(512))


# ---------------------------------------------------------------------------
# Intel HEX parsing
# ---------------------------------------------------------------------------
class TestParseHex:
    def test_round_trips_a_simple_image(self, tmp_path) -> None:
        path = write(tmp_path, "fw.hex", make_hex(SMALL_IMAGE))
        assert bytes(parse_hex(path)) == SMALL_IMAGE

    def test_honours_extended_linear_address(self, tmp_path) -> None:
        """Data past 64 KB must land at its real address, not wrap to 0."""
        image = bytes([0xAB] * 32)
        path = write(tmp_path, "fw.hex", make_hex(image, start=0x12000))
        parsed = parse_hex(path)
        assert len(parsed) == 0x12000 + 32
        assert bytes(parsed[0x12000:]) == image
        assert set(parsed[:0x12000]) == {0xFF}

    def test_honours_extended_segment_address(self, tmp_path) -> None:
        """Real avr-gcc output uses type-02 records, not type-04."""
        text = (hex_record(0x02, 0, bytes([0x30, 0x00]))   # segment 0x3000 -> base 0x30000
                + chr(10) + hex_record(0x00, 0xE000, bytes([0x0D, 0x94]))
                + chr(10) + ":00000001FF" + chr(10))
        parsed = parse_hex(write(tmp_path, "fw.hex", text))
        assert len(parsed) == 0x3E002
        assert bytes(parsed[0x3E000:]) == bytes([0x0D, 0x94])

    def test_gaps_are_padded_with_erased_flash(self, tmp_path) -> None:
        text = (hex_record(0x00, 0x0000, b"\x01\x02")
                + "\n" + hex_record(0x00, 0x0010, b"\x03\x04")
                + "\n:00000001FF\n")
        parsed = parse_hex(write(tmp_path, "fw.hex", text))
        assert bytes(parsed[:2]) == b"\x01\x02"
        assert set(parsed[2:0x10]) == {0xFF}
        assert bytes(parsed[0x10:0x12]) == b"\x03\x04"

    def test_ignores_start_address_records(self, tmp_path) -> None:
        text = (hex_record(0x05, 0, b"\x00\x00\x00\x00")
                + "\n" + hex_record(0x00, 0, b"\x42")
                + "\n:00000001FF\n")
        assert bytes(parse_hex(write(tmp_path, "fw.hex", text))) == b"\x42"

    def test_rejects_non_hex_content(self, tmp_path) -> None:
        with pytest.raises(HexError, match="not Intel HEX"):
            parse_hex(write(tmp_path, "fw.hex", "hello world\n"))

    def test_rejects_bad_checksum(self, tmp_path) -> None:
        good = make_hex(SMALL_IMAGE).splitlines()
        good[1] = good[1][:-2] + "00"          # clobber one checksum byte
        with pytest.raises(HexError, match="corrupt"):
            parse_hex(write(tmp_path, "fw.hex", "\n".join(good) + "\n"))

    def test_rejects_truncated_file(self, tmp_path) -> None:
        text = "\n".join(make_hex(SMALL_IMAGE).splitlines()[:-1]) + "\n"
        with pytest.raises(HexError, match="truncated"):
            parse_hex(write(tmp_path, "fw.hex", text))

    def test_rejects_data_after_eof(self, tmp_path) -> None:
        text = make_hex(SMALL_IMAGE) + hex_record(0x00, 0, b"\x99") + "\n"
        with pytest.raises(HexError, match="after the end-of-file"):
            parse_hex(write(tmp_path, "fw.hex", text))

    def test_rejects_unsupported_record_type(self, tmp_path) -> None:
        text = hex_record(0x06, 0, b"\x00") + "\n:00000001FF\n"
        with pytest.raises(HexError, match="record type 0x06"):
            parse_hex(write(tmp_path, "fw.hex", text))

    def test_rejects_bad_byte_count(self, tmp_path) -> None:
        with pytest.raises(HexError, match="byte-count"):
            parse_hex(write(tmp_path, "fw.hex",
                            ":0F0000000C9434000C943E000C943E000C943E0083\n:00000001FF\n"))


# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------
class TestValidate:
    """Safety checks run before anything is written to the board."""

    def test_accepts_a_normal_image(self, tmp_path) -> None:
        assert fu.validate(write(tmp_path, "fw.hex", make_hex(SMALL_IMAGE))) == []

    def test_rejects_missing_file(self, tmp_path) -> None:
        with pytest.raises(FirmwareError, match="not found"):
            fu.validate(str(tmp_path / "nope.hex"))

    def test_rejects_empty_file(self, tmp_path) -> None:
        with pytest.raises(FirmwareError, match="empty"):
            fu.validate(write(tmp_path, "fw.hex", ""))

    def test_rejects_non_hex_extension(self, tmp_path) -> None:
        with pytest.raises(FirmwareError, match="must be a .hex"):
            fu.validate(write(tmp_path, "fw.bin", make_hex(SMALL_IMAGE)))

    def test_rejects_image_that_would_hit_the_bootloader(self, tmp_path) -> None:
        """A with_bootloader image extends past the application area."""
        oversize = make_hex(b"\x00" * 64, start=fu.APP_MAX_BYTES)
        with pytest.raises(FirmwareError, match="with_bootloader"):
            fu.validate(write(tmp_path, "fw.hex", oversize))

    def test_warns_when_nearly_full(self, tmp_path) -> None:
        big = make_hex(b"\x5A" * (fu.FLASH_WARN_BYTES + 1024))
        warnings = fu.validate(write(tmp_path, "fw.hex", big))
        assert any("available" in w for w in warnings)


# ---------------------------------------------------------------------------
# Finding a bundled firmware image
# ---------------------------------------------------------------------------
class TestFindFirmware:
    def test_finds_hex_in_firmware_subfolder(self, tmp_path, monkeypatch) -> None:
        folder = tmp_path / "firmware"
        folder.mkdir()
        (folder / "gauge_V4.hex").write_text(make_hex(SMALL_IMAGE))
        monkeypatch.setattr(fu, "app_dir", lambda: str(tmp_path))
        assert os.path.basename(fu.find_firmware()) == "gauge_V4.hex"

    def test_finds_hex_beside_the_executable(self, tmp_path, monkeypatch) -> None:
        (tmp_path / "gauge_V4.hex").write_text(make_hex(SMALL_IMAGE))
        monkeypatch.setattr(fu, "app_dir", lambda: str(tmp_path))
        assert fu.find_firmware() is not None

    def test_skips_with_bootloader_images(self, tmp_path, monkeypatch) -> None:
        (tmp_path / "gauge_V4.ino.with_bootloader.hex").write_text(make_hex(SMALL_IMAGE))
        monkeypatch.setattr(fu, "app_dir", lambda: str(tmp_path))
        assert fu.list_firmware() == []

    def test_a_file_is_offered_only_once(self, tmp_path, monkeypatch) -> None:
        """Search folders overlap; on Windows firmware/ and Firmware/ are one folder."""
        folder = tmp_path / "firmware"
        folder.mkdir()
        (folder / "gauge_V4.hex").write_text(make_hex(SMALL_IMAGE))
        monkeypatch.setattr(fu, "app_dir", lambda: str(tmp_path))
        assert len(fu.list_firmware()) == 1

    def test_returns_none_when_nothing_is_bundled(self, tmp_path, monkeypatch) -> None:
        monkeypatch.setattr(fu, "app_dir", lambda: str(tmp_path))
        assert fu.find_firmware() is None

    def test_newest_file_wins(self, tmp_path, monkeypatch) -> None:
        old = tmp_path / "old.hex"
        new = tmp_path / "new.hex"
        old.write_text(make_hex(SMALL_IMAGE))
        new.write_text(make_hex(SMALL_IMAGE))
        os.utime(str(old), (1000, 1000))
        os.utime(str(new), (2000, 2000))
        monkeypatch.setattr(fu, "app_dir", lambda: str(tmp_path))
        assert os.path.basename(fu.find_firmware()) == "new.hex"

    def test_frozen_app_looks_beside_the_exe(self, tmp_path, monkeypatch) -> None:
        """A PyInstaller build must not look in its temp extraction folder."""
        exe = tmp_path / "GaugeConfig.exe"
        exe.write_text("")
        monkeypatch.setattr(sys, "frozen", True, raising=False)
        monkeypatch.setattr(sys, "executable", str(exe))
        assert fu.app_dir() == str(tmp_path)


# ---------------------------------------------------------------------------
# End-to-end upload against the simulated bootloader
# ---------------------------------------------------------------------------
class TestUpload:
    @staticmethod
    def _wire(monkeypatch, boot: FakeBootloader) -> None:
        monkeypatch.setattr(stk500v2, "HAS_SERIAL", True)
        monkeypatch.setattr(
            stk500v2, "serial",
            type("M", (), {"Serial": lambda *a, **k: FakeSerial(boot)}), raising=False)

    def test_flashes_and_verifies(self, tmp_path, monkeypatch) -> None:
        boot = FakeBootloader()
        self._wire(monkeypatch, boot)
        path = write(tmp_path, "fw.hex", make_hex(SMALL_IMAGE))
        logs, progress = [], []
        FirmwareUploader().upload(path, "COM_FAKE",
                                  log_cb=logs.append, progress_cb=progress.append)
        assert bytes(boot.flash[:len(SMALL_IMAGE)]) == SMALL_IMAGE
        assert progress[-1] == pytest.approx(100.0)
        assert any("Verified" in line for line in logs)
        assert boot.in_progmode is False        # left cleanly so the sketch runs

    def test_progress_is_monotonic(self, tmp_path, monkeypatch) -> None:
        boot = FakeBootloader()
        self._wire(monkeypatch, boot)
        path = write(tmp_path, "fw.hex", make_hex(bytes(2048)))
        progress = []
        FirmwareUploader().upload(path, "COM_FAKE", progress_cb=progress.append)
        assert progress == sorted(progress)
        assert max(progress) == pytest.approx(100.0)

    def test_wrong_board_is_refused_before_writing(self, tmp_path, monkeypatch) -> None:
        """An ATmega328P must not be flashed with a Mega 2560 image."""
        boot = FakeBootloader(signature=(0x1E, 0x95, 0x0F))
        self._wire(monkeypatch, boot)
        path = write(tmp_path, "fw.hex", make_hex(SMALL_IMAGE))
        with pytest.raises(FirmwareError, match="ATmega328P"):
            FirmwareUploader().upload(path, "COM_FAKE")
        assert boot.written_pages == []

    def test_verification_failure_is_reported(self, tmp_path, monkeypatch) -> None:
        boot = FakeBootloader()
        self._wire(monkeypatch, boot)

        original = boot._dispatch
        corrupted = []

        def flaky(body):
            reply = original(body)
            if body[0] == stk500v2.CMD_PROGRAM_FLASH_ISP and not corrupted:
                corrupted.append(True)
                boot.flash[0] ^= 0xFF          # one cell that will not take
            return reply

        boot._dispatch = flaky
        path = write(tmp_path, "fw.hex", make_hex(SMALL_IMAGE))
        with pytest.raises(FirmwareError, match="Verification failed"):
            FirmwareUploader().upload(path, "COM_FAKE")

    def test_bad_file_is_rejected_before_opening_the_port(self, tmp_path) -> None:
        path = write(tmp_path, "fw.hex", "not hex\n")
        with pytest.raises(FirmwareError):
            FirmwareUploader().upload(path, "COM_DOES_NOT_EXIST")


# ---------------------------------------------------------------------------
# Cross-check against the canonical toolchain, when it happens to be installed
# ---------------------------------------------------------------------------
def _arduino_hex_files():
    """Real avr-gcc-generated .hex files from an Arduino AVR core, if present."""
    import glob
    roots = glob.glob(os.path.join(
        os.path.expanduser("~"), "AppData", "Local", "Arduino15", "packages",
        "arduino", "hardware", "avr", "*", "bootloaders"))
    roots += glob.glob(os.path.join(
        os.path.expanduser("~"), ".arduino15", "packages", "arduino",
        "hardware", "avr", "*", "bootloaders"))
    files = []
    for root in roots:
        files.extend(glob.glob(os.path.join(root, "**", "*.hex"), recursive=True))
    return sorted(files)


ARDUINO_HEX = _arduino_hex_files()


@pytest.mark.skipif(not ARDUINO_HEX, reason="no Arduino AVR core installed")
class TestRealToolchainOutput:
    """Parse genuine avr-gcc output, not just hex we generated ourselves."""

    def test_every_shipped_bootloader_parses(self) -> None:
        for path in ARDUINO_HEX:
            assert len(parse_hex(path)) > 0, path

    def test_mega2560_bootloader_lands_at_its_real_address(self) -> None:
        mega = [p for p in ARDUINO_HEX if "mega2560" in os.path.basename(p).lower()]
        if not mega:
            pytest.skip("stk500boot mega2560 image not present")
        image = parse_hex(mega[0])
        # The Mega 2560 bootloader occupies the top 8 KB of the 256 KB flash.
        first_data = next(i for i, b in enumerate(image) if b != 0xFF)
        assert first_data == 0x3E000
