#include "storage.h"

#include <Preferences.h>

namespace {

Preferences prefs;

struct StoredWorkStep {
  uint8_t type;
  uint8_t target_index;
  uint8_t aux_value;
};

struct StoredWorkProgram {
  StoredWorkStep steps[kProgramStepCount];
};

using StoredProgramsBlob = StoredWorkProgram[kProgramCount];

String flowKey(uint8_t pump_index) {
  return "p" + String(pump_index) + "_flow";
}

String doseKey(uint8_t pump_index) {
  return "p" + String(pump_index) + "_dose";
}

String valveSpeedKey(uint8_t valve_index) {
  return "v" + String(valve_index) + "_spd";
}

String valveZeroKey(uint8_t valve_index) {
  return "v" + String(valve_index) + "_zero";
}

String valveRatioKey(uint8_t valve_index) {
  return "v" + String(valve_index) + "_ratio";
}

String valveInvertKey(uint8_t valve_index) {
  return "v" + String(valve_index) + "_inv";
}

String timerHoursKey(uint8_t timer_index) {
  return "t" + String(timer_index) + "_h";
}

String timerMinutesKey(uint8_t timer_index) {
  return "t" + String(timer_index) + "_m";
}

String timerSecondsKey(uint8_t timer_index) {
  return "t" + String(timer_index) + "_s";
}

String workProgramKey(uint8_t program_index) {
  char key[8];
  snprintf(key, sizeof(key), "wp%02u", program_index);
  return String(key);
}

constexpr const char *kProgramsBlobKey = "work_prog";
constexpr const char *kProgramNamesBlobKey = "prog_names";
constexpr const char *kPumpUnitsKey = "sys_pump_k";
constexpr const char *kWifiSsidKey = "wifi_ssid";
constexpr const char *kWifiPassKey = "wifi_pass";

void copyStoredProgramToState(const StoredWorkProgram &stored, WorkProgramState &program) {
  for (uint8_t step_index = 0; step_index < kProgramStepCount; ++step_index) {
    const StoredWorkStep &from = stored.steps[step_index];
    WorkStepState &to = program.steps[step_index];
    to.type = static_cast<WorkStepType>(from.type);
    to.target_index = from.target_index;
    to.aux_value = from.aux_value;
  }
}

void copyStoredProgramsToState(const StoredProgramsBlob &stored, AppState &state) {
  for (uint8_t program_index = 0; program_index < kProgramCount; ++program_index) {
    copyStoredProgramToState(stored[program_index], state.programs[program_index]);
  }
}

void copyStateProgramToStored(const WorkProgramState &program, StoredWorkProgram &stored) {
  for (uint8_t step_index = 0; step_index < kProgramStepCount; ++step_index) {
    const WorkStepState &from = program.steps[step_index];
    StoredWorkStep &to = stored.steps[step_index];
    to.type = static_cast<uint8_t>(from.type);
    to.target_index = from.target_index;
    to.aux_value = from.aux_value;
  }
}

void copyStateProgramsToStored(const AppState &state, StoredProgramsBlob &stored) {
  for (uint8_t program_index = 0; program_index < kProgramCount; ++program_index) {
    copyStateProgramToStored(state.programs[program_index], stored[program_index]);
  }
}

}  // namespace

namespace storage {

void load(AppState &state) {
  if (!prefs.begin("column", true)) return;

  for (uint8_t i = 0; i < kPumpCount; ++i) {
    PumpState &pump = state.pumps[i];
    pump.flow_ml_hour = prefs.getFloat(flowKey(i).c_str(), pump.flow_ml_hour);
    pump.target_volume_ml = prefs.getFloat(doseKey(i).c_str(), pump.target_volume_ml);
    pump.pumped_volume_ml = 0.0f;
    pump.last_redraw_volume_ml = 0.0f;
    pump.last_redraw_ms = 0;
    pump.last_config_change_ms = 0;
    pump.running = false;
    pump.config_dirty = false;
  }

  for (uint8_t i = 0; i < kValveCount; ++i) {
    ValveState &valve = state.valves[i];
    valve.speed_steps = prefs.getFloat(valveSpeedKey(i).c_str(), valve.speed_steps);
    valve.zero_offset_steps = prefs.getUShort(valveZeroKey(i).c_str(), valve.zero_offset_steps);
    valve.ratio = prefs.getFloat(valveRatioKey(i).c_str(), valve.ratio);
    valve.invert_direction = prefs.getBool(valveInvertKey(i).c_str(), valve.invert_direction);
    valve.position_index = 0;
    valve.moving = false;
    valve.config_dirty = false;
    valve.motion = ValveMotion::Idle;
    valve.motion_started_ms = 0;
    valve.motion_duration_ms = 0;
    valve.last_config_change_ms = 0;
  }

  for (uint8_t i = 0; i < kTimerCount; ++i) {
    TimerState &timer = state.timers[i];
    timer.hours = prefs.getUChar(timerHoursKey(i).c_str(), timer.hours);
    timer.minutes = prefs.getUChar(timerMinutesKey(i).c_str(), timer.minutes);
    timer.seconds = prefs.getUChar(timerSecondsKey(i).c_str(), timer.seconds);
    timer.config_dirty = false;
    timer.last_config_change_ms = 0;
  }

  bool loaded_individual_programs = false;
  for (uint8_t program_index = 0; program_index < kProgramCount; ++program_index) {
    const String key = workProgramKey(program_index);
    if (prefs.getBytesLength(key.c_str()) != sizeof(StoredWorkProgram)) continue;
    StoredWorkProgram stored_program{};
    prefs.getBytes(key.c_str(), &stored_program, sizeof(stored_program));
    copyStoredProgramToState(stored_program, state.programs[program_index]);
    loaded_individual_programs = true;
  }

  if (!loaded_individual_programs) {
    const size_t programs_blob_size = prefs.getBytesLength(kProgramsBlobKey);
    if (programs_blob_size == sizeof(StoredProgramsBlob)) {
      StoredProgramsBlob stored_programs{};
      prefs.getBytes(kProgramsBlobKey, stored_programs, sizeof(stored_programs));
      copyStoredProgramsToState(stored_programs, state);
    } else if (programs_blob_size == sizeof(state.programs)) {
      prefs.getBytes(kProgramsBlobKey, state.programs, sizeof(state.programs));
    }
  }
  if (prefs.getBytesLength(kProgramNamesBlobKey) == sizeof(state.program_names))
    prefs.getBytes(kProgramNamesBlobKey, state.program_names, sizeof(state.program_names));

  state.system.pump_units_per_ml = prefs.getFloat(kPumpUnitsKey, state.system.pump_units_per_ml);
  prefs.getString(kWifiSsidKey, state.system.wifi_ssid, sizeof(state.system.wifi_ssid));
  prefs.getString(kWifiPassKey, state.system.wifi_password, sizeof(state.system.wifi_password));

  prefs.end();
  state.operator_manual_mode = false;
  state.work_config_dirty = false;
  state.last_work_config_change_ms = 0;
  state.work_runtime = {false, false, false, false, false, false, 0, 0, 0, 0, 0, 0, 0.0f};
  state.needs_redraw = true;
}

void savePumpConfig(const AppState &state, uint8_t pump_index) {
  if (pump_index >= kPumpCount) return;
  if (!prefs.begin("column", false)) return;

  const PumpState &pump = state.pumps[pump_index];
  prefs.putFloat(flowKey(pump_index).c_str(), pump.flow_ml_hour);
  prefs.putFloat(doseKey(pump_index).c_str(), pump.target_volume_ml);
  prefs.end();
}

void saveValveConfig(const AppState &state, uint8_t valve_index) {
  if (valve_index >= kValveCount) return;
  if (!prefs.begin("column", false)) return;

  const ValveState &valve = state.valves[valve_index];
  prefs.putFloat(valveSpeedKey(valve_index).c_str(), valve.speed_steps);
  prefs.putUShort(valveZeroKey(valve_index).c_str(), valve.zero_offset_steps);
  prefs.putFloat(valveRatioKey(valve_index).c_str(), valve.ratio);
  prefs.putBool(valveInvertKey(valve_index).c_str(), valve.invert_direction);
  prefs.end();
}

void saveTimerConfig(const AppState &state, uint8_t timer_index) {
  if (timer_index >= kTimerCount) return;
  if (!prefs.begin("column", false)) return;

  const TimerState &timer = state.timers[timer_index];
  prefs.putUChar(timerHoursKey(timer_index).c_str(), timer.hours);
  prefs.putUChar(timerMinutesKey(timer_index).c_str(), timer.minutes);
  prefs.putUChar(timerSecondsKey(timer_index).c_str(), timer.seconds);
  prefs.end();
}

void saveWorkProgram(const AppState &state, uint8_t program_index) {
  if (program_index >= kProgramCount) return;
  if (!prefs.begin("column", false)) return;

  StoredWorkProgram stored_program{};
  copyStateProgramToStored(state.programs[program_index], stored_program);
  const String key = workProgramKey(program_index);
  prefs.putBytes(key.c_str(), &stored_program, sizeof(stored_program));
  prefs.end();
}

void saveWorkPrograms(const AppState &state) {
  if (!prefs.begin("column", false)) return;
  for (uint8_t program_index = 0; program_index < kProgramCount; ++program_index) {
    StoredWorkProgram stored_program{};
    copyStateProgramToStored(state.programs[program_index], stored_program);
    const String key = workProgramKey(program_index);
    prefs.putBytes(key.c_str(), &stored_program, sizeof(stored_program));
  }
  prefs.end();
}

void saveProgramNames(const AppState &state) {
  if (!prefs.begin("column", false)) return;
  prefs.putBytes(kProgramNamesBlobKey, state.program_names, sizeof(state.program_names));
  prefs.end();
}

void saveSystemSettings(const AppState &state) {
  if (!prefs.begin("column", false)) return;
  prefs.putFloat(kPumpUnitsKey, state.system.pump_units_per_ml);
  prefs.putString(kWifiSsidKey, state.system.wifi_ssid);
  prefs.putString(kWifiPassKey, state.system.wifi_password);
  prefs.end();
}

}  // namespace storage
