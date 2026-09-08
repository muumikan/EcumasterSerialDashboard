# Test results

Newest first. Record what was actually observed, not what was expected — an
entry that only says "worked" is worth very little later.

---

## 2026-09-08 — first run connected to the ECU

Firmware: `2bcd7a8` (five pages, settings in flash) on the CrowPanel Advance
3.5, ECU link through the MAX3232 on the EMU Classic extension port.

**Confirmed**

- Flashing over USB-C succeeds.
- The serial link comes up with the ECU connected.
- The user interface behaves logically — pages and navigation do what they
  are supposed to.
- Displayed values look correct as far as they have been checked against the
  ECU.

**Not tested yet**

- SD card logging. It is on the `feature/sd-logging` branch, which was not
  flashed.
- Alarm behaviour with a real engine running: the arming delay across a
  start, hysteresis on a value sitting at its limit, latching, and the
  engine-off run summary. All of it is untested against a real start.
- The Setup page under load: whether the LVGL object pool holds the Limits
  category, and whether vertical scrolling fights the horizontal page swipe.
- Backlight PWM, brightness and night mode.
- Whether settings actually survive a power cycle.

**Nothing to fix from this run.** No defects were observed.

### Worth checking on the next run

The two things most likely to be wrong are both alarm-related, because they
have never seen a real engine: whether three seconds of arming delay is
actually enough for oil pressure to build on this engine, and whether the
invented thresholds are anywhere near right. Both are adjustable on the Setup
page now, so the next drive is the place to find out rather than a reflash.
