# Changelog

Release notes of the multi-car firmware. The release workflow publishes the section of
the version in `VERSION`, and refreshes the notes of any other release listed here.
Earlier releases (up to 2026.10.8.2) have their notes on the
[releases page](https://github.com/darek-margas/esphome-tesla-ble-multi/releases).

## 2026.10.13.1 — Library v2026.10.13: stricter session handling

Firmware change: rebuild and flash. Library
[darek-margas/tesla-ble](https://github.com/darek-margas/tesla-ble) `v2026.10.13`
(roll back: `v2026.10.12`). No component code change, and no re-pairing.

### Fixed (in the library)
- **Stale session answers are ignored.** A session answer arriving more than 4 s after
  its request carries an out-of-date car clock, which could make later commands fail with
  TIME_EXPIRED. Such answers are now ignored, and a session request still unanswered after
  4 s is asked again with a new id. On a weak link (Bluey at −90 dBm) this replaces a
  failure later on with a quick retry.
- **The car's late answer during a retry wait is no longer lost:**
  - an expired-session fault makes the command rebuild under a new session, instead of
    resending its old bytes;
  - a refusal fails the command at once, instead of after more retries;
  - a late answer completes the command.
- **Replies from a dropped link are discarded:** when a link drops (as Bluey's did with
  `status 133`), nothing received on it is handled after the reconnect.

## 2026.10.12.2 — A car keeps its BLE turn until its command is answered

Firmware change: rebuild and flash. Library
[darek-margas/tesla-ble](https://github.com/darek-margas/tesla-ble) **`v2026.10.12`**
(roll back: `v5.2.0-dm.10` with component 2026.10.12.1). No re-pairing.

### Fixed
- **A turn could end while the car's command was still waiting for its reply.** On a weak
  link, a VCSEC poll could still be waiting for an answer when the turn logic considered
  the car quiet, and the turn passed to the other car. The poll then failed after the
  hand-over, with the confusing error `Failed to build BLE write failed command`. A car now
  keeps its turn while the library still has a command pending, up to the existing 60 s
  turn limit, so the other car is never held up for longer than before.

### Changed
- **The library's releases are now tagged by date** (`v2026.10.12`). It is an independent
  library derived from yoziru/tesla-ble v5.2.0: it keeps the v5 API with PSA crypto, while
  upstream moved to a new v6 API. The old `v5.2.0-dm.*` tags stay. If you declare the
  library in your own YAML, set `ref: v2026.10.12`: this component release needs it
  (`pending_command_count()`), and older refs no longer build.

## 2026.10.12.1 — Find Car recognises 2023+ cars; bounded BLE send queue

Firmware change: rebuild and flash. Same library, `v5.2.0-dm.10`.

### Fixed
- **Find Car didn't find cars from 2023 on.** Newer cars advertise as
  `Tesla <last 6 characters of the VIN>` (for example `Tesla 317000`) instead of
  `S<16 hex>C`. The VIN search only knew the older name, so such a car was never found
  without a `ble_mac_address`. Both names are recognised now, and the search log line names
  both. Only the car's centre beacon is taken (`…C`), not its rear or side beacons, which
  have their own MACs.
- **The BLE send queue is bounded.** While a link stalls, the library keeps resending.
  Those resends used to pile up in RAM until the link dropped: about 40 KB in a stalled
  60 s turn, on an ESP32 with less than 90 KB free. Now:
  - a resend that would wait behind an unsent copy of itself is skipped, because the car gets
    the first copy anyway;
  - the queue holds at most 256 fragments (about 4.6 KB). A message that doesn't fit is
    refused whole (`TX queue full … refused`), never sent in part.

## 2026.10.11.1 — Home Assistant no longer disconnects when the car's charging limit moves

Firmware change: rebuild and flash. Same library, `v5.2.0-dm.10`.

### Fixed
- **Home Assistant (and the log viewer) kept disconnecting** while a car reported a
  maximum charging current that moved by an ampere or two (seen 31 ↔ 32 A while plugged
  in). Every change dropped all API connections, so that Home Assistant would pick up the
  new **Charging Amps** slider limit. Now only a *higher* limit than the slider already
  has reconnects, so in practice once. A lower limit is still enforced: a request above
  the car's current maximum is clamped to it, as before. The **Charging Amps** slider
  therefore keeps the highest limit the car has reported. The per-poll log line is now at
  DEBUG.

## 2026.10.10.2 — Library dm.10: upstream fixes for stuck sessions, buffers and keys

Firmware change: rebuild and flash. Library
[darek-margas/tesla-ble](https://github.com/darek-margas/tesla-ble) `v5.2.0-dm.10`
(roll back: `v5.2.0-dm.9`). No re-pairing.

Upstream (yoziru/tesla-ble) has merged our vehicle actions, the crash fix and the
wake/session fix. The fork takes four of its fixes in return:

### Fixed
- **A session request the car doesn't answer** now fails its command once, not twice with
  two different errors. A double failure could make a car look idle while one of its own
  commands was still running, which ended its BLE turn early. Retries now wait their delay,
  and the time limit escalates. With two cars a stuck car still hands over at the 60 s turn
  limit.
- **A late reply from the car** is checked against the last request to its own part of the
  car (security or infotainment), not whichever was sent last, so it no longer fails the
  authenticity check.
- **Regenerate key** drops the sessions made with the old key, so the next command
  authenticates with the new one.
- **Outgoing messages** are built in buffers sized for the largest message the protocol
  allows. They were 256 bytes, with nothing stopping a longer message from running past
  them. Today's messages are under 200 bytes.

## 2026.10.10.1 — Recover a BLE link stuck opening or closing

Firmware change: rebuild and flash. Same library,
[darek-margas/tesla-ble](https://github.com/darek-margas/tesla-ble) `v5.2.0-dm.9`.

### Fixed
- **A car's link that never finished opening or closing stopped the hand-over.** A
  disconnect requested while ESPHome is still opening a connection only takes effect when
  the open completes; if it never did, the other car never got a turn again. A link that
  hasn't closed 30 s after its turn ended is now logged
  (`BLE link did not close within 30 s (client state …)`) and the BLE stack is restarted.
- **The scanner watchdog no longer starts a scan while a car is connecting or
  disconnecting**, which ESPHome avoids because it can wedge the scanner. If that state
  lasts through two watchdog checks (about two minutes), the BLE stack is restarted
  instead: switched off, and back on once the teardown has finished. Both cars lose their
  link for a moment.
- **Bluetooth switched off on purpose is left alone:** with BLE disabled (for example by a
  "BLE Radio" switch) the watchdog no longer warns that the scanner stopped, tries to start
  it, or restarts the stack.
- **Restarts back off:** after 3 BLE stack restarts within 10 minutes, restarts pause for
  30 minutes (`pausing restarts for 30 min`), so a radio that stays wedged isn't torn down
  every couple of minutes.

## 2026.10.9.8 — BLE scanner watchdog: cars no longer go unheard for hours

Firmware change: rebuild and flash. Same library,
[darek-margas/tesla-ble](https://github.com/darek-margas/tesla-ble) `v5.2.0-dm.9`.

### Fixed
- **The BLE scanner could stay stopped, so no car was heard for hours.** Seen on a
  two-car device: about 45 minutes after boot both cars went "not present" while parked
  in range, took no BLE turns and stopped updating until a reboot. ESPHome restarts its
  continuous scan only while no BLE client is connecting or waiting to connect, so a
  client left waiting keeps it stopped; a search (Find Car) stops and restarts the scan
  too, and then stayed at *Searching*. Now:
  - a watchdog notices a scan stopped for over a minute, logs
    `BLE scanner stopped for over 60 s (state …) - restarting it` with each car's BLE
    client state, releases a client stuck waiting to connect, and restarts the scan;
  - a search always ends: if the scanner never runs in active mode, it ends after the
    2-minute window plus a minute's grace with
    `Search ended: the BLE scanner did not run in active mode`, keeping a known MAC.

## 2026.10.9.7 — Single-car package: VIN only, no BLE MAC from secrets

No firmware change for configs that define the cars themselves (the multi-car layout).
**Read the upgrade note below if you use the single-car package** (the board configs,
`packages/client.yml`). Same library,
[darek-margas/tesla-ble](https://github.com/darek-margas/tesla-ble) `v5.2.0-dm.9`.

### Changed
- **The single-car package no longer reads `!secret ble_mac_address`.** The car is found
  from its VIN and its MAC is saved on the device, as in the multi-car layout. This
  reverses the *Unchanged* note of 2026.10.9.6. A `ble_mac_address` left in your
  `secrets.yaml` does no harm; it is simply not used.
- **`secrets.yaml.example` and `tesla-ble.example.yml` have no MAC any more.** Their dummy
  MAC was a trap: a new user who filled in only the VIN built with a fake address, which
  wins over the VIN search, so the car was never found.
- **`ble_mac_address:` set on the car in your own YAML keeps working** and still wins over
  a MAC found by a search.

### Upgrading from 2026.10.7 or older with the single-car package
Releases up to 2026.10.7 kept the MAC only in your secrets, not on the device. After this
upgrade the first boot searches for the car by its VIN; if the car is away then, it stays
*Not found* and gets no BLE turns until the hourly retry or *Find Car*. Do one of:
- upgrade with the car within Bluetooth range: it is found within 2 minutes and saved;
- upgrade to any release from 2026.10.8 to 2026.10.9.6 first (it saves the MAC from your
  secrets on the device), then to this one;
- or keep pinning it: set `ble_mac_address:` on the car in your own YAML.

From 2026.10.8 on the MAC is already saved on the device, so nothing changes.

### Also
- New issue form for problem reports (version, board, cars, YAML, log, diagnostic
  entities).

### Thanks
The package and example changes and the upgrade note are by @davidcoulson.

## 2026.10.9.6 — Listener component removed, Powershare log at DEBUG

**Breaking for configs that still use the listener:** remove `tesla_ble_listener` from
`external_components` (and any `tesla_ble_listener:` block), then rebuild. Nothing else
changes. Same library, [darek-margas/tesla-ble](https://github.com/darek-margas/tesla-ble)
`v5.2.0-dm.9`.

### Removed
- **`tesla_ble_listener`**, which only logged the BLE MAC of a car with a given VIN for
  copying into YAML. The vehicle component finds the car from its VIN by itself and saves
  the MAC (*Find Car*, *Discovery*, *BLE MAC*), so the listener has had no use since
  2026.10.8. Also removed: `packages/listener.yml`, `listener-test.yml` and its CI build.

### Changed
- The `Powershare: …` log line is logged at DEBUG only (it was INFO during a session):
  the Powershare entities now show the same values.

### Unchanged
- **`ble_mac_address` keeps working**, set in YAML or read from your secrets (the board
  configs read `!secret ble_mac_address`). It still wins over a MAC found by a search; leave
  it out to let the car be found from its VIN. *(The board configs stopped reading the
  secret in 2026.10.9.7; `ble_mac_address:` set on the car in YAML still works.)*

## 2026.10.9.5 — Powershare option in the examples and the single-car package

Rebuild only if you use the single-car package and want Powershare. Same library,
[darek-margas/tesla-ble](https://github.com/darek-margas/tesla-ble) `v5.2.0-dm.9`.

### Changed
- **The single-car package (`packages/client.yml`) has the `powershare` option** as the
  substitution `tesla_powershare` (default `"false"`). Set `tesla_powershare: "true"` in
  your substitutions to build the Powershare entities.
- The examples (`tesla-ble.example.yml`, `multicar-test.yml`) and the README show the
  `powershare` option.
- Release notes now come from this changelog. The notes of 2026.10.9.1 to 2026.10.9.4,
  which repeated those of 2026.10.8.2, are corrected.

## 2026.10.9.4 — Powershare entities only for cars that have it (`powershare: true`)

YAML change for Powershare cars: add `powershare: true` to the car, then rebuild and flash.
Same library, [darek-margas/tesla-ble](https://github.com/darek-margas/tesla-ble) `v5.2.0-dm.9`.

### Changed
- **The Powershare entities are built only for a car with `powershare: true`** (default
  `false`). Few cars can power a load or the home, and a car without the feature just
  leaves the fields out of its replies, so the firmware can't tell on its own. Without the
  option the car gets no Powershare entities; cars built with 2026.10.9.2 or .3 lose them
  on the next build (delete them in Home Assistant if they stay).
- Single Powershare entities can still be left out with `exclude_entities`, e.g.
  `powershare: true` with `exclude_entities: [powershare_stop_reason]`.

## 2026.10.9.3 — Powershare documented

Documentation only, no firmware change.

- README: the Powershare entities and what the car reports over BLE: no energy total for
  the session, and the battery limit is read-only (the protocol has no command to set it;
  set it in the Tesla app). For the energy dashboard, a
  [Riemann sum integral](https://www.home-assistant.io/integrations/integration/) helper on
  *Powershare Power* gives kWh.

## 2026.10.9.2 — Powershare entities

Firmware change: rebuild and flash. Same library,
[darek-margas/tesla-ble](https://github.com/darek-margas/tesla-ble) `v5.2.0-dm.9`.

### Added
- **Powershare**, the car powering a load or the home (vehicle-to-load / vehicle-to-home).
  The car reports it in its charge state over BLE, as the Tesla app shows it; verified on
  a car powering a load (Active, Load, 1.2 kW, 17 h left, stopping at 25 %):
  - **Powershare** (running), **Powershare Status** (*Inactive / Initializing /
    Handshaking / Active / Reconnecting / Stopped*), **Powershare Type** (*None / Load /
    Home*), **Powershare Stop Reason** (*None / SOC Too Low / Retry / Fault / User /
    Reconnecting / Authentication*);
  - **Powershare Power** (kW going out now), **Powershare Time Left** (hours until it
    stops), **Powershare Battery Limit** (the battery % at which it stops).

### Changed
- **Energy Added is not updated while powersharing.** It is the car's charging counter
  (`total_increasing`), and a value going down would be read as a meter reset in the
  statistics. While powersharing, *Charger Power* reads 0.

## 2026.10.9.1 — Powershare fields in the log

Firmware change: rebuild and flash. Same library,
[darek-margas/tesla-ble](https://github.com/darek-margas/tesla-ble) `v5.2.0-dm.9`.

### Added
- Each charge poll logs the car's Powershare fields with *Charger Power* and *Energy
  Added*: `Powershare: allowed=… type=… status=… load_kw=… energy_left_hr=… soc_limit=… |
  charger_power=… energy_added=…`, at INFO while Powershare is active (or charger power is
  negative), DEBUG otherwise. It showed that these fields arrive over BLE.
