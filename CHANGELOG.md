# Changelog

Release notes of the multi-car firmware. The release workflow publishes the section of
the version in `VERSION`, and refreshes the notes of any other release listed here.
Earlier releases (up to 2026.10.8.2) have their notes on the
[releases page](https://github.com/darek-margas/esphome-tesla-ble-multi/releases).

## 2026.10.9.8 — Find Car can no longer leave the BLE scanner stopped

Firmware change: rebuild and flash. Same library,
[darek-margas/tesla-ble](https://github.com/darek-margas/tesla-ble) `v5.2.0-dm.9`.

### Fixed
- **After Find Car, no car was heard any more.** A search switches the shared BLE scanner
  to active and back by stopping it; ESPHome only restarts the scan while no BLE client is
  connecting or waiting to connect, and in one case it stayed stopped. With no scanning,
  every car went "not present", took no BLE turns and stopped updating, and Discovery
  stayed at *Searching*. Now:
  - a watchdog restarts a scan that has been stopped for over a minute, and logs
    `BLE scanner stopped for over 60 s (state …) - restarting it`;
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
