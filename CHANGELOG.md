# Changelog

Release notes of the multi-car firmware. The release workflow publishes the section of
the version in `VERSION`, and refreshes the notes of any other release listed here.
Earlier releases (up to 2026.10.8.2) have their notes on the
[releases page](https://github.com/darek-margas/esphome-tesla-ble-multi/releases).

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
