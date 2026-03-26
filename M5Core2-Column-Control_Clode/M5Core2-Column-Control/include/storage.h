#pragma once

#include "app_types.h"

namespace storage {

void load(AppState &state);
void savePumpConfig(const AppState &state, uint8_t pump_index);
void saveValveConfig(const AppState &state, uint8_t valve_index);
void saveTimerConfig(const AppState &state, uint8_t timer_index);
void saveWorkProgram(const AppState &state, uint8_t program_index);
void saveWorkPrograms(const AppState &state);
void saveProgramNames(const AppState &state);
void saveSystemSettings(const AppState &state);

}  // namespace storage
