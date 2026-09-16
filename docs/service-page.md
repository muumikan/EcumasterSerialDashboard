# Service page

The dashboard raises a WiFi access point while the engine is stopped and serves
a maintenance page on it. The logs are read on the tuning laptop, which comes to
the car anyway, so they go to it directly rather than through anywhere else.

## Joining

| | |
| --- | --- |
| SSID | `EcuDash` |
| Password | `ecudash1` (WPA2) |
| Address | `http://192.168.4.1/` |

These are compile-time constants in `include/service_config.hpp`. The panel has a
plus and a minus button and cannot enter text, so they are not editable there.

The network and the key are also shown on the dashboard's own **DIAG** page,
under the peaks, so they can be read while standing beside the car rather than
looked up. The network name turns green and gains a count once a laptop has
actually joined - which is the difference between "the radio is up" and "I am
on it". The address is not shown there: it is always `192.168.4.1`, and the page
had room for two rows.

The status bar carries the short form on every page: `WIFI`, or `WIFI 1` once
someone has joined. It is blank whenever the radio is off, which is whenever the
car is moving.

## When the access point is up

Turn `Service AP` on under `Log` on the setup page - it is off by default,
because a radio that comes up on its own in a parked car should be asked for.

Then all of these have to hold:

- the engine is stopped: engine speed below the running threshold, or the ECU
  link silent, continuously for ten seconds;
- battery voltage at or above 12.0 V, tested only while the ECU is still
  reporting - with the link quiet there is no reading to test, and refusing on a
  value we do not have would mean the access point never appearing with the
  ignition off.

It goes down again the moment the engine starts, if the voltage falls below the
floor, or after fifteen minutes with nobody connected. Once it has gone down for
idleness it stays down until the engine runs again: a parked car gets one window
per drive rather than a radio that cycles all night.

## Why the engine has to be stopped

With the engine off the ECU sends nothing, so nothing is written to the card
while the server reads from it. That is what keeps the whole feature inside the
existing cooperative loop: no second task, no mutex, no card shared between two
cores. The one failure mode that would actually matter - a corrupted log -
cannot arise.

It also means the live channel view that would help most when chasing a decoder
bug is not possible here, since there is no data to show. That is the trade.

## The page

**Logs.** Every `.emulog` on the card with its size and a checkbox. One selected
file downloads as itself; several come down as a single `.tar`, uncompressed
because the files are already gzip. The file currently being written is listed
but cannot be selected or deleted - it is still open.

Deleting asks first and names what it is about to remove. There is no "delete
everything" form: the page confirms a list and the server acts on that list.

**Name the next drive.** Text appended to the next log file's name, so a tuning
day's runs can be told apart. Anything outside `[A-Za-z0-9_-]` is replaced.

**Settings.** Changes apply to the dashboard the moment the server accepts them
- the page writes into the same `DashSettings` the panel edits, then the owner
re-applies alarm limits and brightness and repaints the SETUP page's rows. The
flash write is debounced by four seconds after the last edit, the same as a held
button on the panel, so a change made and then powered off inside four seconds is
lost. The value that comes back from the server is the clamped one and may differ
from what was typed.

The same fields the panel edits, generated from the shared table in
`include/setup_items.hpp`, clamped by the same ranges and applied through the
same path. Neither editor can drift from the other, and neither is a way around
the rule that only numbers and switches are editable - which value an alarm
watches stays in code. Fields differing from the built-in defaults are marked.

**Diagnostics.** What `announce()` prints to a console nobody can reach in a car,
plus the clock. The dashboard has no network and no GPS, so its clock is seeded
from the build stamp and drifts; one button sets it from the browser's own clock,
which needs no internet and is right to about a second.

**Alarms.** The event log, newest first - what tripped, when, and at what engine
speed. Unavailable if the display failed to come up, since the alarm engine lives
with the UI; the page says so rather than showing an empty list that would look
like a clean run.

## Notes for whoever changes this

- The page is one PROGMEM string in `src/web/service_page_html.cpp`. This network
  has no route to the internet, so nothing can be loaded from a CDN: all the CSS
  and script live in that string, and system fonts do the typography.
- Writes are POSTs and are refused with 409 while the engine is running. That
  cannot happen anyway - the access point is down by then - but the server does
  not rely on that to be safe.
- Downloads run LVGL between blocks (`ServiceContext::pump`), so the screen shows
  progress instead of freezing for the length of the transfer.
- Filenames from the client are checked against `safeName()` before anything is
  opened: no slashes, no `..`, and a `.emulog` suffix.
