# ESPHome Tesla BLE - multi-car

<a href="#a-car-in-home-assistant"><img src="docs/preview.png" align="right" width="320" alt="A car in Home Assistant: controls, sensors and diagnostics - click for full size"></a>

Control more than one Tesla from one ESP32 over BLE.

This is a multi-car fork of [yoziru/esphome-tesla-ble](https://github.com/yoziru/esphome-tesla-ble). Each car gets its own BLE client, key, sessions and Home Assistant sub-device, so one ESP32 serves several cars instead of needing one ESP32 per car.

It runs on ESPHome 2026.9.x with the Tesla BLE library from [our fork](#tesla-ble-library) (`v5.2.0-dm.4`) and is tested with two cars on a classic ESP32 (Shelly Plus 1).

## What works

- Multiple cars from one ESP32, one BLE link at a time (see [How it works](#how-it-works))
- Separate private key and session storage per VIN
- Home Assistant sub-device per car
- Pair / regenerate key per car
- Lock / unlock, frunk / trunk / windows, charge port
- Charging controls and limits, scheduled charging start and scheduled departure (with preconditioning and off-peak charging)
- Climate (preset and Bioweapon mode read back), cabin overheat protection with its temperature
- Low power mode, keep accessory power, guest mode
- Media player: play / pause, volume and track skip, with now playing (title, artist, source)
- Honk / flash, sentry mode
- Vehicle, charging, climate, drive, closure and TPMS sensors
- `Present` binary sensor per car, based on the car's BLE adverts
- Commands for a sleeping car wake it first; background polling never wakes a car
- Commands for a car that is not connected right now are queued and sent on its turn

The original single-car package layout still works. Multi-car configs define the vehicles directly.

## Example: two cars

Keep VINs and BLE MACs in ESPHome secrets.

```yaml
substitutions:
  devicename: tesla-ble
  friendly_name: "Tesla BLE"

  charging_amps_max: "32"

  # Background polling. With several cars, each car's VCSEC poll is what asks
  # for its BLE turn, so 60 s lets each car hold the link for about a minute.
  # Commands are never delayed by these values.
  vcsec_poll_interval: "60"
  infotainment_poll_interval_awake: "120"
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

esp32_ble_tracker:
  scan_parameters:
    interval: 320ms
    window: 30ms
    active: false
    continuous: true

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

    wake_on_boot: true            # per car, default true: wake once after boot to fill sensors

    # Optional, shown with their defaults:
    # presence_timeout: 5min      # Present turns to away after this long without the car
    # connection_interval: 15ms   # BLE link interval
    # supervision_timeout: 6s     # BLE link timeout

  - id: car_two
    name: "Car Two"
    device_id: car_two_device

    vin: !secret tesla_vin_car_two
    ble_mac_address: !secret ble_mac_address_car_two
    wake_on_boot: false           # this car is not woken after an ESP32 reboot

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

## Configuration reference

Per car, under `tesla_ble_vehicle:`:

| Option | Default | Meaning |
|---|---|---|
| `name` | required | Car name, used for entity names and as the `[Name]` prefix in the log |
| `vin` | required | Vehicle VIN |
| `ble_mac_address` | - | Car's BLE MAC address. Optional: without it the car is searched for by the advert name derived from its VIN, and the MAC is saved in NVS (see [Finding the BLE MAC](#finding-the-ble-mac)) |
| `device_id` | - | Home Assistant sub-device for this car's entities |
| `role` | `DRIVER` | `DRIVER` (all controls) or `CHARGING_MANAGER` (charging + basic controls) |
| `charging_amps_max` | `32` | Upper limit of the charging amps control |
| `vcsec_poll_interval` | `10` s | VCSEC status poll. Never wakes the car. With several cars this is also how often the car asks for a BLE turn: use 30-60 s |
| `infotainment_poll_interval_awake` | `30` s | Infotainment data while awake and idle |
| `infotainment_poll_interval_active` | `10` s | Infotainment data while charging, in sentry mode or with climate on |
| `infotainment_sleep_timeout` | `660` s | After this long idle, polls stop asking infotainment so the car can sleep |
| `wake_on_boot` | `true` | Wake the car once after the ESP32 boots so every sensor gets a value. `false`: sensors stay empty until the car wakes on its own or you press *Force data update* |
| `presence_timeout` | `5min` | `Present` turns to away after the car was neither connected nor heard for this long (1 min - 1 h). Keep it well above a BLE turn, so it does not flicker while the cars take turns |
| `connection_interval` | `15ms` | BLE connection interval (7.5 ms - 4 s). Shorter = faster messages and service discovery. Keep it the same for every car |
| `supervision_timeout` | `6s` | BLE link timeout (100 ms - 32 s, must be more than twice the interval) |

The defaults of the last two suit almost every setup; you normally leave them out.

### Waking a car after boot (`wake_on_boot`)

After the ESP32 boots it knows nothing about the cars. VCSEC (locked, asleep, doors, charge port) is read without waking anything, but charge, climate, tyres and the rest come from the infotainment system, which only answers while the car is awake. With `wake_on_boot: true` (the default) the first infotainment poll after boot wakes the car once, so every sensor has a value within a minute or two. After that the normal policy applies and the car is never woken by polling again.

The option is per car. Set `wake_on_boot: false` on a car you would rather not disturb - for example one that is rarely used, parked far from the ESP32, or where every wake costs battery you care about. Its infotainment sensors then stay empty after a reboot until the car wakes on its own (you unlock it, it starts charging, you use the app) or you press *Force data update*. VCSEC-based entities (Asleep, Doors lock, door/trunk/frunk state, charge port) fill in right away either way.

## How it works

### One car connected at a time

**Why.** The first multi-car version kept a permanent BLE connection to every car. On the classic ESP32 that does not work reliably: with two Tesla links up, the controller stops serving the first-opened link (HCI handle 0) and it times out (`rsn 0x8` in the log) every 10-20 seconds, while the second link runs fine. It is not about signal strength or the car: it happened to whichever car connected first, with matched connection intervals (15-80 ms) and long supervision timeouts, and each car on its own was completely stable. Every drop also reset that car's Tesla sessions and polls, so one car was effectively starved.

**What it does instead.** The cars take turns on the radio. Only the car whose turn it is may connect (the gate is in the BLE client's advert handling, so the other car never even starts a connection):

- the car holding the link keeps it as long as nobody else needs it, so its commands go out immediately
- when another car has work waiting (a queued command or a due VCSEC poll) and the link has been quiet for 1 s, the owner disconnects and the next car connects (round-robin)
- a turn lasts at least 2 s (so polls get started) and at most 60 s while another car waits; a car that does not connect within 30 s loses its turn
- with a single car configured nobody else waits, so it keeps its link - the same as the original single-car behaviour

A hand-over takes about 1 s from one car's last traffic to the next car's first answer, because:

- each car's GATT service table is cached in NVS (written once per car, survives reboots), so a reconnect skips service discovery. A stale cache entry (for example after a Tesla firmware update) shows up as a missing characteristic: the entry is cleared and the car reconnects
- on a planned hand-over the Tesla VCSEC and infotainment sessions are kept in memory, so the next turn does not repeat the session handshake. An unexpected link loss still resets them

### Presence

A Tesla advertises over BLE all the time while it is in range, also while asleep. The scanner keeps listening while the other car is connected, so each car's adverts are recorded even when it does not hold the link.

- only a car heard in the last 60 s (or connected) can ask for a turn, so a car that is away never takes the link from the car that is home. As soon as it is heard again it gets the next turn
- `Present` (presence binary sensor) is on while the car is heard or connected, and turns to away after `presence_timeout` (default 5 min) without either. It is published only when it changes, and after a reboot it waits a minute for the first advert before it reports away
- safety nets: a car that is heard but cannot connect backs off (30 s, doubling up to 5 min); a car that is never heard still gets one try every 10 min

### Commands

- A command for the car holding the link goes out at once.
- A command for a car that is not connected is queued, the car asks for its turn, and the command is sent as soon as it connects (typically 1.5-2 s in total). Queued commands expire after 2 min if the car does not become reachable.
- An infotainment command (climate, charging, horn, cabin overheat, ...) for a car that is asleep first wakes it, waits 8 s for infotainment to come up, then sends the command. Without the wait the library's first infotainment session request is often lost and its watchdog resets the link 30 s later.
- A command that fails with a temporary error (connection lost, session reset) is sent once more. A horn or flash may in rare cases trigger twice; lock, climate and charging commands are idempotent.

### Sleep and polling

- VCSEC (lock state, sleep state, presence) never wakes a car.
- Connecting never wakes a car: after connecting, only VCSEC is polled, and the infotainment decision waits for its answer, so a sleeping car is left asleep.
- Infotainment polls follow the polling policy: active interval while charging, in sentry mode or with climate on, awake interval otherwise, and after `infotainment_sleep_timeout` idle they no longer ask infotainment, so the car can fall asleep.
- When VCSEC sees a car wake up (for example climate turned on from the Tesla app), its state is read once right away, at most once per 15 minutes, so the change shows without waiting for the idle interval.
- Exception: the first infotainment poll after boot wakes each car once (`wake_on_boot`).
- *Force data update* always fetches fresh data, waking the car if needed. If the car is not connected it is queued like a command.

### BLE transport

Tesla messages are larger than one BLE write, so they are fragmented into 18-byte writes (5 ms apart). The adapter:

- waits for `ESP_GATTC_WRITE_CHAR_EVT` before sending the next fragment
- treats status `143` (`ESP_GATT_CONGESTED`) as *sent*. ESP-IDF returns it for a fragment it accepted while the link is congested; resending it duplicated bytes inside the Tesla frame and corrupted the message. The link now just pauses until the congestion clears
- retries genuinely failed fragments with backoff

### Reading the log

Every line carries the car name. A typical cycle:

```text
[Car One] Yielding BLE link to the next car
[Car Two] BLE turn starts
[Car Two] Connection established - polling VCSEC
[Car Two] Sending queued 'Cabin Overheat On'
[Car Two] [Cabin Overheat On] Command completed successfully in 477 ms
```

Other useful lines:

```text
[Car One] Not heard for 300 s - not present
[Car One] Present (BLE heard)
[Car Two] 'Honk Horn': car asleep - waking it first
[Car Two] Awake - sending 'Honk Horn' in 8 s
[Car Two] First poll after boot - waking the car once to fill sensors
[Car One] Car woke up - reading its state once
```

At `DEBUG` level each Tesla message also logs its size and how long it took to leave the ESP32, and each connection logs the BLE parameters the link actually uses:

```text
[Car One] BLE link params: interval 15.00 ms, latency 0, supervision timeout 6000 ms (requested 15.00 ms / 6000 ms)
```

## External component

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/darek-margas/esphome-tesla-ble-multi.git
      ref: multicar-v2          # or a release tag such as v2026.10.0
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
        source: https://github.com/darek-margas/tesla-ble.git
        ref: v5.2.0-dm.4  # our fork: v5.2.0 + low power, keep accessory power, guest mode, overheat temperature, scheduled departure, media, wake fix (roll back: v5.2.0-dm.3)
```

### Tesla BLE library

The library is built from [darek-margas/tesla-ble](https://github.com/darek-margas/tesla-ble), a fork of [yoziru/tesla-ble](https://github.com/yoziru/tesla-ble):

- branch `main` mirrors upstream (synced with GitHub's *Sync fork*);
- branch `multicar` (default) = `main` + the commands this component needs that upstream does not have yet: guest mode, cabin overheat protection temperature, scheduled departure, media volume and the media state read (low power mode and keep accessory power come from upstream `main`);
- tags `v5.2.0-dm.N` are created by the fork's release workflow after its tests pass.

| `ref` | Contents |
|---|---|
| `v5.2.0-dm.4` | current: everything below + one session request per wake (first command after a wake no longer takes ~8 s) and identical resends (a lost reply no longer runs a toggle twice) |
| `v5.2.0-dm.3` | everything below + media controls and media state (artist, title) |
| `v5.2.0-dm.2` | everything below + scheduled departure |
| `v5.2.0-dm.1` | guest mode, overheat temperature, low power, keep accessory power |
| `v5.2.0` | upstream release (the new controls fail to build with it) |

**If your own YAML declares the `tesla-ble` component** (as in the example above), set `source` to the fork and `ref: v5.2.0-dm.4` (dm.3 still builds, without the wake fix). With a ref older than dm.3 the build stops with an error at `set_media_state_callback` - that is the check telling you the library is too old. Upstream changes are synced into the fork deliberately, so a new upstream release cannot break this build unannounced.

The component enables the ESP-IDF GATT client cache (`CONFIG_BT_GATTC_CACHE_NVS_FLASH`) itself; nothing to add.

The component also works around one pairing bug in upstream TeslaBLE: software keys are enrolled as `CLOUD_KEY`. The NFC card is the approving key, not the key being added.

## BLE tracker

```yaml
esp32_ble_tracker:
  scan_parameters:
    interval: 320ms
    window: 30ms
    active: false
    continuous: true
```

- `continuous: true` is required: scanning is how a car is found to connect, and how presence is detected
- keep the window short: scanning takes radio time from the connected car. A long active scan window (for example 120 ms of 211 ms) noticeably hurt the links
- `active: false` is enough; Teslas are found by MAC address. Only the listener component (finding a MAC from a VIN) may need active scanning

Each `tesla_ble_vehicle` instance creates its own internal ESPHome BLE client, so the log shows separate clients:

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

Turning BLE off disconnects all cars. Turning it back on lets them reconnect in turn.

<img src="docs/ha-parent-device.png" width="700" alt="ESP32 node with BLE Radio, Restart and the connected cars">

## Pairing

Pair each car separately.

1. Press that car's **Pair BLE Key** button once. If the car is not connected right now, the request waits for its turn (`'Pair' waits for this car's BLE turn`).
2. Put an NFC key card on the car's card reader.
3. The approval request should then appear on the car screen.
4. Confirm it.

One slightly confusing Tesla behaviour: the request may not appear until the NFC card is actually on the reader. Pressing Pair repeatedly does not help.

The component therefore treats Pair as single-shot for 180 seconds. Extra presses during that period are ignored and the log says:

```text
Pairing already requested - present NFC card on reader
```

During the first 35 seconds after Pair, background polling for that car is paused and the car keeps its BLE turn, to give the whitelist request a quiet link.

The request on the car screen looks like this:

<img src="docs/vehicle-pair-request.png" width="500" alt="Pairing request on the car screen">

After pairing, test with something obvious such as **Flash Lights** or **Honk Horn**.

### Regenerate key

**Regenerate Key** creates a new private key for that car only.

Keys and Tesla session data are stored in a VIN-specific NVS namespace. Regenerating one car's key does not touch another car.

There is deliberately no migration from the old global `storage/private_key` key used by earlier versions.

If you regenerate a key, that car needs to be paired again.

## Finding the BLE MAC

You normally do not need to: leave `ble_mac_address` out and the component searches for the car by its advert name, which is derived from the VIN.

- **Where the MAC comes from**, in this order: `ble_mac_address` in YAML (always wins), the MAC saved in NVS by an earlier search, otherwise none.
- **A search** runs once at boot when there is no MAC, and whenever you press the car's **Find Car** button. Teslas send their advert name in the scan response, so for the search window (2 minutes, counted from when the scanner is running) the scanner is switched to active, then back to passive.
- **Diagnostic entities per car:** *Discovery* (`Searching` / `Found` / `Not found` / `Configured` for a YAML MAC) and *BLE MAC*. The log shows `[Car One] Found car: BLE MAC AA:BB:CC:DD:EE:FF (advert S...C)`, and the MAC is saved in NVS. A search that does not see the car ends as `Not found` (check the VIN, or press Find Car with the car nearby); a previously saved MAC is kept.
- **A car without a MAC** takes no BLE turns, and its commands fail right away (`No BLE MAC - '...' not sent (press Find Car)`) instead of waiting in the queue.
- **With `ble_mac_address` set** nothing changes, except that the MAC is also saved in NVS (only when it differs), so you can later delete it from YAML and carry on without a search. If this VIN's advert is seen from a different address than the configured one, the log warns once (likely a typo or swapped cars); the YAML MAC is never overridden.


To look it up anyway:

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

All entities created for that vehicle, including Pair and Regenerate Key, are attached to the corresponding Home Assistant device. Their names then leave out the car name ("Charge Port Latch", not "Car One Charge Port Latch"): Home Assistant already shows the device name in front, so it would appear twice. Without `device_id` the car name stays in each entity name.

<img src="docs/ha-device-overview.png" width="700" alt="ESP32 node with one device per car">

Per car, besides the vehicle entities:

| Entity | Type | Notes |
|---|---|---|
| Present | binary sensor (presence) | Home while connected or heard over BLE in the last `presence_timeout` (5 min), otherwise away |
| BLE RSSI | sensor, diagnostic | Signal of the connected link (only while this car holds it). Disabled by default |
| BLE Advert RSSI | sensor, diagnostic | Signal of the car's adverts, every 10 s, also while not connected; unknown when not heard. Disabled by default |
| Last Command | text sensor, diagnostic | Result of the last command. Disabled by default |

The Restart and BLE Radio controls above remain on the parent ESPHome device.

### A car in Home Assistant

Controls: locks, covers, charging, climate (preset and Bioweapon fan mode are read back from the car), scheduled charging start time, sentry mode and buttons:

- **Cabin Overheat Protection**: *Off / Fan Only / On 30 °C / On 35 °C / On 40 °C* - the activation temperature only applies to *On* (A/C), as in the Tesla app.
- **Scheduled Charging** (switch) + **Scheduled Charging Start** (time): start charging at a time.
- **Scheduled Departure** (switch), **Scheduled Departure Time** (time), **Departure Preconditioning** and **Departure Off-Peak Charging** (*Off / All Week / Weekdays*), **Off-Peak End Time** (time): depart by a time. Setting a time also turns its schedule on; start-at and depart-by are alternatives in the car. All read back from the car.
- **Low Power Mode**, **Keep Accessory Power**, **Guest Mode** (guest mode needs car software 2024.14+): the car does not report these modes over BLE, so the switches show the last state that was set successfully (Home Assistant shows on and off buttons).
- **Media** (media player): play / pause and volume of the car's media, read back from the car. ESPHome media players carry no titles and Home Assistant does not offer next / previous for them, so these are separate entities: **Media Title**, **Media Artist** and **Media Source** (empty while nothing plays) and the **Media Next Track** / **Media Previous Track** buttons. The media state is read with each infotainment poll while the car is awake, and shortly after every media command. While the car sleeps the player shows *Off* (ESPHome cannot mark one entity unavailable), and media commands are not sent: they never wake the car. To get one player with the titles and the skip arrows, combine them in Home Assistant with a [universal media player](https://www.home-assistant.io/integrations/universal/) (entity ids for a car named *Bluey*; check yours):

  ```yaml
  # Home Assistant configuration.yaml
  media_player:
    - platform: universal
      name: Bluey Player
      unique_id: bluey_player
      children:
        - media_player.bluey_media
      commands:
        media_next_track:
          action: button.press
          target:
            entity_id: button.bluey_media_next_track
        media_previous_track:
          action: button.press
          target:
            entity_id: button.bluey_media_previous_track
      attributes:
        media_title: sensor.bluey_media_title
        media_artist: sensor.bluey_media_artist
        source: sensor.bluey_media_source
  ```

<img src="docs/ha-car-controls.png" width="700" alt="Car controls">

Sensors and diagnostics. A sensor that stays *Unknown* means that car does not send that value over BLE (for example Estimated Range), or it does not apply right now (Time to Full when not charging):

<table>
<tr>
<td valign="top"><img src="docs/ha-car-sensors.png" width="253" alt="Car sensors"></td>
<td valign="top"><img src="docs/ha-car-diagnostic.png" width="252" alt="Car diagnostics"><br><br>
Diagnostics: link signal (BLE RSSI, and the car's advert RSSI while not connected), <em>Force data update</em> to read everything now, pairing and key regeneration, and the result of the last command.</td>
</tr>
</table>

## Polling

Commands are always sent immediately (or on the car's next turn). Polling settings only control background data.

Recommended for two cars:

```yaml
vcsec_poll_interval: "60"               # how often each car asks for a turn
infotainment_poll_interval_awake: "120" # awake, idle
infotainment_poll_interval_active: "60" # charging / sentry / climate on
infotainment_sleep_timeout: "660"       # leave as is: lets the car sleep
```

Why these values:

- with several cars, a due VCSEC poll is what makes the waiting car ask for the link. At 10 s the cars swap constantly and most of the time goes into hand-overs; at 60 s each car holds the link for about a minute and its commands go out without any hand-over
- infotainment polls only run during a car's turn, so intervals shorter than about two turns do not give fresher data
- keep the sleep timeout: it is what lets an idle car fall asleep

With a single car the original defaults (10 / 30 / 10 / 660) are fine.

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

### A car never gets a turn

Check `Present` for that car (or `[Name] Present (BLE heard)` in the log). A car that is not heard is never given the link. If the car is in range but not heard, check `esp32_ble_tracker` has `continuous: true` and the MAC address is right.

A car that is heard but does not connect logs `Not reachable during its BLE turn - next try in N s`.

### Commands are slow

- A command for the car that holds the link should finish in well under a second; for the other car, about 1.5-2 s including the hand-over.
- If hand-overs are slow, check that `connection_interval` is not set high (the default 15 ms is right) and that `vcsec_poll_interval` is not so low that the cars swap continuously.
- Commands for a sleeping car take about 10 s longer: wake, 8 s for infotainment, then the command.

### Sensors are empty after a reboot

With `wake_on_boot: false` a sleeping car's infotainment sensors stay empty until it wakes. Press *Force data update*, or leave `wake_on_boot` at its default.

### BLE gets into a strange state

Use the parent **BLE Radio** switch:

```text
OFF
wait 3-5 seconds
ON
```

If that does not recover it, use the parent **Restart** button.

## Current status

Working with two Teslas on one classic ESP32: no link drops, about 1 s hand-overs, commands to either car in about 2 s, independent pairing, presence detection and sleep-friendly polling.

## Credits

Original project and most of the Tesla integration work:

- [yoziru/esphome-tesla-ble](https://github.com/yoziru/esphome-tesla-ble)
- Tesla BLE protocol/library work used by that project

This fork mainly adds the multi-car plumbing, per-car storage, ESPHome sub-devices, the one-link-at-a-time scheduler, presence detection and the BLE transport changes needed to run more than one vehicle from the same ESP32.
