#pragma once

#include <M5Unified.h>

#include "app_types.h"

namespace ui {

enum class ManualValveAction : uint8_t {
  Left,
  Right,
  Zero,
  Stop
};

void init();
void tick(AppState &state);
bool startWorkProgram(AppState &state, uint8_t program_index);
void stopWorkProgram(AppState &state);
void pauseWorkProgram(AppState &state);
void resumeWorkProgram(AppState &state);
void skipWorkStep(AppState &state);
void updatePumpConfig(AppState &state, uint8_t pump_index, float flow_ml_hour, float dose_ml);
void startManualPump(AppState &state, uint8_t pump_index);
void stopManualPump(AppState &state, uint8_t pump_index);
void updateValveConfig(AppState &state, uint8_t valve_index, float speed_steps, uint16_t zero_offset_steps, float ratio,
                       bool invert_direction);
void commandManualValve(AppState &state, uint8_t valve_index, ManualValveAction action);
void updateTimerConfig(AppState &state, uint8_t timer_index, uint8_t hours, uint8_t minutes, uint8_t seconds);

}  // namespace ui
