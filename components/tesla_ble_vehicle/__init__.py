import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble, esp32_ble_client, esp32_ble_tracker, binary_sensor, button, switch, number, sensor, text_sensor, lock, cover, climate, select, datetime, media_player
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.components.esp32_ble import BTLoggers
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_DEVICE_ID,
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_FORCE_UPDATE,
    CONF_ICON,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_MODE,
    CONF_NAME,
    CONF_RESTORE_MODE,
    CONF_STATE_CLASS,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome import automation


CODEOWNERS = ["@yoziru"]
DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["esp32_ble_client", "binary_sensor", "button", "switch", "number", "sensor", "text_sensor", "lock", "cover", "climate", "select", "datetime", "media_player"]
MULTI_CONF = True

tesla_ble_vehicle_ns = cg.esphome_ns.namespace("tesla_ble_vehicle")
TeslaBLEClient = tesla_ble_vehicle_ns.class_(
    "TeslaBLEClient", esp32_ble_client.BLEClientBase
)
TeslaBLEVehicle = tesla_ble_vehicle_ns.class_(
    "TeslaBLEVehicle", cg.PollingComponent
)

# Custom button classes - generated via macro in C++, just reference here
# The class name follows pattern: Tesla{Id}Button where Id is PascalCase of id
TeslaWakeButton = tesla_ble_vehicle_ns.class_("TeslaWakeButton", button.Button)
TeslaFindCarButton = tesla_ble_vehicle_ns.class_("TeslaFindCarButton", button.Button)
TeslaPairButton = tesla_ble_vehicle_ns.class_("TeslaPairButton", button.Button)
TeslaRegenerateKeyButton = tesla_ble_vehicle_ns.class_("TeslaRegenerateKeyButton", button.Button)
TeslaForceUpdateButton = tesla_ble_vehicle_ns.class_("TeslaForceUpdateButton", button.Button)
TeslaFlashLightsButton = tesla_ble_vehicle_ns.class_("TeslaFlashLightsButton", button.Button)
TeslaHonkHornButton = tesla_ble_vehicle_ns.class_("TeslaHonkHornButton", button.Button)
TeslaUnlatchDriverDoorButton = tesla_ble_vehicle_ns.class_("TeslaUnlatchDriverDoorButton", button.Button)
TeslaReleaseChargeCableButton = tesla_ble_vehicle_ns.class_("TeslaReleaseChargeCableButton", button.Button)
TeslaMediaNextTrackButton = tesla_ble_vehicle_ns.class_("TeslaMediaNextTrackButton", button.Button)
TeslaMediaPreviousTrackButton = tesla_ble_vehicle_ns.class_("TeslaMediaPreviousTrackButton", button.Button)

# Custom switch classes - generated via macro in C++, just reference here
TeslaChargingSwitch = tesla_ble_vehicle_ns.class_("TeslaChargingSwitch", switch.Switch)
TeslaSteeringWheelHeatSwitch = tesla_ble_vehicle_ns.class_("TeslaSteeringWheelHeatSwitch", switch.Switch)
TeslaSentryModeSwitch = tesla_ble_vehicle_ns.class_("TeslaSentryModeSwitch", switch.Switch)
TeslaScheduledChargingSwitch = tesla_ble_vehicle_ns.class_("TeslaScheduledChargingSwitch", switch.Switch)
TeslaScheduledChargingTime = tesla_ble_vehicle_ns.class_("TeslaScheduledChargingTime", datetime.TimeEntity)
TeslaScheduledDepartureSwitch = tesla_ble_vehicle_ns.class_("TeslaScheduledDepartureSwitch", switch.Switch)
TeslaDepartureTime = tesla_ble_vehicle_ns.class_("TeslaDepartureTime", datetime.TimeEntity)
TeslaOffPeakEndTime = tesla_ble_vehicle_ns.class_("TeslaOffPeakEndTime", datetime.TimeEntity)
TeslaDeparturePreconditioningSelect = tesla_ble_vehicle_ns.class_("TeslaDeparturePreconditioningSelect", select.Select)
TeslaDepartureOffPeakSelect = tesla_ble_vehicle_ns.class_("TeslaDepartureOffPeakSelect", select.Select)

# Custom lock classes
TeslaDoorsLock = tesla_ble_vehicle_ns.class_("TeslaDoorsLock", lock.Lock)
TeslaChargePortLatchLock = tesla_ble_vehicle_ns.class_("TeslaChargePortLatchLock", lock.Lock)

# Custom cover classes
TeslaTrunkCover = tesla_ble_vehicle_ns.class_("TeslaTrunkCover", cover.Cover)
TeslaFrunkCover = tesla_ble_vehicle_ns.class_("TeslaFrunkCover", cover.Cover)
TeslaWindowsCover = tesla_ble_vehicle_ns.class_("TeslaWindowsCover", cover.Cover)
TeslaChargePortDoorCover = tesla_ble_vehicle_ns.class_("TeslaChargePortDoorCover", cover.Cover)

# Custom climate class
TeslaClimate = tesla_ble_vehicle_ns.class_("TeslaClimate", climate.Climate)

# Custom media player class
TeslaMediaPlayer = tesla_ble_vehicle_ns.class_("TeslaMediaPlayer", media_player.MediaPlayer)

# Custom select classes
TeslaCabinOverheatSelect = tesla_ble_vehicle_ns.class_("TeslaCabinOverheatSelect", select.Select)
TeslaLowPowerModeSwitch = tesla_ble_vehicle_ns.class_("TeslaLowPowerModeSwitch", switch.Switch)
TeslaKeepAccessoryPowerSwitch = tesla_ble_vehicle_ns.class_("TeslaKeepAccessoryPowerSwitch", switch.Switch)
TeslaGuestModeSwitch = tesla_ble_vehicle_ns.class_("TeslaGuestModeSwitch", switch.Switch)

# Custom number classes
TeslaChargingAmpsNumber = tesla_ble_vehicle_ns.class_("TeslaChargingAmpsNumber", number.Number)
TeslaChargingLimitNumber = tesla_ble_vehicle_ns.class_("TeslaChargingLimitNumber", number.Number)

# Actions
WakeAction = tesla_ble_vehicle_ns.class_("WakeAction", automation.Action)
PairAction = tesla_ble_vehicle_ns.class_("PairAction", automation.Action)
RegenerateKeyAction = tesla_ble_vehicle_ns.class_("RegenerateKeyAction", automation.Action)
ForceUpdateAction = tesla_ble_vehicle_ns.class_("ForceUpdateAction", automation.Action)
SetChargingAction = tesla_ble_vehicle_ns.class_("SetChargingAction", automation.Action)
SetChargingAmpsAction = tesla_ble_vehicle_ns.class_("SetChargingAmpsAction", automation.Action)
SetChargingLimitAction = tesla_ble_vehicle_ns.class_("SetChargingLimitAction", automation.Action)

# Configuration constants
CONF_VIN = "vin"
CONF_BLE_MAC_ADDRESS = "ble_mac_address"
CONF_INTERNAL_BLE_CLIENT_ID = "internal_ble_client_id"
CONF_CONNECTION_INTERVAL = "connection_interval"
CONF_SUPERVISION_TIMEOUT = "supervision_timeout"

# BLE link-layer units: connection interval in 1.25 ms steps, supervision
# timeout in 10 ms steps (Bluetooth Core spec, Vol 6 Part B 4.5.1).
CONN_INTERVAL_UNIT_US = 1250
SUPERVISION_TIMEOUT_UNIT_US = 10000
CONF_CHARGING_AMPS_MAX = "charging_amps_max"
DEFAULT_CHARGING_AMPS_MAX = 32
CONF_ROLE = "role"

# Polling configuration constants
CONF_VCSEC_POLL_INTERVAL = "vcsec_poll_interval"
CONF_INFOTAINMENT_POLL_INTERVAL_AWAKE = "infotainment_poll_interval_awake" 
CONF_INFOTAINMENT_POLL_INTERVAL_ACTIVE = "infotainment_poll_interval_active"
CONF_INFOTAINMENT_SLEEP_TIMEOUT = "infotainment_sleep_timeout"
CONF_WAKE_ON_BOOT = "wake_on_boot"
CONF_PRESENCE_TIMEOUT = "presence_timeout"
CONF_DISCOVERY_RETRY_INTERVAL = "discovery_retry_interval"

# Tesla key roles
TESLA_ROLES = {
    "DRIVER": "Keys_Role_ROLE_DRIVER",
    "CHARGING_MANAGER": "Keys_Role_ROLE_CHARGING_MANAGER",
}

# =============================================================================
# ENTITY DEFINITIONS - Add new sensors/controls here!
# =============================================================================
# Just add to these lists - no C++ changes needed unless custom logic is required.
# The sensor ID must match the ID used in vehicle_state_manager.cpp update methods.
#
# Each definition is a dict with:
#   - id: unique identifier (must match C++ usage)
#   - name: display name
#   - icon: MDI icon (optional)
#   - device_class: ESPHome device class (optional)
#   - unit: unit of measurement (optional, for sensors/numbers)
#   - accuracy_decimals: number of decimals for display precision (optional, for sensors only)
#   - state_class: e.g. "measurement" (optional, for sensors only)
#   - disabled_by_default: whether disabled by default (optional, default False)
#   - entity_category: entity category (optional, e.g. "diagnostic")
#
# For buttons/switches, also include:
#   - class: the C++ class reference (e.g. TeslaWakeButton)
#   - setter: setter method name if needed for special handling (optional)
#
# For numbers, also include:
#   - min: minimum value
#   - max: maximum value (or "config" to use config value like charging_amps_max)
#   - step: step size

BINARY_SENSORS = [
    # VCSEC sensors
    {"id": "asleep", "name": "Asleep", "icon": "mdi:sleep"},
    {"id": "user_present", "name": "User Present", "icon": "mdi:account-check", "device_class": "occupancy"},
    {"id": "charger", "name": "Charger", "icon": "mdi:power-plug", "device_class": "plug"},
    {"id": "cabin_overheat_active", "name": "Cabin Overheat Active", "icon": "mdi:snowflake-thermometer", "device_class": "running"},
    # Car is here: its BLE adverts were heard in the last 60 s (or it is connected)
    {"id": "present", "name": "Present", "icon": "mdi:car-connected", "device_class": "presence"},

    # Climate
    {"id": "preconditioning", "name": "Preconditioning", "icon": "mdi:car-defrost-front", "device_class": "running"},
    {"id": "front_defroster", "name": "Front Defroster", "icon": "mdi:car-defrost-front", "device_class": "running"},
    {"id": "rear_defroster", "name": "Rear Defroster", "icon": "mdi:car-defrost-rear", "device_class": "running"},
    {"id": "battery_heater", "name": "Battery Heater", "icon": "mdi:heating-coil", "device_class": "running"},
    {"id": "battery_heater_no_power", "name": "Battery Heater No Power", "icon": "mdi:battery-alert", "device_class": "problem"},
    # Seat heaters: Running when heating at any level (level in the text sensors)
    {"id": "seat_heater_front_left", "name": "Seat Heater Front Left", "icon": "mdi:car-seat-heater", "device_class": "running"},
    {"id": "seat_heater_front_right", "name": "Seat Heater Front Right", "icon": "mdi:car-seat-heater", "device_class": "running"},
    {"id": "seat_heater_rear_left", "name": "Seat Heater Rear Left", "icon": "mdi:car-seat-heater", "device_class": "running", "disabled_by_default": True},
    {"id": "seat_heater_rear_center", "name": "Seat Heater Rear Center", "icon": "mdi:car-seat-heater", "device_class": "running", "disabled_by_default": True},
    {"id": "seat_heater_rear_right", "name": "Seat Heater Rear Right", "icon": "mdi:car-seat-heater", "device_class": "running", "disabled_by_default": True},

    # Charging
    {"id": "scheduled_charging_pending", "name": "Scheduled Charging Pending", "icon": "mdi:calendar-clock"},

    # Closures / modes
    {"id": "speed_limit_mode", "name": "Speed Limit Mode", "icon": "mdi:speedometer-slow"},

    # Tyre low-pressure warnings (hard = significantly low, soft = slightly low)
    {"id": "tpms_hard_warning_front_left", "name": "TPMS Warning Front Left", "icon": "mdi:car-tire-alert", "device_class": "problem"},
    {"id": "tpms_hard_warning_front_right", "name": "TPMS Warning Front Right", "icon": "mdi:car-tire-alert", "device_class": "problem"},
    {"id": "tpms_hard_warning_rear_left", "name": "TPMS Warning Rear Left", "icon": "mdi:car-tire-alert", "device_class": "problem"},
    {"id": "tpms_hard_warning_rear_right", "name": "TPMS Warning Rear Right", "icon": "mdi:car-tire-alert", "device_class": "problem"},
    {"id": "tpms_soft_warning_front_left", "name": "TPMS Soft Warning Front Left", "icon": "mdi:car-tire-alert", "device_class": "problem", "disabled_by_default": True},
    {"id": "tpms_soft_warning_front_right", "name": "TPMS Soft Warning Front Right", "icon": "mdi:car-tire-alert", "device_class": "problem", "disabled_by_default": True},
    {"id": "tpms_soft_warning_rear_left", "name": "TPMS Soft Warning Rear Left", "icon": "mdi:car-tire-alert", "device_class": "problem", "disabled_by_default": True},
    {"id": "tpms_soft_warning_rear_right", "name": "TPMS Soft Warning Rear Right", "icon": "mdi:car-tire-alert", "device_class": "problem", "disabled_by_default": True},
    
    # Drive sensors
    {"id": "parking_brake", "name": "Parking Brake", "icon": "mdi:car-brake-parking"},
    
    # Individual closures. No fixed icon: Home Assistant shows open/closed door
    # and window icons and colours them by state. Doors follow VCSEC (current
    # while asleep); windows and sunroof follow the infotainment poll.
    {"id": "door_driver_front", "name": "Door Driver Front", "device_class": "door"},
    {"id": "door_driver_rear", "name": "Door Driver Rear", "device_class": "door"},
    {"id": "door_passenger_front", "name": "Door Passenger Front", "device_class": "door"},
    {"id": "door_passenger_rear", "name": "Door Passenger Rear", "device_class": "door"},
    {"id": "window_driver_front", "name": "Window Driver Front", "device_class": "window"},
    {"id": "window_driver_rear", "name": "Window Driver Rear", "device_class": "window"},
    {"id": "window_passenger_front", "name": "Window Passenger Front", "device_class": "window"},
    {"id": "window_passenger_rear", "name": "Window Passenger Rear", "device_class": "window"},
    {"id": "sunroof", "name": "Sunroof", "icon": "mdi:car-select", "device_class": "window", "disabled_by_default": True},

]

SENSORS = [
    {"id": "ble_rssi", "name": "BLE RSSI", "icon": "mdi:signal", "unit": "dBm", "accuracy_decimals": 0, "entity_category": "diagnostic", "disabled_by_default": True, "device_class": "signal_strength", "state_class": "measurement"},
    # RSSI of the car's advertisements: available while the car is in range,
    # also when it is not connected (unknown once not heard for 60 s)
    {"id": "ble_advert_rssi", "name": "BLE Advert RSSI", "icon": "mdi:bluetooth-audio", "unit": "dBm", "accuracy_decimals": 0, "entity_category": "diagnostic", "disabled_by_default": True, "device_class": "signal_strength", "state_class": "measurement"},
    # Charge state sensors
    {"id": "battery_level", "name": "Battery", "unit": "%", "device_class": "battery", "state_class": "measurement"},
    {"id": "range", "name": "Range", "icon": "mdi:map-marker-distance", "device_class": "distance", "unit": "km", "state_class": "measurement"},
    {"id": "est_battery_range", "name": "Estimated Range", "icon": "mdi:map-marker-path", "device_class": "distance", "unit": "km", "disabled_by_default": True, "state_class": "measurement"},
    {"id": "ideal_battery_range", "name": "Ideal Range", "icon": "mdi:map-marker-check", "device_class": "distance", "unit": "km", "disabled_by_default": True, "state_class": "measurement"},
    {"id": "usable_battery_level", "name": "Usable Battery", "unit": "%", "device_class": "battery", "state_class": "measurement"},
    {"id": "charger_power", "name": "Charger Power", "icon": "mdi:flash", "device_class": "power", "unit": "kW", "state_class": "measurement"},
    {"id": "charger_voltage", "name": "Charger Voltage", "icon": "mdi:lightning-bolt", "device_class": "voltage", "unit": "V", "state_class": "measurement"},
    {"id": "charger_current", "name": "Charger Current", "icon": "mdi:current-ac", "device_class": "current", "unit": "A", "state_class": "measurement"},
    {"id": "evse_max_current", "name": "Charger Max", "icon": "mdi:ev-plug-tesla", "device_class": "current", "unit": "A", "state_class": "measurement"},
    {"id": "charge_current_request", "name": "Requested Current", "icon": "mdi:current-ac", "device_class": "current", "unit": "A", "entity_category": "diagnostic", "disabled_by_default": True, "state_class": "measurement"},
    {"id": "vehicle_max_charge_current", "name": "Car Max Acceptable", "icon": "mdi:car-battery", "device_class": "current", "unit": "A", "state_class": "measurement"},
    # state_class makes Home Assistant treat it as a number, so the 0 decimals
    # apply (without unit/device class/state class it shows the raw "1.0")
    {"id": "charger_phases", "name": "Charger Phases", "icon": "mdi:sine-wave", "accuracy_decimals": 0, "state_class": "measurement"},
    {"id": "charger_power_estimated", "name": "Charger Power Estimated", "icon": "mdi:flash", "device_class": "power", "unit": "kW", "accuracy_decimals": 2, "state_class": "measurement"},
    {"id": "charging_rate", "name": "Charging Rate", "icon": "mdi:speedometer", "device_class": "speed", "unit": "km/h", "accuracy_decimals": 1, "state_class": "measurement"},
    {"id": "range_added", "name": "Range Added", "icon": "mdi:map-marker-plus", "device_class": "distance", "unit": "km", "accuracy_decimals": 0},
    {"id": "energy_added", "name": "Energy Added", "icon": "mdi:battery-charging", "device_class": "energy", "unit": "kWh", "accuracy_decimals": 1, "state_class": "total_increasing"},
    {"id": "time_to_full", "name": "Time to Full", "icon": "mdi:clock-outline", "device_class": "duration", "unit": "min", "state_class": "measurement"},
    {"id": "time_to_charge_limit", "name": "Time to Charge Limit", "icon": "mdi:clock-outline", "device_class": "duration", "unit": "min", "state_class": "measurement"},
    
    # Climate state sensors
    {"id": "passenger_temp_setting", "name": "Passenger Temperature Setting", "icon": "mdi:thermometer", "device_class": "temperature", "unit": "°C", "accuracy_decimals": 1, "state_class": "measurement"},
    {"id": "outside_temp", "name": "Outside Temperature", "icon": "mdi:thermometer", "device_class": "temperature", "unit": "°C", "accuracy_decimals": 1, "state_class": "measurement"},
    
    # Speed limit mode's current limit
    {"id": "speed_limit", "name": "Speed Limit", "icon": "mdi:speedometer-slow", "device_class": "speed", "unit": "km/h", "accuracy_decimals": 0, "state_class": "measurement"},

    # Drive state sensors
    {"id": "odometer", "name": "Odometer", "icon": "mdi:counter", "device_class": "distance", "unit": "km", "state_class": "total_increasing"},
    
    # Tire pressure sensors
    {"id": "tpms_front_left", "name": "TPMS Front Left", "icon": "mdi:tire", "device_class": "pressure", "unit": "bar", "accuracy_decimals": 1, "state_class": "measurement"},
    {"id": "tpms_front_right", "name": "TPMS Front Right", "icon": "mdi:tire", "device_class": "pressure", "unit": "bar", "accuracy_decimals": 1, "state_class": "measurement"},
    {"id": "tpms_rear_left", "name": "TPMS Rear Left", "icon": "mdi:tire", "device_class": "pressure", "unit": "bar", "accuracy_decimals": 1, "state_class": "measurement"},
    {"id": "tpms_rear_right", "name": "TPMS Rear Right", "icon": "mdi:tire", "device_class": "pressure", "unit": "bar", "accuracy_decimals": 1, "state_class": "measurement"},
]

TEXT_SENSORS = [
    {"id": "charging_state", "name": "Charging", "icon": "mdi:ev-station"},
    {"id": "iec61851_state", "name": "IEC 61851", "icon": "mdi:ev-plug-type2", "disabled_by_default": True},
    {"id": "shift_state", "name": "Shift State", "icon": "mdi:car-shift-pattern", "disabled_by_default": True},
    {"id": "charge_limit_reason", "name": "Charge Limit Reason", "icon": "mdi:ev-plug-tesla"},
    {"id": "scheduled_charging_mode", "name": "Scheduled Charging Mode", "icon": "mdi:calendar-clock"},
    {"id": "steering_wheel_heat_level", "name": "Steering Wheel Heat Level", "icon": "mdi:steering"},
    {"id": "seat_heater_front_left_level", "name": "Seat Heater Front Left Level", "icon": "mdi:car-seat-heater", "disabled_by_default": True},
    {"id": "seat_heater_front_right_level", "name": "Seat Heater Front Right Level", "icon": "mdi:car-seat-heater", "disabled_by_default": True},
    {"id": "seat_heater_rear_left_level", "name": "Seat Heater Rear Left Level", "icon": "mdi:car-seat-heater", "disabled_by_default": True},
    {"id": "seat_heater_rear_center_level", "name": "Seat Heater Rear Center Level", "icon": "mdi:car-seat-heater", "disabled_by_default": True},
    {"id": "seat_heater_rear_right_level", "name": "Seat Heater Rear Right Level", "icon": "mdi:car-seat-heater", "disabled_by_default": True},
    {"id": "last_command", "name": "Last Command", "icon": "mdi:history", "entity_category": "diagnostic", "disabled_by_default": True, "setter": "set_last_command_text_sensor"},
    # Now playing (the media player entity carries no titles); empty while nothing plays
    {"id": "media_title", "name": "Media Title", "icon": "mdi:music-note"},
    {"id": "media_artist", "name": "Media Artist", "icon": "mdi:account-music"},
    {"id": "media_source", "name": "Media Source", "icon": "mdi:radio"},
    # BLE MAC discovery: Searching / Found / Not found / Configured (ble_mac_address in YAML)
    {"id": "discovery", "name": "Discovery", "icon": "mdi:car-search", "entity_category": "diagnostic"},
    {"id": "ble_mac", "name": "BLE MAC", "icon": "mdi:bluetooth", "entity_category": "diagnostic"},
]

BUTTONS = [
    {"id": "wake", "name": "Wake up", "class": TeslaWakeButton, "setter": "set_wake_button", "icon": "mdi:sleep-off"},
    {"id": "pair", "name": "Pair BLE Key", "class": TeslaPairButton, "setter": "set_pair_button", "icon": "mdi:key-wireless", "entity_category": "diagnostic"},
    {"id": "regenerate_key", "name": "Regenerate key", "class": TeslaRegenerateKeyButton, "setter": "set_regenerate_key_button", "icon": "mdi:key-change", "entity_category": "diagnostic", "disabled_by_default": True},
    {"id": "force_update", "name": "Force data update", "class": TeslaForceUpdateButton, "setter": "set_force_update_button", "icon": "mdi:database-sync", "entity_category": "diagnostic"},
    # Search for the car's BLE MAC (active scan for 2 min); see Discovery / BLE MAC
    {"id": "find_car", "name": "Find Car", "class": TeslaFindCarButton, "setter": None, "icon": "mdi:car-search", "entity_category": "diagnostic"},
    # Unique actions (not part of combined entities)
    {"id": "unlatch_driver_door", "name": "Unlatch Driver Door", "class": TeslaUnlatchDriverDoorButton, "setter": None, "icon": "mdi:car-door", "disabled_by_default": True},
    {"id": "release_charge_cable", "name": "Release Charge Cable", "class": TeslaReleaseChargeCableButton, "setter": None, "icon": "mdi:ev-plug-tesla"},
    # Vehicle controls
    {"id": "flash_lights", "name": "Flash Lights", "class": TeslaFlashLightsButton, "setter": None, "icon": "mdi:car-light-high"},
    {"id": "honk_horn", "name": "Sound Horn", "class": TeslaHonkHornButton, "setter": None, "icon": "mdi:bullhorn"},
    # Track skip: Home Assistant does not offer next/previous on ESPHome media players
    {"id": "media_next_track", "name": "Media Next Track", "class": TeslaMediaNextTrackButton, "setter": None, "icon": "mdi:skip-next"},
    {"id": "media_previous_track", "name": "Media Previous Track", "class": TeslaMediaPreviousTrackButton, "setter": None, "icon": "mdi:skip-previous"},
]

SWITCHES = [
    {"id": "charging", "name": "Charger", "class": TeslaChargingSwitch, "setter": "set_charging_switch", "icon": "mdi:ev-station"},
    {"id": "steering_wheel_heat", "name": "Heated Steering", "class": TeslaSteeringWheelHeatSwitch, "setter": "set_steering_wheel_heat_switch", "icon": "mdi:steering"},
    {"id": "sentry_mode", "name": "Sentry Mode", "class": TeslaSentryModeSwitch, "setter": "set_sentry_mode_switch", "icon": "mdi:shield-car"},
    # On = "start charging at" the Scheduled Charging Start time
    {"id": "scheduled_charging", "name": "Scheduled Charging", "class": TeslaScheduledChargingSwitch, "setter": "set_scheduled_charging_switch", "icon": "mdi:calendar-clock"},
    # On = depart by the Scheduled Departure Time (preconditioning / off-peak charging below)
    {"id": "scheduled_departure", "name": "Scheduled Departure", "class": TeslaScheduledDepartureSwitch, "setter": "set_scheduled_departure_switch", "icon": "mdi:car-clock"},
    # Not reported by the car over BLE: show the last state that was set (assumed state)
    {"id": "low_power_mode", "name": "Low Power Mode", "class": TeslaLowPowerModeSwitch, "setter": None, "icon": "mdi:leaf"},
    {"id": "keep_accessory_power", "name": "Keep Accessory Power", "class": TeslaKeepAccessoryPowerSwitch, "setter": None, "icon": "mdi:power-socket"},
    {"id": "guest_mode", "name": "Guest Mode", "class": TeslaGuestModeSwitch, "setter": None, "icon": "mdi:account-key"},
]

SELECTS = [
    {
        "id": "cabin_overheat_protection",
        "name": "Cabin Overheat Protection",
        "class": TeslaCabinOverheatSelect,
        "setter": "set_cabin_overheat_select",
        "icon": "mdi:snowflake-thermometer",
        # The activation temperature only applies to On (A/C), as in the Tesla app
        "options": ["Off", "Fan Only", "On 30 °C", "On 35 °C", "On 40 °C"],
    },
    {
        "id": "departure_preconditioning",
        "name": "Departure Preconditioning",
        "class": TeslaDeparturePreconditioningSelect,
        "setter": "set_departure_preconditioning_select",
        "icon": "mdi:car-defrost-front",
        "options": ["Off", "All Week", "Weekdays"],
    },
    {
        "id": "departure_off_peak",
        "name": "Departure Off-Peak Charging",
        "class": TeslaDepartureOffPeakSelect,
        "setter": "set_departure_off_peak_select",
        "icon": "mdi:transmission-tower",
        "options": ["Off", "All Week", "Weekdays"],
    },
]

# Time entities: "start charging at" (setting it also turns scheduled charging on)
TIMES = [
    {"id": "scheduled_charging_start", "name": "Scheduled Charging Start", "class": TeslaScheduledChargingTime, "setter": "set_scheduled_charging_time_entity", "icon": "mdi:clock-start"},
    # Setting it also turns scheduled departure on
    {"id": "scheduled_departure_time", "name": "Scheduled Departure Time", "class": TeslaDepartureTime, "setter": "set_departure_time_entity", "icon": "mdi:clock-end"},
    {"id": "off_peak_end_time", "name": "Off-Peak End Time", "class": TeslaOffPeakEndTime, "setter": "set_off_peak_end_time_entity", "icon": "mdi:clock-end"},
]

# Lock entities (combined sensor + control)
LOCKS = [
    {"id": "doors", "name": "Doors", "class": TeslaDoorsLock, "setter": "set_doors_lock", "icon": "mdi:car-door-lock"},
    {"id": "charge_port_latch", "name": "Charge Port Latch", "class": TeslaChargePortLatchLock, "setter": "set_charge_port_latch_lock", "icon": "mdi:ev-plug-tesla"},
]

# Cover entities (combined sensor + control)
COVERS = [
    {"id": "trunk", "name": "Trunk", "class": TeslaTrunkCover, "setter": "set_trunk_cover", "icon": "mdi:car-back", "device_class": "door"},
    {"id": "frunk", "name": "Frunk", "class": TeslaFrunkCover, "setter": "set_frunk_cover", "icon": "mdi:car", "device_class": "door"},
    {"id": "windows", "name": "Windows", "class": TeslaWindowsCover, "setter": "set_windows_cover", "icon": "mdi:car-door", "device_class": "awning"},
    {"id": "charge_port_door", "name": "Charge Port Door", "class": TeslaChargePortDoorCover, "setter": "set_charge_port_door_cover", "icon": "mdi:ev-plug-tesla", "device_class": "door"},
]

# Climate entity
CLIMATE = {
    "id": "climate",
    "name": "Climate",
    "class": TeslaClimate,
    "setter": "set_climate",
}

# Media player entity: play/pause and volume; Off while the car is asleep
MEDIA_PLAYER = {
    "id": "media",
    "name": "Media",
    "class": TeslaMediaPlayer,
    "setter": "set_media_player",
    "icon": "mdi:car-speaker",
}

NUMBERS = [
    {
        "id": "charging_amps",
        "name": "Charging Amps",
        "class": TeslaChargingAmpsNumber,
        "setter": "set_charging_amps_number",
        "icon": "mdi:current-ac",
        "unit": "A",
        "device_class": "current",
        "min": 0,
        "max": "config",  # Will use charging_amps_max from config
        "step": 1,
    },
    {
        "id": "charging_limit",
        "name": "Charging Limit",
        "class": TeslaChargingLimitNumber,
        "setter": "set_charging_limit_number",
        "icon": "mdi:battery-charging-100",
        "unit": "%",
        "device_class": "battery",
        "min": 50,
        "max": 100,
        "step": 1,
    },
]

# =============================================================================
# CONFIG SCHEMA
# =============================================================================

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(TeslaBLEVehicle),
            cv.GenerateID(CONF_INTERNAL_BLE_CLIENT_ID): cv.declare_id(TeslaBLEClient),
            cv.Required(CONF_NAME): cv.string,
            cv.Optional(CONF_DEVICE_ID): cv.sub_device_id,
            cv.Required(CONF_VIN): cv.string,
            # Optional: without it the car is found by its VIN-derived advert
            # name and the MAC is remembered in NVS.
            cv.Optional(CONF_BLE_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_CHARGING_AMPS_MAX, default=DEFAULT_CHARGING_AMPS_MAX): cv.int_range(min=1, max=48),
            cv.Optional(CONF_ROLE, default="DRIVER"): cv.enum(TESLA_ROLES, upper=True),
            # Polling intervals (in seconds)
            cv.Optional(CONF_VCSEC_POLL_INTERVAL, default=10): cv.int_range(min=5, max=300),
            cv.Optional(CONF_INFOTAINMENT_POLL_INTERVAL_AWAKE, default=30): cv.int_range(min=10, max=600), 
            cv.Optional(CONF_INFOTAINMENT_POLL_INTERVAL_ACTIVE, default=10): cv.int_range(min=5, max=120),
            cv.Optional(CONF_INFOTAINMENT_SLEEP_TIMEOUT, default=660): cv.int_range(min=60, max=3600),
            # Wake the car once after boot to fill all sensors. Later polls
            # never wake a sleeping car on their own.
            cv.Optional(CONF_WAKE_ON_BOOT, default=True): cv.boolean,
            # Present turns to away only after the car was neither connected
            # nor heard for this long. Longer than a BLE turn of the other
            # car, so Present does not flicker while the cars take turns.
            # While a car has no MAC at all, a search that ends "Not found"
            # is retried after this long (e.g. the car was away at boot).
            # "never" turns it off; a known MAC is never searched for again.
            cv.Optional(CONF_DISCOVERY_RETRY_INTERVAL, default="1h"): cv.Any(
                cv.one_of("never", lower=True),
                cv.All(
                    cv.positive_time_period_milliseconds,
                    cv.Range(min=cv.TimePeriod(minutes=5), max=cv.TimePeriod(hours=24)),
                ),
            ),
            cv.Optional(CONF_PRESENCE_TIMEOUT, default="5min"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(minutes=1), max=cv.TimePeriod(hours=1)),
            ),
            # BLE link parameters. With two cars on one ESP32, both links use
            # the same interval by default so their radio slots interleave
            # instead of colliding, and a long supervision timeout so a few
            # missed slots do not drop the link.
            cv.Optional(CONF_CONNECTION_INTERVAL, default="15ms"): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(min=cv.TimePeriod(microseconds=7500), max=cv.TimePeriod(seconds=4)),
            ),
            cv.Optional(CONF_SUPERVISION_TIMEOUT, default="6s"): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(min=cv.TimePeriod(milliseconds=100), max=cv.TimePeriod(seconds=32)),
            ),
        },
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
)


def _validate_link_params(config):
    interval_us = config[CONF_CONNECTION_INTERVAL].total_microseconds
    timeout_us = config[CONF_SUPERVISION_TIMEOUT].total_microseconds
    # The spec requires timeout > (1 + latency) * interval * 2; latency is 0.
    if timeout_us <= 2 * interval_us:
        raise cv.Invalid(
            f"{CONF_SUPERVISION_TIMEOUT} must be more than twice {CONF_CONNECTION_INTERVAL}"
        )
    return config


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _validate_link_params)


# =============================================================================
# HELPER FUNCTIONS
# =============================================================================

def get_device_class_const(component_module, device_class_str):
    """Convert device class string to the actual constant."""
    if device_class_str is None:
        return None
    return getattr(component_module, f"DEVICE_CLASS_{device_class_str.upper()}", None)


def _base_config(definition, id_type, suffix, vehicle_id, vehicle_name, device_id=None):
    config = {
        CONF_ID: cv.declare_id(id_type)(f"{vehicle_id}_{definition['id']}_{suffix}"),
        # With its own Home Assistant device (device_id) the car name is the
        # device name, which Home Assistant already puts in front of every
        # entity name - adding it here would show it twice.
        CONF_NAME: definition['name'] if device_id is not None else f"{vehicle_name} {definition['name']}",
        CONF_DISABLED_BY_DEFAULT: definition.get("disabled_by_default", False),
    }
    if device_id is not None:
        config[CONF_DEVICE_ID] = device_id
    if "icon" in definition:
        config[CONF_ICON] = definition["icon"]
    if definition.get("entity_category") == "diagnostic":
        config[CONF_ENTITY_CATEGORY] = ENTITY_CATEGORY_DIAGNOSTIC
    return config


def _with_device_class(config, module, definition):
    if "device_class" in definition:
        dc = get_device_class_const(module, definition["device_class"])
        if dc:
            config[CONF_DEVICE_CLASS] = dc
    return config


def _attach(var, entity, definition):
    cg.add(entity.set_parent(var))
    if definition.get("setter"):
        cg.add(getattr(var, definition["setter"])(entity))
    return entity


async def create_binary_sensor(var, definition, vehicle_id, vehicle_name, device_id=None):
    """Create a binary sensor and register with TeslaBLEVehicle using generic setter."""
    config = _with_device_class(
        _base_config(definition, binary_sensor.BinarySensor, "sensor", vehicle_id, vehicle_name, device_id),
        binary_sensor, definition)
    sens = await binary_sensor.new_binary_sensor(config)
    cg.add(var.set_binary_sensor(definition["id"], sens))
    return sens


async def create_sensor(var, definition, vehicle_id, vehicle_name, device_id=None):
    """Create a sensor and register with TeslaBLEVehicle using generic setter."""
    config = _base_config(definition, sensor.Sensor, "sensor", vehicle_id, vehicle_name, device_id)
    config[CONF_FORCE_UPDATE] = False
    if "unit" in definition:
        config[CONF_UNIT_OF_MEASUREMENT] = definition["unit"]
    if "accuracy_decimals" in definition:
        config[CONF_ACCURACY_DECIMALS] = definition["accuracy_decimals"]
    if "state_class" in definition:
        config[CONF_STATE_CLASS] = sensor.validate_state_class(definition["state_class"])
    config = _with_device_class(config, sensor, definition)
    sens = await sensor.new_sensor(config)
    cg.add(var.set_sensor(definition["id"], sens))
    return sens


async def create_text_sensor(var, definition, vehicle_id, vehicle_name, device_id=None):
    """Create a text sensor and register with TeslaBLEVehicle using generic setter."""
    config = _base_config(definition, text_sensor.TextSensor, "sensor", vehicle_id, vehicle_name, device_id)
    config[CONF_FORCE_UPDATE] = False
    sens = await text_sensor.new_text_sensor(config)
    if definition.get("setter"):
        cg.add(getattr(var, definition["setter"])(sens))
    else:
        cg.add(var.set_text_sensor(definition["id"], sens))
    return sens


async def create_button(var, definition, vehicle_id, vehicle_name, device_id=None):
    """Create a button and register with TeslaBLEVehicle."""
    return _attach(var, await button.new_button(
        _base_config(definition, definition["class"], "button", vehicle_id, vehicle_name, device_id)), definition)


async def create_switch(var, definition, vehicle_id, vehicle_name, device_id=None):
    """Create a switch and register with TeslaBLEVehicle."""
    config = _base_config(definition, definition["class"], "switch", vehicle_id, vehicle_name, device_id)
    config[CONF_RESTORE_MODE] = switch.RESTORE_MODES['RESTORE_DEFAULT_OFF']
    return _attach(var, await switch.new_switch(config), definition)


async def create_number(var, definition, config, vehicle_id, vehicle_name, device_id=None):
    """Create a number and register with TeslaBLEVehicle."""
    max_val = definition["max"]
    if max_val == "config":
        max_val = config.get(CONF_CHARGING_AMPS_MAX, DEFAULT_CHARGING_AMPS_MAX)
    num_config = _base_config(definition, definition["class"], "number", vehicle_id, vehicle_name, device_id)
    num_config[CONF_MODE] = number.NUMBER_MODES['AUTO']
    if "unit" in definition:
        num_config[CONF_UNIT_OF_MEASUREMENT] = definition["unit"]
    num_config = _with_device_class(num_config, number, definition)
    num = await number.new_number(
        num_config,
        min_value=definition["min"],
        max_value=max_val,
        step=definition["step"]
    )
    return _attach(var, num, definition)


async def create_select(var, definition, vehicle_id, vehicle_name, device_id=None):
    """Create a select and register with TeslaBLEVehicle."""
    config = _base_config(definition, definition["class"], "select", vehicle_id, vehicle_name, device_id)
    sel = cg.new_Pvariable(config[CONF_ID])
    await select.register_select(sel, config, options=definition["options"])
    return _attach(var, sel, definition)


async def create_time(var, definition, vehicle_id, vehicle_name, device_id=None):
    """Create a time entity and register with TeslaBLEVehicle."""
    config = _base_config(definition, definition["class"], "time", vehicle_id, vehicle_name, device_id)
    config[CONF_TYPE] = "TIME"
    return _attach(var, await datetime.new_datetime(config), definition)


async def create_lock(var, definition, vehicle_id, vehicle_name, device_id=None):
    """Create a lock and register with TeslaBLEVehicle."""
    config = _base_config(definition, definition["class"], "lock", vehicle_id, vehicle_name, device_id)
    lck = cg.new_Pvariable(config[CONF_ID])
    await lock.register_lock(lck, config)
    return _attach(var, lck, definition)


async def create_cover(var, definition, vehicle_id, vehicle_name, device_id=None):
    """Create a cover and register with TeslaBLEVehicle."""
    config = _base_config(definition, definition["class"], "cover", vehicle_id, vehicle_name, device_id)
    if "device_class" in definition:
        config[CONF_DEVICE_CLASS] = definition["device_class"]
    cvr = cg.new_Pvariable(config[CONF_ID])
    await cover.register_cover(cvr, config)
    return _attach(var, cvr, definition)


async def create_climate_entity(var, definition, vehicle_id, vehicle_name, device_id=None):
    """Create a climate entity and register with TeslaBLEVehicle."""
    from esphome.components.climate import CONF_VISUAL
    config = _base_config(definition, definition["class"], "climate", vehicle_id, vehicle_name, device_id)
    config.update({CONF_VISUAL: {}, CONF_ACCURACY_DECIMALS: 1})
    clm = cg.new_Pvariable(config[CONF_ID])
    await climate.register_climate(clm, config)
    return _attach(var, clm, definition)


async def create_media_player(var, definition, vehicle_id, vehicle_name, device_id=None):
    """Create the media player entity and register with TeslaBLEVehicle."""
    config = _base_config(definition, definition["class"], "media_player", vehicle_id, vehicle_name, device_id)
    player = cg.new_Pvariable(config[CONF_ID])
    await media_player.register_media_player(player, config)
    return _attach(var, player, definition)


# =============================================================================
# CODE GENERATION
# =============================================================================

async def to_code(config):
    # Tesla owns a private low-level BLE client. Using esp32_ble_client directly
    # avoids requiring a user-visible top-level ble_client: entry.
    esp32_ble.register_bt_logger(BTLoggers.GATT, BTLoggers.SMP)
    # Cache each car's GATT service table in NVS: after a BLE hand-over the
    # reconnect skips service discovery (written once per car).
    add_idf_sdkconfig_option("CONFIG_BT_GATTC_CACHE_NVS_FLASH", True)
    cg.add_define("USE_ESP32_BLE_UUID")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    ble_component_config = {
        CONF_ID: config[CONF_INTERNAL_BLE_CLIENT_ID],
    }
    ble_tracker_config = {
        esp32_ble_tracker.CONF_ESP32_BLE_ID: config[esp32_ble_tracker.CONF_ESP32_BLE_ID],
    }

    ble_var = cg.new_Pvariable(config[CONF_INTERNAL_BLE_CLIENT_ID])
    await cg.register_component(ble_var, ble_component_config)
    await esp32_ble_tracker.register_client(ble_var, ble_tracker_config)
    if CONF_BLE_MAC_ADDRESS in config:
        cg.add(ble_var.set_address(config[CONF_BLE_MAC_ADDRESS].as_hex))
        cg.add(var.set_mac_from_config(True))
    cg.add(ble_var.set_auto_connect(True))
    cg.add(ble_var.set_vehicle(var))
    interval_units = round(
        config[CONF_CONNECTION_INTERVAL].total_microseconds / CONN_INTERVAL_UNIT_US
    )
    timeout_units = round(
        config[CONF_SUPERVISION_TIMEOUT].total_microseconds / SUPERVISION_TIMEOUT_UNIT_US
    )
    cg.add(ble_var.set_link_params(interval_units, timeout_units))
    cg.add(var.set_ble_client(ble_var))

    cg.add(var.set_vin(config[CONF_VIN]))
    cg.add(var.set_debug_name(config[CONF_NAME]))
    
    vehicle_id = str(config[CONF_ID])
    vehicle_name = config[CONF_NAME]
    device_id = config.get(CONF_DEVICE_ID)
    role = config[CONF_ROLE]
    charging_amps_max = config[CONF_CHARGING_AMPS_MAX]
    vcsec_interval_seconds = config[CONF_VCSEC_POLL_INTERVAL]
    
    cg.add(var.set_update_interval(vcsec_interval_seconds * 1000))
    # str() strips cv.enum()'s EnumValue subclass so the short role word
    # ("DRIVER"/"CHARGING_MANAGER") reaches C++ instead of the long protobuf name.
    cg.add(var.set_role(str(role)))
    cg.add(var.set_charging_amps_max(charging_amps_max))
    
    # Set polling intervals (convert from seconds to milliseconds)
    cg.add(var.set_vcsec_poll_interval(vcsec_interval_seconds * 1000))
    cg.add(var.set_infotainment_poll_interval_awake(config[CONF_INFOTAINMENT_POLL_INTERVAL_AWAKE] * 1000))
    cg.add(var.set_infotainment_poll_interval_active(config[CONF_INFOTAINMENT_POLL_INTERVAL_ACTIVE] * 1000))
    cg.add(var.set_infotainment_sleep_timeout(config[CONF_INFOTAINMENT_SLEEP_TIMEOUT] * 1000))
    cg.add(var.set_wake_on_boot(config[CONF_WAKE_ON_BOOT]))
    cg.add(var.set_presence_timeout(int(config[CONF_PRESENCE_TIMEOUT].total_milliseconds)))
    retry = config[CONF_DISCOVERY_RETRY_INTERVAL]
    cg.add(var.set_discovery_retry_interval(0 if isinstance(retry, str) else int(retry.total_milliseconds)))
    
    for creators in (
        (BINARY_SENSORS, create_binary_sensor),
        (SENSORS, create_sensor),
        (TEXT_SENSORS, create_text_sensor),
        (BUTTONS, create_button),
        (SWITCHES, create_switch),
        (SELECTS, create_select),
        (TIMES, create_time),
        (LOCKS, create_lock),
        (COVERS, create_cover),
    ):
        for definition in creators[0]:
            await creators[1](var, definition, vehicle_id, vehicle_name, device_id)

    for definition in NUMBERS:
        await create_number(var, definition, config, vehicle_id, vehicle_name, device_id)

    await create_climate_entity(var, CLIMATE, vehicle_id, vehicle_name, device_id)
    await create_media_player(var, MEDIA_PLAYER, vehicle_id, vehicle_name, device_id)


# =============================================================================
# ACTION SCHEMAS
# =============================================================================

_SIMPLE_ACTION_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_id(TeslaBLEVehicle),
})

TESLA_WAKE_ACTION_SCHEMA = _SIMPLE_ACTION_SCHEMA
TESLA_PAIR_ACTION_SCHEMA = _SIMPLE_ACTION_SCHEMA
TESLA_REGENERATE_KEY_ACTION_SCHEMA = _SIMPLE_ACTION_SCHEMA
TESLA_FORCE_UPDATE_ACTION_SCHEMA = _SIMPLE_ACTION_SCHEMA

TESLA_SET_CHARGING_ACTION_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_id(TeslaBLEVehicle),
    cv.Required("state"): cv.templatable(cv.boolean),
})

TESLA_SET_CHARGING_AMPS_ACTION_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_id(TeslaBLEVehicle),
    cv.Required("amps"): cv.templatable(cv.int_range(min=0, max=80)),
})

TESLA_SET_CHARGING_LIMIT_ACTION_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_id(TeslaBLEVehicle),
    cv.Required("limit"): cv.templatable(cv.int_range(min=50, max=100)),
})


# =============================================================================
# ACTION REGISTRATION
# =============================================================================

async def _simple_to_code(config, action_id, template_arg):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


async def _param_to_code(config, action_id, template_arg, args, key, setter, ctype):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[key], args, ctype)
    cg.add(getattr(var, setter)(template_))
    return var


@automation.register_action(
    "tesla_ble_vehicle.wake", WakeAction, TESLA_WAKE_ACTION_SCHEMA, synchronous=True
)
async def tesla_wake_to_code(config, action_id, template_arg, args):
    return await _simple_to_code(config, action_id, template_arg)


@automation.register_action(
    "tesla_ble_vehicle.pair", PairAction, TESLA_PAIR_ACTION_SCHEMA, synchronous=True
)
async def tesla_pair_to_code(config, action_id, template_arg, args):
    return await _simple_to_code(config, action_id, template_arg)


@automation.register_action(
    "tesla_ble_vehicle.regenerate_key", RegenerateKeyAction, TESLA_REGENERATE_KEY_ACTION_SCHEMA, synchronous=True
)
async def tesla_regenerate_key_to_code(config, action_id, template_arg, args):
    return await _simple_to_code(config, action_id, template_arg)


@automation.register_action(
    "tesla_ble_vehicle.force_update", ForceUpdateAction, TESLA_FORCE_UPDATE_ACTION_SCHEMA, synchronous=True
)
async def tesla_force_update_to_code(config, action_id, template_arg, args):
    return await _simple_to_code(config, action_id, template_arg)


@automation.register_action(
    "tesla_ble_vehicle.set_charging", SetChargingAction, TESLA_SET_CHARGING_ACTION_SCHEMA, synchronous=True
)
async def tesla_set_charging_to_code(config, action_id, template_arg, args):
    return await _param_to_code(config, action_id, template_arg, args, "state", "set_state", bool)


@automation.register_action(
    "tesla_ble_vehicle.set_charging_amps", SetChargingAmpsAction, TESLA_SET_CHARGING_AMPS_ACTION_SCHEMA, synchronous=True
)
async def tesla_set_charging_amps_to_code(config, action_id, template_arg, args):
    return await _param_to_code(config, action_id, template_arg, args, "amps", "set_amps", int)


@automation.register_action(
    "tesla_ble_vehicle.set_charging_limit", SetChargingLimitAction, TESLA_SET_CHARGING_LIMIT_ACTION_SCHEMA, synchronous=True
)
async def tesla_set_charging_limit_to_code(config, action_id, template_arg, args):
    return await _param_to_code(config, action_id, template_arg, args, "limit", "set_limit", int)
