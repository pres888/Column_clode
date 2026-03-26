#pragma once

#include <stdint.h>

enum class ScreenId : uint8_t {
  Home,
  Pump,
  PumpDetail,
  Valve,
  ValveDetail,
  Timer,
  TimerDetail,
  Work,
  WorkProgram,
  WorkStep,
  WorkRun
};

constexpr uint8_t kPumpCount = 6;
constexpr uint8_t kValveCount = 2;
constexpr uint8_t kTimerCount = 6;
constexpr uint8_t kProgramCount = 50;
constexpr uint8_t kProgramStepCount = 25;
constexpr uint8_t kValvePositionCount = 5;  // positions 0-4 (displayed as 1-5)
constexpr float kValveStepCalibration = 0.6f;

enum class ValveMotion : uint8_t {
  Idle,
  StepLeft,
  StepRight,
  Zeroing
};

enum class WorkStepType : uint8_t {
  None,
  Pump,
  Valve,
  Timer
};

struct PumpState {
  float flow_ml_hour;
  float target_volume_ml;
  float pumped_volume_ml;
  float last_redraw_volume_ml;
  uint32_t last_redraw_ms;
  uint32_t last_config_change_ms;
  bool running;
  bool config_dirty;
};

struct ValveState {
  float speed_steps;
  uint16_t zero_offset_steps;
  float ratio;
  uint8_t position_index;
  bool invert_direction;
  bool moving;
  bool config_dirty;
  ValveMotion motion;
  uint32_t motion_started_ms;
  uint32_t motion_duration_ms;
  uint32_t last_config_change_ms;
};

struct TimerState {
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  bool config_dirty;
  uint32_t last_config_change_ms;
};

struct WorkStepState {
  WorkStepType type;
  uint8_t target_index;
  uint8_t aux_value;
};

struct WorkProgramState {
  WorkStepState steps[kProgramStepCount];
};

struct WorkRuntimeState {
  bool running;
  bool step_started;
  bool pump_chunk_active;
  bool paused;
  bool completed;
  bool resume_pending;
  uint8_t program_index;
  uint8_t step_index;
  uint32_t step_started_ms;
  uint32_t step_duration_ms;
  uint32_t pump_chunk_end_ms;
  uint32_t paused_remaining_ms;
  float pump_remaining_ml;
};

struct SystemSettings {
  float pump_units_per_ml;
  char wifi_ssid[33];
  char wifi_password[65];
};

struct AppState {
  ScreenId screen = ScreenId::Home;
  int8_t selected_pump = -1;
  int8_t selected_valve = -1;
  int8_t selected_timer = -1;
  int8_t selected_program = -1;
  int8_t selected_work_step = -1;
  uint8_t work_program_page = 0;
  uint8_t work_step_page = 0;
  bool needs_redraw = true;
  bool operator_manual_mode = false;
  bool work_config_dirty = false;
  uint32_t last_work_config_change_ms = 0;
  PumpState pumps[kPumpCount] = {
      {120.0f, 25.0f, 0.0f, 0.0f, 0, 0, false, false},
      {120.0f, 25.0f, 0.0f, 0.0f, 0, 0, false, false},
      {120.0f, 25.0f, 0.0f, 0.0f, 0, 0, false, false},
      {120.0f, 25.0f, 0.0f, 0.0f, 0, 0, false, false},
      {120.0f, 25.0f, 0.0f, 0.0f, 0, 0, false, false},
      {120.0f, 25.0f, 0.0f, 0.0f, 0, 0, false, false},
  };
  ValveState valves[kValveCount] = {
      {600.0f, 120, 50.0f, 0, false, false, false, ValveMotion::Idle, 0, 0, 0},
      {600.0f, 120, 50.0f, 0, false, false, false, ValveMotion::Idle, 0, 0, 0},
  };
  TimerState timers[kTimerCount] = {
      {0, 5, 0, false, 0},
      {0, 5, 0, false, 0},
      {0, 5, 0, false, 0},
      {0, 5, 0, false, 0},
      {0, 5, 0, false, 0},
      {0, 5, 0, false, 0},
  };
  char program_names[kProgramCount][21] = {};
  WorkProgramState programs[kProgramCount] = {};
  WorkRuntimeState work_runtime = {false, false, false, false, false, false, 0, 0, 0, 0, 0, 0, 0.0f};
  SystemSettings system = {87.04f, "", ""};
};
