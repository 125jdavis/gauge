# Updating Your Gauge Firmware

This guide is for gauge owners. You do not need to install anything — no Python,
no Arduino software. Everything needed is in the folder you were sent.

---

## What you were sent

```
GaugeConfig.exe        <- the program you run
firmware/
    gauge_V4_<date>.hex   <- the firmware update itself
```

Keep these together. The program looks in the `firmware` folder automatically.

---

## Before you start

- Have the gauge connected to your computer with a USB cable.
- Close any other program that might be using the gauge (a serial monitor, or a
  second copy of this tool).
- The update takes about a minute. **Do not unplug the gauge while it runs.**

---

## Updating

1. **Double-click `GaugeConfig.exe`.**

2. On the **Connection** tab, choose your gauge in the **Port** dropdown.
   It usually appears as something like `COM3 - USB Serial Device`. If you are
   not sure which one it is, unplug the gauge, press **Refresh**, note the list,
   plug it back in, press **Refresh** again — the one that appeared is your gauge.

3. Look at the **Firmware Update** box. The **Firmware file** should already be
   filled in with the `.hex` file you were sent. If it is empty, or you were sent
   a file separately, press **Browse...** and select it.

4. Press **Update Firmware**, then confirm.

5. Wait. The progress bar fills twice — once while writing, once while the tool
   checks its work. The log underneath shows what is happening.

6. When it says **Upload complete** and the gauge reconnects, you are done.

---

## If something goes wrong

Nothing here damages the gauge. If an update fails partway, the gauge will sit in
its update mode waiting — just fix the problem below and run the update again.

| What you see | What to do |
|--------------|------------|
| "The bootloader did not respond" | Check the USB cable. Make sure you picked the right port, and that no other program is using it. |
| "Could not open COM…" | Another program has the port. Close any serial monitor, then retry. |
| "This firmware is for the ATmega2560, but the board reports…" | The selected port is a different device, not your gauge. Pick the correct port. |
| "the file is corrupt" / "looks truncated" | The `.hex` did not arrive intact. Ask for a fresh copy. |
| "Verification failed at…" | Run the update again. If it fails a second time, get in touch. |
| The gauge does not respond afterwards | Unplug it, plug it back in, then press **Connect**. |

If you get stuck, send us the text from the log box — it tells us exactly where
things stopped.

---

## After updating

Your settings are stored separately from the firmware and are kept across an
update. It is still worth using **Save to file...** on the toolbar beforehand if
you have spent time tuning things, so you have a copy you can reload.
