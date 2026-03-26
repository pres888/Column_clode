#pragma once

#include <stdint.h>

namespace serial_link {

enum class ValveAction : uint8_t {
  None,
  StepLeft,
  StepRight,
  Zero
};

void init();
void tick();
void setPumpUnitsPerMl(float value);
float getPumpUnitsPerMl();
void sendCommand(const char *command);
void startPumpDose(uint8_t pump_index, float dose_ml, float flow_ml_hour);
void stopPumpDose(uint8_t pump_index);
void startValveStep(uint8_t valve_index, bool move_right, float ratio, float speed_steps_min, bool invert_direction);
void zeroValve(uint8_t valve_index, uint16_t zero_offset_steps, float speed_steps_min, bool invert_direction);
void stopValveMotion(uint8_t valve_index);
bool takeValveCompletion(uint8_t &valve_index, ValveAction &action);

}  // namespace serial_link
