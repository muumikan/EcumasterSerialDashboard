# Test results

Newest first. Record what was actually observed, not what was expected — an
entry that only says "worked" is worth very little later.

---

## 2026-09-19 — second service-page session, three defects

Observed by using the dashboard, all three fixed in this session. The logs page
itself worked well: the listing, the downloads and opening the files in the
Client all behaved.

- **The SETUP page rebooted the dashboard on the Limits category.** Opening it
  panicked and restarted the board, every time, with nothing printed. The cause
  is LVGL's fixed pool: `lv_obj_create` returns NULL when it is full and LVGL
  then dereferences that NULL one call later, with no assert and no log, so an
  out-of-memory looks exactly like a wild pointer. Limits is eighteen rows of
  about eight objects each, built in one go, and the IDLE and BOOST pages added
  on 2026-09-14 had taken enough of the pool to put it over. The rows are a
  fixed pool of eight now, re-labelled rather than rebuilt, so the page costs
  the same whatever is on it; `LV_MEM_SIZE` went from 96 to 128 kB for margin,
  and what is left of the pool is on the service page's Diagnostics tab.

  The run on 2026-09-08 listed "whether the LVGL object pool holds the Limits
  category" as a thing to check. It did not.

- **Every write the service page accepts was refused.** Deleting a log said
  "the dashboard refused the request"; changing a limit or enabling an alarm
  failed the same way. Both are the 409 from `writable()`, which asked
  `snapshot().rpm >= kEngineRunningRpm` with no regard for the link. Killing
  the ignition cuts the ECU's power mid-frame, so the snapshot holds the last
  rpm it ever saw - the same freeze the engine-off summary was fixed for on
  2026-09-08 - and the dashboard spent the whole session believing an engine
  that had been idling at 850 rpm was still running. It uses `engineStopped()`
  now, which both this and the access point read from one place.

  It had worked before because it depends on how the car was shut down: with
  the ECU still powered, the last frame says 0 rpm.

- **The refusal did not say why.** The page read the 409's plain text as JSON,
  which threw, and reported a generic refusal - so the one message that would
  have named the cause was thrown away. Every write now shows what the server
  actually said.

Added in the same session, both asked for after living with the logs:

- Logs are written into a folder per day, `/20260919/1732_04.emulog`. Files
  already in the root are left there and still listed.
- The multi-file `.tar` is named for the moment it was fetched, since a tuning
  day means fetching several.

### Worth checking on the next run

- SETUP: that the Limits and Alarms categories open, that a swipe up and down
  walks a long category, and - the thing this page lost when it stopped being
  an LVGL scroller - that a vertical swipe starting on a `+` or `-` does not
  land as a press on it.
- Deleting a log, now that it can be reached at all, and that the day's folder
  disappears with its last file.
- That the dashboard still has heap to spare with the radio up: the service
  page reports free heap and the LVGL pool side by side under Diagnostics.

---

## 2026-09-16 — service page, first run on the car

Working on the hardware, observed: the access point comes up once `Service AP`
is switched on in Setup (it is off by default, and the settings version bump
means it starts off after this update), the page loads, a log downloads, and
the clock sets from the browser.

Fixed in the same session, both found by using it:

- **The access point could not explain itself.** Five conditions hold the radio
  down and none printed anything unless it was taking down an access point that
  had already come up. An access point that never appears looked exactly like a
  firmware without the feature. `announce()` now prints a `service` line with
  the current reason.
- **The settings page fought back.** The panel never repainted a value changed
  from the web, and the web form rebuilt itself after every edit, throwing away
  focus and scroll position.

**Also fixed in the tidy-up afterwards**

- **The `Bad frames` counter had never worked.** `EngineDataModel::noteBadFrames`
  was added with its field and its reader in `4c0b53b`, but the call site was
  never written, so nothing ever put the adapter's splice and reject counts into
  the model. The DIAG page and the service page both showed `0`, in green,
  whatever the link was actually doing. Worth knowing when reading the results
  below: every `Bad frames: 0` recorded before 16 September 2026 means nothing.

**Still to check on the car**

- **A multi-file `.tar`.** The ustar headers are written by hand and the
  `Content-Length` is computed in a separate pass from the one that writes the
  bytes; if those two disagree by one byte the browser truncates or hangs.
  Select two or three, download, extract, and open **each** in the Client.
- **Selecting every file when there are many.** The names go to `/all.tar` in
  the query string. Thirty files is about a kilobyte of URL, and where the
  request line stops being accepted has not been established.
- **Deleting.** Never once run. Check that a throwaway file goes, that the list
  refreshes, and that the file currently being written cannot be selected.
- **Starting the engine mid-download.** The case the whole design leans on: the
  access point should drop, logging resume, and `framesDropped()` stay at zero.
  Read the count from the console afterwards rather than assuming. The DIAG
  page's `Bad frames` is now worth reading too - it counted nothing until this
  session.
- **The idle timeout.** Fifteen minutes with nobody connected should take the
  radio down, and it should stay down until the engine runs again. This is the
  guard standing between the feature and a flat battery overnight, and it is
  entirely unverified.
- **The voltage floor.** Below 12.0 V the access point should refuse to come up
  and say so on the console.
- **Rotation on engine stop.** The file for the drive just finished should be
  complete and open in the Client, and a new one should start.
- **The session name.** Set one, confirm the next file carries it, and that
  something like `run 3/4` comes out as `run-3-4`.
- **Turning logging off and back on** without a reboot. Off should close the
  file properly - the last few seconds live in a RAM buffer - and on should
  open a new one.
- **Settings and clock across a power cycle.** The flash write is debounced four
  seconds after the last edit; the clock has to be held by the RTC.
- **The SERVICE AP block on DIAG fits.** Its position was worked out by
  arithmetic against the column height, not by looking at it. Confirm the key
  row is not clipped at the bottom, and that the network name turns green and
  gains a count when a laptop joins.

---

## 2026-09-16 — IDLE and BOOST pages, not yet on the car

Both environments build and link. Nothing below has been observed on hardware;
this entry exists so the list is written down before the next trip to the car
rather than reconstructed at it.

**To check on the car**

- **`idleAngleCorr` goes negative and reads correctly.** The whole reason the
  decoder fix was made. Unlike `IgnAngle`, this one *is* testable on this car:
  the idle loop trims timing to hold its target, so the IDLE page's `IDLE IGN`
  tile should sit somewhere around zero and move in both directions at a
  steady idle. A reading pinned near +125 means the fix did not take.
- **The idle target mark lands where the Client says it does.** Compare the
  bar's mark against Idle Target in EMU Classic Client on the laptop.
- **`IDLE CTL` follows the loop.** Should read `CLOSED` at idle and `OPEN` off
  throttle above idle, matching the Client's Idle Control Active.
- **The boost table set number matches the Client's.** It is shown raw, on the
  assumption the ECU numbers its sets the same way its own software displays
  them. That assumption is untested and this is the one item most likely to be
  wrong.
- **Both correction terms show a sign and change it.** `IDLE PID` and
  `BOOST PID` are signed; a reading that never goes below zero is suspect.
- **Eight page dots still clear the page name.** The dot pitch was tightened
  from 10 px to 8 px for this. Read on the panel, not in a screenshot.
- **The display is the right way up, and touch agrees with it.** The panel is
  now rotated 180 degrees so the USB socket faces the reachable side. The
  image is the easy half to confirm; the half worth actually testing is touch,
  because a transform that did not turn with the image would still register
  presses - just mirrored. Swipe the pages left and right, and press a Setup
  button near an edge rather than in the middle, where a mirrored hit and the
  intended one land in the same place.
- **The pages exist and are reachable** without the object pool running out -
  see the standing worry in [decision-log.md](decision-log.md).

**Cannot be tested on this car**

- **The classic-protocol `n/a` state.** The car runs the EDL-1 build, so the
  path where `controlChannels` is false is only exercised by flashing
  `crowpanel_advance_35`, which nothing else needs.

---

## 2026-09-10 — .emulog logging opens in EMU Classic Client

Environment `crowpanel_advance_35_edl`, first run of the rewritten `EmuLog`.

**Confirmed on the car**

- Logs written by the dashboard **open in EMU Classic Client and read the same
  as ones the Client recorded itself**. Reported as working exactly as it
  should.

That closes the format question outright. It also confirms, indirectly, three
things that had no test of their own: the ROM miniz compressor works on this
chip, the SD write path holds up across a whole session, and the RTC is
reachable from `AppController` after being moved out of `DashUi` — a log gets
its name from the clock, so a file that opened at all had a name to open under.

**Also confirmed the same day**

Four things that had been carried as untested since the first run on 8
September, all reported working:

- **Settings survive a power cycle.** The stale NVS record that blocked this
  test earlier has been overwritten, so it could finally be checked at all.
- **Backlight PWM, brightness and night mode.**
- **The Setup page's Limits category.** This is the screen that builds the
  most LVGL objects of any page, and it was the standing worry about object
  pool headroom. It behaves.
- **The check-engine bit numbering.** Confirmed by the intended test -
  unplugging the intake air sensor and watching which flag lights. The
  numbering was an inference until now; it is right.

**Cannot be tested on this car**

- **`IgnAngle` going negative.** One of the 22 decoder fixes, and the only one
  that reaches the screen. The ECU has to pull timing below zero to exercise
  it - knock retard, launch control, ALS - and this setup does not do that.
  The fix stands on Ecumaster's own format definition, which declares the
  channel `sbyte`, rather than on a measurement. Recorded so it stops
  appearing on test lists.

**Not yet exercised**

The four UI changes made after this build was flashed - the AFR tile, alarm
list scrolling, the black palette and the two alarms defaulting off - plus
what the open items in the decision log still list.

---

## 2026-09-09 — clock, alarm list and SD logging on the car

Firmware: `0e2ef5c`, environment `crowpanel_advance_35_edl`. First run of the
RTC, the Alarms page and SD logging on the hardware.

**Confirmed with the ECU connected**

- Values read correctly on screen and behave sensibly on all six pages.
- **The alarm list works on EDL-1.** Events land in the order they happened,
  with times, and the page reads the way it was designed to.
- This covers three of the items the 8 September entry listed as untested:
  alarm behaviour on a real engine, the run summary, and SD logging.

**The RTC seeding rule was wrong the first time**

The panel came up reading 00:24 while the firmware reading it had been compiled
at 19:44, so no seed had been written. The condition was the chip's VL flag
alone, and VL only says the oscillator stopped — it says nothing about whether
the time is right. With the backup cell fitted since the factory the clock had
been running the whole time and had never been set, VL clear throughout.

Fixed by adding a plausibility test: a clock reading earlier than the build it
is being read by has never been set. Verified on the next boot — the clock came
up correct and, on subsequent boots, was left alone rather than reseeded.

**SD logging: works, but mounted intermittently and the cause is unknown**

- With no card, `sdCommand(): Card Failed! cmd: 0x00` at CMD0, as expected.
- With a card, the same build over four consecutive resets: mount, fail,
  mount, fail. Card, wiring and filesystem are constant across those four, so
  none of them is the variable.
- Lowering the SPI clock from 80 MHz to 25 MHz **did not change it**. That was
  still worth doing — 80 MHz is outside what SD in SPI mode supports — but it
  was a fix for a different problem.
- A later session of eleven consecutive resets all mounted, every one on the
  **first** attempt, so the retry added in between never ran and cannot be
  credited. Something outside the firmware changed; card seating is the guess.
  Recorded as an open item rather than a closed one.

The file-number sequence is what proves the eleven: visible banners showed
00009, 00010, 00014, 00015, 00016 and 00019, and the gaps line up exactly with
the boots whose banner was lost, so those mounted too.

**Two smaller findings**

- **The console showed nothing after an upload.** All boot output is printed
  inside the first second, and a monitor attached afterwards misses it. The
  announce-on-connect added for this was not enough on its own: `Serial` reads
  as connected on this USB-CDC port before the host has re-opened it, so on
  about half the resets the reprint also went nowhere. Pressing any key now
  reprints the state, which does not depend on winning a race.
- **Stored settings were discarded.** `getBytes(): not enough space in buffer:
  132 < 136` — the record in NVS was written by a firmware whose `DashSettings`
  was four bytes larger. The size and version check rejected it and fell back
  to defaults, which is the designed behaviour. It stops once any setting is
  saved.

**Still untested**

- Whether the bad-frame count stays at zero while the card is being written —
  a long write during logging is the case that would push it.
- Whether the SD mount survives vibration in the car.
- Whether settings survive a power cycle (the stale record above has to be
  overwritten once before this can be checked at all).
- Backlight PWM, brightness and night mode.
- Whether the log file opens in EMU Classic Client. It does not, and the
  format question is still open.

---

## 2026-09-08 — EDL-1 link, after the framing fix

Firmware: `4c0b53b`, environment `crowpanel_advance_35_edl`. ECU reconfigured
to stream the EDL-1 logger protocol.

**Confirmed**

- The EDL-1 stream decodes correctly. Values read right on screen.
- **Bad frames stays at zero.** The 2048-byte receive buffer, the next-marker
  check and the plausibility gate together hold the link clean under normal
  running.
- Communication described as faultless over the session.

**What this replaces**

The first EDL-1 attempt showed bursts of impossible values across several
fields at once — CLT max 27472 C, oil pressure 15.9 bar, invented CEL flags.
Root cause was the 256-byte default UART buffer being smaller than one
260-byte frame. See the decision log.

**Still untested on EDL-1**

- Alarm behaviour across a real start, hysteresis, latching, run summary.
- Settings surviving a power cycle.
- Anything to do with SD logging.
- Whether the bad-frame count stays at zero under load — a long card write or
  a settings save is the case that would push it.

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
have never seen a real engine: whether the arming delay is long enough for oil
pressure to build on this engine, and whether the invented thresholds are
anywhere near right. Both are adjustable on the Setup page now, so the next
drive is the place to find out rather than a reflash.

---

## 2026-09-08 (later) — findings from the first run

Observed on the car and fixed in the same session:

- **The engine-off summary never appeared.** Cutting the ignition kills the ECU
  before RPM falls, so the last frame freezes at a few hundred rpm and the
  summary's trigger never fired. It now also ends the run on link loss, and is
  checked every pass rather than only when a frame arrives.
- **The idle return to Drive was unwanted.** It moved the screen away while a
  page was still being read. Removed; pages change only by swipe.
- **Three seconds of arming delay was too short.** Raised to six.
- **The boot sweep was over before you could look at it.** 900 ms to 2600 ms.
- **Cells looked washed out.** Panels darkened and the dividing lines lifted, so
  the numerals read against near-black.

Defaults corrected from what the car actually does: fuel pressure warn 3.2 to
2.2 bar and critical 2.6 to 1.9 bar, shift first light 3 500 to 1 000 rpm, red
zone 6 800 to 6 000.

Note that the settings blob version was bumped, so **the first boot after this
update discards any settings stored on the device** and comes up on the new
defaults.
