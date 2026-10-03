# ESPHome Tesla BLE - multi-car

Control more than one Tesla from one ESP32 over BLE.

This is a multi-car fork of [yoziru/esphome-tesla-ble](https://github.com/yoziru/esphome-tesla-ble). The main change is that each car gets its own BLE client, key, sessions and Home Assistant sub-device instead of needing one ESP32 per car.

It currently runs on ESPHome 2026.9.x with the Tesla BLE library v5.2.0.

## What works

- Multiple cars from one ESP32
- Separate BLE connection per car
- Separate private key and session storage per VIN
- Home Assistant sub-device per car
- Pair / regenerate key per car
- Lock / unlock
- Frunk / trunk / windows
- Charge port
- Charging controls and limits
- Climate
- Honk / flash
- Sentry mode
- Vehicle, charging, climate, drive, closure and TPMS sensors
- BLE radio on/off and normal ESPHome restart controls can be added to the parent device

The original single-car package layout still works. Multi-car configs should define the vehicles directly.

## Example: two cars

Keep VINs and BLE MACs in ESPHome secrets.

```yaml
substitutions:
  devicename: tesla-ble
  friendly_name: "Tesla BLE"

  charging_amps_max: "32"

  # Two simultaneous Tesla connections produce much more BLE traffic than one.
  # These are deliberately slower than the old single-car defaults.
  vcsec_poll_interval: "60"
  infotainment_poll_interval_awake: "180"
  infotainment_poll_interval_active: "60"
  infotainment_sleep_timeout: "660"

esphome:
  name: ${devicename}
  friendly_name: ${friendly_name}

  devices:
    - id: car_one_device
      name: "Car One"

    - id: car_two_device
      name: "Car Two"

  project:
    name: "Tesla.BLE"
    version: "multicar"

tesla_ble_vehicle:
  - id: car_one
    name: "Car One"
    device_id: car_one_device

    vin: !secret tesla_vin_car_one
    ble_mac_address: !secret ble_mac_address_car_one

    role: DRIVER
    charging_amps_max: ${charging_amps_max}
    vcsec_poll_interval: ${vcsec_poll_interval}
    infotainment_poll_interval_awake: ${infotainment_poll_interval_awake}
    infotainment_poll_interval_active: ${infotainment_poll_interval_active}
    infotainment_sleep_timeout: ${infotainment_sleep_timeout}

  - id: car_two
    name: "Car Two"
    device_id: car_two_device

    vin: !secret tesla_vin_car_two
    ble_mac_address: !secret ble_mac_address_car_two

    role: DRIVER
    charging_amps_max: ${charging_amps_max}
    vcsec_poll_interval: ${vcsec_poll_interval}
    infotainment_poll_interval_awake: ${infotainment_poll_interval_awake}
    infotainment_poll_interval_active: ${infotainment_poll_interval_active}
    infotainment_sleep_timeout: ${infotainment_sleep_timeout}
```

Secrets:

```yaml
tesla_vin_car_one: "5YJ30123456789ABC"
ble_mac_address_car_one: "A0:B1:C2:D3:E4:F5"

tesla_vin_car_two: "5YJ30123456789ABD"
ble_mac_address_car_two: "A0:B1:C2:D3:E4:F6"
```

Do not put real VINs or MACs into a public repo.

## External component

For a normal ESPHome config:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/darek-margas/esphome-tesla-ble-multi.git
      ref: multicar-v2
      path: components
    components:
      - tesla_ble_vehicle
      - tesla_ble_listener
    refresh: 60s
```

ESP32 / ESP-IDF example:

```yaml
esp32:
  board: esp32dev
  variant: esp32

  framework:
    type: esp-idf

    components:
      - name: tesla-ble
        source: https://github.com/yoziru/tesla-ble.git
        ref: v5.2.0
```

The component currently works around one pairing bug in upstream TeslaBLE: software keys are enrolled as `CLOUD_KEY`. The NFC card is the approving key, not the key being added.

## BLE tracker

```yaml
esp32_ble_tracker:
  scan_parameters:
    interval: 211ms
    window: 120ms
    active: true
    continuous: true
```

Each `tesla_ble_vehicle` instance creates its own internal ESPHome BLE client.

You should see separate clients in the log, for example:

```text
[0] [AA:BB:CC:DD:EE:01]
[1] [AA:BB:CC:DD:EE:02]
```

## Parent device controls

These are not car controls. They belong to the ESPHome node itself and are useful when testing BLE.

```yaml
button:
  - platform: restart
    name: "Restart"
    id: tesla_ble_restart
    entity_category: diagnostic

switch:
  - platform: template
    name: "BLE Radio"
    id: tesla_ble_radio
    icon: mdi:bluetooth
    optimistic: true
    restore_mode: RESTORE_DEFAULT_ON
    entity_category: diagnostic

    turn_on_action:
      - ble.enable:

    turn_off_action:
      - ble.disable:
```

Turning BLE off disconnects all cars. Turning it back on makes both internal clients reconnect.

## Pairing

Pair each car separately.

1. Make sure the ESP32 is connected to the car over BLE.
2. Press that car's **Pair BLE Key** button once.
3. Put an NFC key card on the car's card reader.
4. The approval request should then appear on the car screen.
5. Confirm it.

One slightly confusing Tesla behaviour: the request may not appear until the NFC card is actually on the reader. Pressing Pair repeatedly does not help.

The component therefore treats Pair as single-shot for 180 seconds. Extra presses during that period are ignored and the log says:

```text
Pairing already requested - present NFC card on reader
```

During the first 35 seconds after Pair, background polling for that car is paused to give the whitelist request a quiet BLE link.

After pairing, test with something obvious such as **Flash Lights** or **Honk Horn**.

### Regenerate key

**Regenerate Key** creates a new private key for that car only.

Keys and Tesla session data are stored in a VIN-specific NVS namespace. Regenerating one car's key does not touch another car.

There is deliberately no migration from the old global `storage/private_key` key used by earlier versions.

If you regenerate a key, that car needs to be paired again.

## Finding the BLE MAC

Tesla VCSEC advertises continuously. The advertisement name looks roughly like:

```text
SxxxxxxxxxxxxxxxxC
```

### Android

Use a BLE scanner such as nRF Connect and find the Tesla advertisement. Android can show the MAC address.

### iPhone

iOS does not expose BLE MAC addresses to scanner apps.

### Listener component

The repo also contains `tesla_ble_listener`. It can be used temporarily if the VIN is known but the BLE MAC is not.

Example:

```yaml
tesla_ble_listener:
  id: tesla_listener
  vin: !secret tesla_vin
```

Watch the ESPHome log for the detected Tesla name and MAC, then remove or disable the listener once the real vehicle instance is configured.

## Home Assistant sub-devices

ESPHome 2026.9 supports logical devices under one physical node.

Define them under `esphome.devices`:

```yaml
esphome:
  name: tesla-ble
  devices:
    - id: car_one_device
      name: "Car One"
    - id: car_two_device
      name: "Car Two"
```

and attach each Tesla instance:

```yaml
tesla_ble_vehicle:
  - id: car_one
    name: "Car One"
    device_id: car_one_device
    # ...

  - id: car_two
    name: "Car Two"
    device_id: car_two_device
    # ...
```

All entities created for that vehicle, including Pair and Regenerate Key, are attached to the corresponding Home Assistant device.

The Restart and BLE Radio controls above remain on the parent ESPHome device.

## Polling

The old single-car defaults were fairly aggressive:

```text
VCSEC             10 s
Infotainment      30 s
Active            10 s
```

With two cars that is unnecessary traffic and can push the ESP32 GATT client hard.

A better starting point for two cars is:

```yaml
vcsec_poll_interval: "60"
infotainment_poll_interval_awake: "180"
infotainment_poll_interval_active: "60"
infotainment_sleep_timeout: "660"
```

Commands are still immediate. These settings only control background polling.

Once everything is stable, shorten them if you really need faster state updates.

## BLE transport notes

Tesla messages are larger than one BLE write, so they are fragmented into
18-byte writes. The multi-car adapter:

- allows only one Tesla GATT fragment to be outstanding across all cars
- waits for `ESP_GATTC_WRITE_CHAR_EVT` before advancing
- treats status `143` (`ESP_GATT_CONGESTED`) as *sent*: the ESP-IDF stack
  accepted the fragment, so it is never resent; only that car's link pauses
  until the congestion clears
- retries genuinely failed fragments with backoff

Every log line carries the car name, for example `[Bluey] Polling VCSEC`. At
`DEBUG` level each message also logs its size and how long it took to leave
the ESP32. A warning such as

```text
[Bluey] TX msg #7 sent in 1415 ms ... slower than the library's 1000 ms resend timer
```

means the library will resend that request before the car has seen it.

### One car connected at a time

On the original ESP32, two simultaneous Tesla connections starve each other:
the first-opened link stops being served and times out (`rsn 0x8`), whichever
car holds it and whatever the connection parameters. Each car alone is
reliable, so the cars take turns on the radio:

- the car whose turn it is connects, polls VCSEC, then (only if the car is
  known to be awake, per the normal polling policy) infotainment
- when the other car has work waiting and this link has gone quiet, the car
  disconnects and the other one connects
- commands for a car that is not connected wait for its turn (up to 2 min);
  expect a few seconds of extra latency while the link is established
- only a car that is actually here gets a turn: a Tesla advertises all the
  time while in range (also asleep), and the scanner hears those adverts
  while the other car is connected. A car not heard for 60 s never takes the
  link from the car that is here; as soon as it is heard again it gets the
  next turn
- each car has a diagnostic `BLE Reachable` binary sensor: on while the car
  is heard (or connected), off after 60 s without an advert
- safety nets: a car that is heard but cannot connect backs off (30 s,
  doubling up to 5 min), and a car that is never heard still gets one try
  every 10 min
- connecting never wakes a car: infotainment waits for the VCSEC sleep state

With a single car configured nothing changes: it keeps its link.

The log shows each hand-over, and presence changes:

```text
[Szarik] Not heard for 60 s - BLE unreachable
[Szarik] BLE reachable
[Szarik] Yielding BLE link to the next car
[Bluey] BLE turn starts
```

With two cars, a `vcsec_poll_interval` of 30-60 s keeps the hand-overs
reasonable; at the default 10 s the cars swap continuously.

### Link parameters

Two cars on one ESP32 share one radio. If the two connections use unrelated
intervals, their radio slots collide and the controller keeps sacrificing the
first-opened link until it times out (`rsn 0x8` in the log). Both links
therefore use the same connection interval and a long supervision timeout by
default:

```yaml
tesla_ble_vehicle:
  - name: Szarik
    # ...
    connection_interval: 30ms   # 7.5ms - 4s, same for every car
    supervision_timeout: 6s     # 100ms - 32s, > 2 x connection_interval
```

Within 10 s of connecting, and whenever the values change, each car logs what
the link actually uses:

```text
[Szarik] BLE link params: interval 30.00 ms, latency 0, supervision timeout 6000 ms (requested 30.00 ms / 6000 ms)
```

Keep an active scan window short while cars are connected; scanning takes radio
time from both links:

```yaml
esp32_ble_tracker:
  scan_parameters:
    interval: 320ms
    window: 30ms
    active: false
    continuous: true
```

## Roles

```yaml
role: DRIVER
```

or:

```yaml
role: CHARGING_MANAGER
```

The role is stored in the Tesla whitelist when pairing. Changing it requires pairing the key again.

## Troubleshooting

### Car is visible but commands fail with HMAC errors

Typical log:

```text
Missing session info HMAC tag for DOMAIN_VEHICLE_SECURITY
auth response authentication failed
```

If the car is not paired yet, this is expected.

If it should already be paired:

- verify VIN and BLE MAC belong to the same car
- check that the correct per-VIN key was paired
- try BLE Radio off, wait a few seconds, then on
- if the key was regenerated, pair again

### Pair button appears to do nothing

Put the NFC card on the reader.

On at least some vehicles the car does not show the approval request until the physical card is present.

Do not keep pressing Pair. The component ignores duplicate presses for 180 seconds.

### One car works and the other does not

Check the log for both internal clients and both MAC addresses.

If only one client reaches service discovery, this is a BLE connection problem, not a key problem.

### Commands are slow

Check polling rates first.

Two cars doing VCSEC plus a full infotainment poll every 10 seconds produce a lot of traffic for one ESP32.

### BLE gets into a strange state

Use the parent **BLE Radio** switch:

```text
OFF
wait 3-5 seconds
ON
```

If that does not recover it, use the parent **Restart** button.

## Current status

This branch is working with two Teslas on one classic ESP32, including independent pairing and commands for both cars.

There are still occasional ESP-IDF GATT congestion events under heavier polling. The global write serialization and slower polling make this usable, but this is the main area still worth improving.

## Credits

Original project and most of the Tesla integration work:

- [yoziru/esphome-tesla-ble](https://github.com/yoziru/esphome-tesla-ble)
- Tesla BLE protocol/library work used by that project

This fork mainly adds the multi-car plumbing, per-car storage, ESPHome sub-devices and the BLE transport changes needed to run more than one vehicle from the same ESP32.
