#pragma once

namespace esphome {
namespace tesla_ble_vehicle {

enum class ControlStateCommand {
  CHARGING_STATE,
  SET_AMPS,
  SET_LIMIT,
  SET_STEERING_WHEEL_HEAT,
  SET_SENTRY_MODE,
  SET_CLIMATE,  // climate mode, temperature, preset or fan mode (one climate entity)
};

enum class ControlStateRefresh { NONE, CHARGE_STATE, CLIMATE_STATE, CLOSURES_STATE, MEDIA_STATE };

struct ControlStateDecision {
  bool publish_requested_state;
  bool republish_confirmed_state;  // show the car's last confirmed value again
  ControlStateRefresh refresh;
};

inline ControlStateDecision control_state_decision(ControlStateCommand command,
                                                    bool command_succeeded) {
  if (!command_succeeded) {
    // Entities that show the requested value at once (numbers, climate) go
    // back to what the car last confirmed; switches never left it.
    const bool shows_request = command == ControlStateCommand::SET_AMPS ||
                               command == ControlStateCommand::SET_LIMIT ||
                               command == ControlStateCommand::SET_CLIMATE;
    return {false, shows_request, ControlStateRefresh::NONE};
  }

  switch (command) {
    case ControlStateCommand::CHARGING_STATE:
    case ControlStateCommand::SET_AMPS:
    case ControlStateCommand::SET_LIMIT:
      return {true, false, ControlStateRefresh::CHARGE_STATE};
    case ControlStateCommand::SET_STEERING_WHEEL_HEAT:
    case ControlStateCommand::SET_CLIMATE:
      return {true, false, ControlStateRefresh::CLIMATE_STATE};
    case ControlStateCommand::SET_SENTRY_MODE:
      return {true, false, ControlStateRefresh::CLOSURES_STATE};
  }

  return {false, false, ControlStateRefresh::NONE};
}

}  // namespace tesla_ble_vehicle
}  // namespace esphome
