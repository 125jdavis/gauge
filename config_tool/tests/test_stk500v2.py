"""
tests/test_stk500v2.py
======================
Protocol tests for the pure-Python bootloader client
(CONFIG_TOOL_SPECIFICATION.md §13), driven against a simulated stk500boot.

Run with: python -m pytest config_tool/tests/
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.dirname(__file__))

import pytest

import stk500v2
from stk500v2 import FLASH_PAGE_SIZE, STK500Error, STK500v2
from fake_bootloader import APP_END, FakeBootloader, FakeSerial


@pytest.fixture
def session(monkeypatch):
    """An opened STK500v2 client wired to a fake bootloader."""
    boot = FakeBootloader()

    def fake_serial(port, baud, timeout=None):
        return FakeSerial(boot)

    monkeypatch.setattr(stk500v2, "HAS_SERIAL", True)
    monkeypatch.setattr(stk500v2, "serial",
                        type("M", (), {"Serial": fake_serial}), raising=False)
    client = STK500v2("COM_FAKE")
    client.open()
    return client, boot


class TestHandshake:
    def test_sign_on_succeeds(self, session) -> None:
        client, _ = session
        assert client._ser is not None

    def test_reset_toggles_dtr_rts(self, session) -> None:
        """The board is reset by dropping and raising DTR/RTS."""
        client, _ = session
        assert client._ser.dtr is True and client._ser.rts is True

    def test_signature_is_read(self, session) -> None:
        client, boot = session
        assert client.read_signature() == (0x1E, 0x98, 0x01)

    def test_no_bootloader_raises(self, monkeypatch) -> None:
        """A silent port produces a clear error, not a hang."""
        class Silent(FakeSerial):
            def write(self, data): return len(data)

        monkeypatch.setattr(stk500v2, "HAS_SERIAL", True)
        monkeypatch.setattr(stk500v2, "serial",
                            type("M", (), {"Serial": lambda *a, **k: Silent(FakeBootloader())}),
                            raising=False)
        monkeypatch.setattr(stk500v2, "SYNC_ATTEMPTS", 2)
        with pytest.raises(STK500Error, match="did not respond"):
            STK500v2("COM_FAKE").open()


class TestAddressing:
    """The bootloader shifts the word address left by one to get bytes."""

    def test_byte_address_round_trips(self, session) -> None:
        client, boot = session
        client.load_address(0x1234)
        assert boot.address == 0x1234

    def test_address_above_64k_is_not_truncated(self, session) -> None:
        """Past 64 KB the extended-address flag must not corrupt the address."""
        client, boot = session
        client.load_address(0x1F000)
        assert boot.address == 0x1F000

    def test_address_near_end_of_flash(self, session) -> None:
        client, boot = session
        client.load_address(APP_END - FLASH_PAGE_SIZE)
        assert boot.address == APP_END - FLASH_PAGE_SIZE


class TestProgramAndVerify:
    def test_small_image_round_trips(self, session) -> None:
        client, boot = session
        image = bytes(range(256)) * 4          # 1 KB, 4 pages
        client.enter_progmode()
        client.program_flash(image)
        assert bytes(boot.flash[:len(image)]) == image
        client.verify_flash(image)             # must not raise

    def test_image_is_padded_to_a_whole_page(self, session) -> None:
        client, boot = session
        image = b"\xAA" * 300                  # 1 page + 44 bytes
        client.enter_progmode()
        client.program_flash(image)
        assert bytes(boot.flash[:300]) == image
        assert bytes(boot.flash[300:FLASH_PAGE_SIZE * 2]) == b"\xFF" * (512 - 300)

    def test_pages_are_written_in_order_from_zero(self, session) -> None:
        """Required: the bootloader erases by its own cursor, not the address."""
        client, boot = session
        client.enter_progmode()
        client.program_flash(b"\x5A" * (FLASH_PAGE_SIZE * 5))
        expected = [i * FLASH_PAGE_SIZE for i in range(5)]
        assert boot.written_pages == expected
        assert boot.erased_pages == expected

    def test_large_image_crossing_64k_round_trips(self, session) -> None:
        """A realistic Mega sketch spans the 64 KB boundary."""
        client, boot = session
        image = bytes((i * 7 + 3) & 0xFF for i in range(70 * 1024))
        client.enter_progmode()
        client.program_flash(image)
        client.verify_flash(image)
        assert bytes(boot.flash[:len(image)]) == image

    def test_progress_reaches_100(self, session) -> None:
        client, _ = session
        seen = []
        client.enter_progmode()
        client.program_flash(b"\x00" * (FLASH_PAGE_SIZE * 4), progress_cb=seen.append)
        assert seen[-1] == pytest.approx(100.0)
        assert seen == sorted(seen)

    def test_verify_detects_corruption(self, session) -> None:
        client, boot = session
        image = b"\x11" * (FLASH_PAGE_SIZE * 2)
        client.enter_progmode()
        client.program_flash(image)
        boot.flash[FLASH_PAGE_SIZE + 5] = 0x00      # simulate a bad cell
        with pytest.raises(STK500Error, match="Verification failed at 0x00105"):
            client.verify_flash(image)


class TestFraming:
    def test_bad_reply_checksum_is_caught(self, session) -> None:
        client, boot = session
        original = boot.handle_frame

        def corrupt(frame):
            reply = bytearray(original(frame))
            reply[-1] ^= 0xFF
            return bytes(reply)

        boot.handle_frame = corrupt
        with pytest.raises(STK500Error, match="checksum"):
            client.read_signature()

    def test_out_of_sequence_reply_is_caught(self, session) -> None:
        client, boot = session
        original = boot.handle_frame

        def wrong_seq(frame):
            reply = bytearray(original(frame))
            reply[1] = (reply[1] + 1) & 0xFF
            reply[-1] = boot._checksum(reply[:-1])   # keep the checksum valid
            return bytes(reply)

        boot.handle_frame = wrong_seq
        with pytest.raises(STK500Error, match="sequence"):
            client.read_signature()

    def test_startup_noise_is_skipped(self, session) -> None:
        """Stray bytes before a frame must not desynchronise the client."""
        client, boot = session
        original = boot.handle_frame
        boot.handle_frame = lambda frame: b"garbage\x00" + original(frame)
        assert client.read_signature() == (0x1E, 0x98, 0x01)
