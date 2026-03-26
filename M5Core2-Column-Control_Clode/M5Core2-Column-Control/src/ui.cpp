#include "ui.h"
#include "serial_link.h"
#include "storage.h"

namespace {

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

constexpr int16_t kScreenW = 320;
constexpr int16_t kHeaderH = 28;
constexpr int16_t kTileW = 142;
constexpr int16_t kTileH = 76;

constexpr Rect kPumpTile{12, 44, kTileW, kTileH};
constexpr Rect kValveTile{166, 44, kTileW, kTileH};
constexpr Rect kTimerTile{12, 132, kTileW, kTileH};
constexpr Rect kWorkTile{166, 132, kTileW, kTileH};
constexpr Rect kBackButton{232, 4, 80, 20};

constexpr Rect kFlowMinusButton{18, 102, 88, 30};
constexpr Rect kFlowPlusButton{214, 102, 88, 30};
constexpr Rect kVolumeMinusButton{18, 146, 88, 30};
constexpr Rect kVolumePlusButton{214, 146, 88, 30};
constexpr Rect kStartStopButton{18, 196, 82, 28};
constexpr Rect kResetButton{110, 196, 84, 28};
constexpr Rect kPrimeButton{204, 196, 98, 28};
constexpr Rect kPumpedBox{216, 48, 86, 28};
constexpr Rect kProgressBar{18, 58, 186, 12};

constexpr Rect kValveInvertToggle{18, 40, 76, 26};
constexpr Rect kValveSpeedMinusButton{18, 102, 64, 26};
constexpr Rect kValveSpeedPlusButton{238, 102, 64, 26};
constexpr Rect kValveZeroMinusButton{18, 138, 64, 26};
constexpr Rect kValveZeroPlusButton{238, 138, 64, 26};
constexpr Rect kValveRatioMinusButton{18, 174, 64, 26};
constexpr Rect kValveRatioPlusButton{238, 174, 64, 26};
constexpr Rect kValveLeftButton{18, 206, 88, 24};
constexpr Rect kValveZeroButton{116, 206, 88, 24};
constexpr Rect kValveRightButton{214, 206, 88, 24};

constexpr Rect kTimerHoursMinusButton{18, 92, 64, 26};
constexpr Rect kTimerHoursPlusButton{238, 92, 64, 26};
constexpr Rect kTimerMinutesMinusButton{18, 128, 64, 26};
constexpr Rect kTimerMinutesPlusButton{238, 128, 64, 26};
constexpr Rect kTimerSecondsMinusButton{18, 164, 64, 26};
constexpr Rect kTimerSecondsPlusButton{238, 164, 64, 26};

constexpr Rect kWorkPrevProgramsButton{12, 206, 64, 26};
constexpr Rect kWorkNextProgramsButton{84, 206, 64, 26};
constexpr Rect kWorkProgramRunButton{210, 206, 98, 26};

constexpr Rect kWorkStepsPrevButton{12, 206, 64, 26};
constexpr Rect kWorkStepsNextButton{84, 206, 64, 26};
constexpr Rect kWorkProgramEditRunButton{210, 206, 98, 26};

constexpr Rect kWorkTypeMinusButton{18, 78, 78, 26};
constexpr Rect kWorkTypePlusButton{224, 78, 78, 26};
constexpr Rect kWorkTargetMinusButton{18, 112, 78, 26};
constexpr Rect kWorkTargetPlusButton{224, 112, 78, 26};
constexpr Rect kWorkAuxMinusButton{18, 146, 78, 26};
constexpr Rect kWorkAuxPlusButton{224, 146, 78, 26};
constexpr Rect kWorkClearStepButton{228, 206, 80, 24};

constexpr Rect kWorkRunStopButton{28, 206, 72, 24};
constexpr Rect kWorkRunPauseButton{124, 206, 72, 24};
constexpr Rect kWorkRunSkipButton{220, 206, 72, 24};
constexpr Rect kWorkRunPrimaryMinusButton{18, 122, 58, 28};
constexpr Rect kWorkRunPrimaryPlusButton{244, 122, 58, 28};
constexpr Rect kWorkRunSecondaryMinusButton{18, 158, 58, 28};
constexpr Rect kWorkRunSecondaryPlusButton{244, 158, 58, 28};

bool contains(const Rect &r, const lgfx::v1::touch_point_t &p) {
  return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

float clampValue(float value, float min_value, float max_value) {
  if (value < min_value) return min_value;
  if (value > max_value) return max_value;
  return value;
}

void drawControlRowText(const char *label, const String &value_text, const Rect &minus_button, const Rect &plus_button, int16_t y);
void drawControlRow(const char *label, float value, const char *unit, const Rect &minus_button, const Rect &plus_button, int16_t y);

void drawCompactValueRow(const char *label, const String &value_text, const Rect &minus_button, const Rect &plus_button, int16_t y) {
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.drawString(label, 18, y);

  M5.Display.fillRoundRect(104, y - 2, 112, 28, 8, 0x18E3);
  M5.Display.drawRoundRect(104, y - 2, 112, 28, 8, WHITE);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(value_text.c_str(), 160, y + 12);

  M5.Display.fillRoundRect(minus_button.x, minus_button.y, minus_button.w, minus_button.h, 8, 0x632C);
  M5.Display.fillRoundRect(plus_button.x, plus_button.y, plus_button.w, plus_button.h, 8, 0x632C);
  M5.Display.setTextSize(2);
  M5.Display.drawString("-", minus_button.x + minus_button.w / 2, minus_button.y + minus_button.h / 2);
  M5.Display.drawString("+", plus_button.x + plus_button.w / 2, plus_button.y + plus_button.h / 2);
}

void drawWideValueRow(const char *label, const String &value_text, const Rect &minus_button, const Rect &plus_button, int16_t y) {
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.drawString(label, 18, y);

  M5.Display.fillRoundRect(92, y - 2, 136, 28, 8, 0x18E3);
  M5.Display.drawRoundRect(92, y - 2, 136, 28, 8, WHITE);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(value_text.c_str(), 160, y + 12);

  M5.Display.fillRoundRect(minus_button.x, minus_button.y, minus_button.w, minus_button.h, 8, 0x632C);
  M5.Display.fillRoundRect(plus_button.x, plus_button.y, plus_button.w, plus_button.h, 8, 0x632C);
  M5.Display.setTextSize(2);
  M5.Display.drawString("-", minus_button.x + minus_button.w / 2, minus_button.y + minus_button.h / 2);
  M5.Display.drawString("+", plus_button.x + plus_button.w / 2, plus_button.y + plus_button.h / 2);
}

void drawHeader(const char *title, bool show_back) {
  M5.Display.fillRect(0, 0, kScreenW, kHeaderH, 0x18C3);
  M5.Display.setTextColor(WHITE, 0x18C3);
  M5.Display.setTextDatum(middle_left);
  M5.Display.setTextSize(1);
  M5.Display.drawString(title, 12, 14);

  if (show_back) {
    M5.Display.fillRoundRect(kBackButton.x, kBackButton.y, kBackButton.w, kBackButton.h, 8, 0x632C);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("BACK", kBackButton.x + kBackButton.w / 2, kBackButton.y + kBackButton.h / 2);
  }
}

void drawTile(const Rect &r, const char *label, uint16_t color) {
  M5.Display.fillRoundRect(r.x, r.y, r.w, r.h, 16, color);
  M5.Display.drawRoundRect(r.x, r.y, r.w, r.h, 16, WHITE);
  M5.Display.setTextColor(WHITE, color);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
}

void drawHome() {
  M5.Display.fillScreen(0x0841);
  drawHeader("COLUMN CONTROL", false);
  drawTile(kPumpTile, "PUMP", 0x0B5D);
  drawTile(kValveTile, "VALVE", 0x0418);
  drawTile(kTimerTile, "TIMER", 0x7A40);
  drawTile(kWorkTile, "WORK", 0x780F);
}

void drawPlaceholder(const char *title, const char *line1, const char *line2) {
  M5.Display.fillScreen(BLACK);
  drawHeader(title, true);
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.drawString(line1, 18, 58);
  M5.Display.drawString(line2, 18, 84);
}

Rect pumpCardRect(uint8_t index) {
  const int16_t x = index % 2 == 0 ? 12 : 166;
  const int16_t y = 40 + static_cast<int16_t>(index / 2) * 60;
  return Rect{x, y, 142, 52};
}

Rect valveCardRect(uint8_t index) {
  const int16_t x = 12;
  const int16_t y = 52 + static_cast<int16_t>(index) * 72;
  return Rect{x, y, 296, 60};
}

Rect timerCardRect(uint8_t index) {
  const int16_t x = index % 2 == 0 ? 12 : 166;
  const int16_t y = 40 + static_cast<int16_t>(index / 2) * 60;
  return Rect{x, y, 142, 52};
}

Rect programCardRect(uint8_t page_slot) {
  const int16_t x = page_slot % 2 == 0 ? 12 : 166;
  const int16_t y = 40 + static_cast<int16_t>(page_slot / 2) * 52;
  return Rect{x, y, 142, 44};
}

Rect workStepCardRect(uint8_t page_slot) {
  return Rect{12, static_cast<int16_t>(40 + page_slot * 32), 296, 28};
}

String formatTimerValue(const TimerState &timer) {
  char buffer[9];
  snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", timer.hours, timer.minutes, timer.seconds);
  return String(buffer);
}

String formatDurationMs(uint32_t duration_ms) {
  const uint32_t total_seconds = duration_ms / 1000UL;
  const uint32_t hours = total_seconds / 3600UL;
  const uint32_t minutes = (total_seconds / 60UL) % 60UL;
  const uint32_t seconds = total_seconds % 60UL;
  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", static_cast<unsigned long>(hours),
           static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds));
  return String(buffer);
}

const char *workStepTypeLabel(WorkStepType type) {
  switch (type) {
    case WorkStepType::Pump:
      return "PUMP";
    case WorkStepType::Valve:
      return "VALVE";
    case WorkStepType::Timer:
      return "TIMER";
    case WorkStepType::None:
    default:
      return "NONE";
  }
}

uint32_t timerDurationMs(const TimerState &timer) {
  return (static_cast<uint32_t>(timer.hours) * 3600UL + static_cast<uint32_t>(timer.minutes) * 60UL +
          static_cast<uint32_t>(timer.seconds)) *
         1000UL;
}

void setTimerFromSeconds(TimerState &timer, uint32_t total_seconds) {
  const uint32_t clamped = total_seconds > 359999UL ? 359999UL : total_seconds;
  timer.hours = static_cast<uint8_t>(clamped / 3600UL);
  timer.minutes = static_cast<uint8_t>((clamped / 60UL) % 60UL);
  timer.seconds = static_cast<uint8_t>(clamped % 60UL);
}

uint8_t programStepCount(const WorkProgramState &program) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < kProgramStepCount; ++i) {
    if (program.steps[i].type != WorkStepType::None) count = i + 1;
  }
  return count;
}

bool programIsEmpty(const WorkProgramState &program) {
  return programStepCount(program) == 0;
}

String workStepRunLabel(const WorkStepState &step) {
  switch (step.type) {
    case WorkStepType::Pump:
      return "P" + String((step.target_index % kPumpCount) + 1);
    case WorkStepType::Valve:
      return "V" + String((step.target_index % kValveCount) + 1) + (step.aux_value == 0 ? " ZERO" : " POS " + String(step.aux_value));
    case WorkStepType::Timer:
      return "T" + String((step.target_index % kTimerCount) + 1);
    case WorkStepType::None:
    default:
      return "Skip";
  }
}

String workStepSummary(const AppState &state, const WorkStepState &step) {
  switch (step.type) {
    case WorkStepType::Pump:
      return "Pump P" + String((step.target_index % kPumpCount) + 1);
    case WorkStepType::Valve:
      if (step.aux_value == 0) return "Valve V" + String((step.target_index % kValveCount) + 1) + " -> ZERO";
      return "Valve V" + String((step.target_index % kValveCount) + 1) + " -> " + String(step.aux_value);
    case WorkStepType::Timer:
      return "Timer T" + String((step.target_index % kTimerCount) + 1) + " " +
             formatTimerValue(state.timers[step.target_index % kTimerCount]);
    case WorkStepType::None:
    default:
      return "Skip";
  }
}

float progressPercent(const PumpState &pump) {
  if (pump.target_volume_ml <= 0.0f) return 0.0f;
  return clampValue((pump.pumped_volume_ml / pump.target_volume_ml) * 100.0f, 0.0f, 100.0f);
}

float valvePositionUnits(const ValveState &valve) {
  return 6.4f * valve.ratio * kValveStepCalibration;
}

uint32_t valveStepDurationMs(const ValveState &valve) {
  const float distance_units = valvePositionUnits(valve);
  const float feedrate_units_min = clampValue(valve.speed_steps, 10.0f, 6000.0f);
  return static_cast<uint32_t>(clampValue((distance_units / feedrate_units_min) * 60000.0f, 150.0f, 15000.0f));
}

uint32_t valveZeroDurationMs(const ValveState &valve) {
  const float distance_units = (valve.position_index * valvePositionUnits(valve)) + (valve.zero_offset_steps / 100.0f);
  const float feedrate_units_min = clampValue(valve.speed_steps, 10.0f, 6000.0f);
  return static_cast<uint32_t>(clampValue((distance_units / feedrate_units_min) * 60000.0f, 300.0f, 30000.0f));
}

uint32_t estimateWorkStepDurationMs(const AppState &state, const WorkStepState &step) {
  if (step.type == WorkStepType::Pump && step.target_index < kPumpCount) {
    const PumpState &pump = state.pumps[step.target_index];
    return static_cast<uint32_t>(
        clampValue((pump.target_volume_ml / max(pump.flow_ml_hour / 60.0f, 0.01f)) * 60000.0f, 500.0f, 86400000.0f));
  }

  if (step.type == WorkStepType::Valve && step.target_index < kValveCount) {
    const ValveState &valve = state.valves[step.target_index];
    if (step.aux_value == 0) return valveZeroDurationMs(valve);
    const uint8_t target_position = step.aux_value - 1;
    const uint8_t distance = valve.position_index > target_position ? valve.position_index - target_position
                                                                    : target_position - valve.position_index;
    return valveStepDurationMs(valve) * max<uint8_t>(distance, 1);
  }

  if (step.type == WorkStepType::Timer && step.target_index < kTimerCount)
    return timerDurationMs(state.timers[step.target_index]);

  return 0;
}

uint32_t estimateWorkRemainingMs(const AppState &state) {
  const WorkRuntimeState &runtime = state.work_runtime;
  if (!runtime.running || runtime.program_index >= kProgramCount || runtime.completed) return 0;

  const WorkProgramState &program = state.programs[runtime.program_index];
  uint32_t total = 0;
  const uint32_t now = millis();

  for (uint8_t i = runtime.step_index; i < kProgramStepCount; ++i) {
    const WorkStepState &step = program.steps[i];
    if (step.type == WorkStepType::None) continue;

    if (i == runtime.step_index && runtime.step_started) {
      if (runtime.paused)
        total += runtime.paused_remaining_ms;
      else {
        const uint32_t elapsed = now - runtime.step_started_ms;
        total += runtime.step_duration_ms > elapsed ? runtime.step_duration_ms - elapsed : 0;
      }
      if (step.type == WorkStepType::Valve && step.target_index < kValveCount && step.aux_value > 0) {
        const ValveState &valve = state.valves[step.target_index];
        const uint8_t target_position = step.aux_value - 1;
        const uint8_t distance = valve.position_index > target_position ? valve.position_index - target_position
                                                                        : target_position - valve.position_index;
        if (distance > 1) total += valveStepDurationMs(valve) * (distance - 1);
      }
    } else {
      total += estimateWorkStepDurationMs(state, step);
    }
  }

  return total;
}

const char *valveMotionLabel(const ValveState &valve) {
  switch (valve.motion) {
    case ValveMotion::StepLeft:
      return "MOVE LEFT";
    case ValveMotion::StepRight:
      return "MOVE RIGHT";
    case ValveMotion::Zeroing:
      return "ZEROING";
    case ValveMotion::Idle:
    default:
      return "IDLE";
  }
}

void stopPump(PumpState &pump) {
  pump.running = false;
}

void stopValve(ValveState &valve) {
  valve.moving = false;
  valve.motion = ValveMotion::Idle;
  valve.motion_started_ms = 0;
  valve.motion_duration_ms = 0;
}

void stopRuntimeMotion(AppState &state) {
  if (!state.work_runtime.running || state.work_runtime.program_index >= kProgramCount ||
      state.work_runtime.step_index >= kProgramStepCount)
    return;

  const WorkStepState &step = state.programs[state.work_runtime.program_index].steps[state.work_runtime.step_index];
  if (step.type == WorkStepType::Pump && step.target_index < kPumpCount) {
    serial_link::stopPumpDose(step.target_index);
    stopPump(state.pumps[step.target_index]);
    state.work_runtime.pump_chunk_active = false;
    state.work_runtime.pump_chunk_end_ms = 0;
    state.work_runtime.pump_remaining_ml = 0.0f;
  } else if (step.type == WorkStepType::Valve && step.target_index < kValveCount) {
    serial_link::stopValveMotion(step.target_index);
    stopValve(state.valves[step.target_index]);
  }
}

void stopWorkRuntime(AppState &state) {
  stopRuntimeMotion(state);
  state.work_runtime = {false, false, false, false, false, false, 0, 0, 0, 0, 0, 0, 0.0f};
}

void startWorkPumpChunk(AppState &state, uint8_t pump_index, uint32_t now) {
  if (pump_index >= kPumpCount) return;

  WorkRuntimeState &runtime = state.work_runtime;
  PumpState &pump = state.pumps[pump_index];
  const float flow_ml_s = max(pump.flow_ml_hour / 3600.0f, 0.0001f);
  const float chunk_ml = min(runtime.pump_remaining_ml, clampValue(flow_ml_s, 0.005f, 0.15f));

  if (chunk_ml <= 0.0f) return;

  serial_link::startPumpDose(pump_index, chunk_ml, pump.flow_ml_hour);
  runtime.pump_remaining_ml = max(runtime.pump_remaining_ml - chunk_ml, 0.0f);
  runtime.pump_chunk_end_ms = now + static_cast<uint32_t>((chunk_ml / flow_ml_s) * 1000.0f);
  runtime.pump_chunk_active = true;
  pump.running = true;
  pump.last_redraw_ms = now;
  pump.last_redraw_volume_ml = pump.pumped_volume_ml;
}

void pauseWorkRuntime(AppState &state) {
  WorkRuntimeState &runtime = state.work_runtime;
  if (!runtime.running || runtime.paused || runtime.completed || runtime.program_index >= kProgramCount ||
      runtime.step_index >= kProgramStepCount)
    return;

  const uint32_t now = millis();
  runtime.paused_remaining_ms = 0;
  if (runtime.step_started) {
    const uint32_t elapsed = now - runtime.step_started_ms;
    runtime.paused_remaining_ms = runtime.step_duration_ms > elapsed ? runtime.step_duration_ms - elapsed : 0;
  }

  stopRuntimeMotion(state);
  if (runtime.program_index < kProgramCount && runtime.step_index < kProgramStepCount) {
    const WorkStepState &step = state.programs[runtime.program_index].steps[runtime.step_index];
    if (step.type == WorkStepType::Pump && step.target_index < kPumpCount)
      runtime.pump_remaining_ml = max(state.pumps[step.target_index].target_volume_ml - state.pumps[step.target_index].pumped_volume_ml,
                                      0.0f);
  }
  runtime.paused = true;
}

void resumeWorkRuntime(AppState &state) {
  WorkRuntimeState &runtime = state.work_runtime;
  if (!runtime.running || !runtime.paused || runtime.completed) return;

  runtime.paused = false;
  runtime.resume_pending = true;
  runtime.step_started = false;
}

bool startWorkProgramRuntime(AppState &state, uint8_t program_index) {
  if (program_index >= kProgramCount || programIsEmpty(state.programs[program_index])) return false;

  stopWorkRuntime(state);
  state.work_runtime.running = true;
  state.work_runtime.program_index = program_index;
  state.work_runtime.step_index = 0;
  state.work_runtime.step_started = false;
  state.work_runtime.paused = false;
  state.work_runtime.completed = false;
  state.work_runtime.resume_pending = false;
  state.work_runtime.paused_remaining_ms = 0;
  return true;
}

void applyActivePumpConfigChange(AppState &state, uint8_t pump_index, uint32_t now) {
  if (pump_index >= kPumpCount) return;

  PumpState &pump = state.pumps[pump_index];
  pump.last_config_change_ms = now;
  pump.config_dirty = true;
  pump.last_redraw_ms = now;
  pump.last_redraw_volume_ml = pump.pumped_volume_ml;

  WorkRuntimeState &runtime = state.work_runtime;
  if (!runtime.running || runtime.paused || runtime.completed || runtime.program_index >= kProgramCount ||
      runtime.step_index >= kProgramStepCount)
    return;

  WorkStepState &step = state.programs[runtime.program_index].steps[runtime.step_index];
  if (step.type != WorkStepType::Pump || step.target_index != pump_index) return;

  const float remaining_dose_ml = max(pump.target_volume_ml - pump.pumped_volume_ml, 0.0f);
  runtime.pump_remaining_ml = remaining_dose_ml;
  runtime.pump_chunk_active = false;
  runtime.pump_chunk_end_ms = 0;

  if (pump.running) {
    serial_link::stopPumpDose(step.target_index);
    if (remaining_dose_ml > 0.0f) {
      runtime.step_started_ms = now;
      runtime.step_duration_ms = static_cast<uint32_t>(
          clampValue((remaining_dose_ml / max(pump.flow_ml_hour / 60.0f, 0.01f)) * 60000.0f, 500.0f, 86400000.0f));
      stopPump(pump);
    } else {
      stopPump(pump);
      runtime.step_started = false;
      ++runtime.step_index;
    }
  }
}

void drawPumpCard(uint8_t index, const PumpState &pump) {
  const Rect card = pumpCardRect(index);
  const uint16_t fill = pump.running ? 0x14A5 : 0x18E3;

  M5.Display.fillRoundRect(card.x, card.y, card.w, card.h, 10, fill);
  M5.Display.drawRoundRect(card.x, card.y, card.w, card.h, 10, WHITE);
  M5.Display.setTextColor(WHITE, fill);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString(("P" + String(index + 1)).c_str(), card.x + card.w / 2, card.y + card.h / 2);
}

void drawValveCard(uint8_t index, const ValveState &valve) {
  const Rect card = valveCardRect(index);
  const uint16_t fill = valve.moving ? 0x0418 : 0x18E3;

  M5.Display.fillRoundRect(card.x, card.y, card.w, card.h, 12, fill);
  M5.Display.drawRoundRect(card.x, card.y, card.w, card.h, 12, WHITE);

  M5.Display.setTextColor(WHITE, fill);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
  M5.Display.drawString(("VALVE " + String(index + 1)).c_str(), card.x + 12, card.y + 8);

  M5.Display.setTextSize(1);
  M5.Display.drawString((String("POS ") + String(valve.position_index + 1)).c_str(), card.x + 12, card.y + 34);
  M5.Display.drawString(valveMotionLabel(valve), card.x + 92, card.y + 34);
}

void drawTimerCard(uint8_t index, const TimerState &timer) {
  const Rect card = timerCardRect(index);

  M5.Display.fillRoundRect(card.x, card.y, card.w, card.h, 10, 0x18E3);
  M5.Display.drawRoundRect(card.x, card.y, card.w, card.h, 10, WHITE);
  M5.Display.setTextColor(WHITE, 0x18E3);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.drawString(("TIMER " + String(index + 1)).c_str(), card.x + 8, card.y + 6);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString(formatTimerValue(timer).c_str(), card.x + card.w / 2, card.y + 34);
}

void drawPumpList(const AppState &state) {
  M5.Display.fillScreen(BLACK);
  drawHeader("PUMP", true);
  for (uint8_t i = 0; i < kPumpCount; ++i) drawPumpCard(i, state.pumps[i]);
}

void drawValveList(const AppState &state) {
  M5.Display.fillScreen(BLACK);
  drawHeader("VALVE", true);
  for (uint8_t i = 0; i < kValveCount; ++i) drawValveCard(i, state.valves[i]);
}

void drawTimerList(const AppState &state) {
  M5.Display.fillScreen(BLACK);
  drawHeader("TIMER", true);
  for (uint8_t i = 0; i < kTimerCount; ++i) drawTimerCard(i, state.timers[i]);
}

void drawProgramCard(uint8_t index, const WorkProgramState &program) {
  const Rect card = programCardRect(index % 6);
  const uint16_t fill = programIsEmpty(program) ? 0x18E3 : 0x780F;

  M5.Display.fillRoundRect(card.x, card.y, card.w, card.h, 10, fill);
  M5.Display.drawRoundRect(card.x, card.y, card.w, card.h, 10, WHITE);
  M5.Display.setTextColor(WHITE, fill);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.drawString(("PROGRAM " + String(index + 1)).c_str(), card.x + 8, card.y + 6);
  M5.Display.drawString((String(programStepCount(program)) + " steps").c_str(), card.x + 8, card.y + 22);
}

void drawWorkList(const AppState &state) {
  M5.Display.fillScreen(BLACK);
  drawHeader("WORK", true);

  const uint8_t start_index = state.work_program_page * 6;
  for (uint8_t slot = 0; slot < 6; ++slot) {
    const uint8_t program_index = start_index + slot;
    if (program_index >= kProgramCount) break;
    drawProgramCard(program_index, state.programs[program_index]);
  }

  M5.Display.fillRoundRect(kWorkPrevProgramsButton.x, kWorkPrevProgramsButton.y, kWorkPrevProgramsButton.w,
                           kWorkPrevProgramsButton.h, 8, 0x632C);
  M5.Display.fillRoundRect(kWorkNextProgramsButton.x, kWorkNextProgramsButton.y, kWorkNextProgramsButton.w,
                           kWorkNextProgramsButton.h, 8, 0x632C);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(WHITE, 0x632C);
  M5.Display.setTextSize(1);
  M5.Display.drawString("<", kWorkPrevProgramsButton.x + kWorkPrevProgramsButton.w / 2,
                        kWorkPrevProgramsButton.y + kWorkPrevProgramsButton.h / 2);
  M5.Display.drawString(">", kWorkNextProgramsButton.x + kWorkNextProgramsButton.w / 2,
                        kWorkNextProgramsButton.y + kWorkNextProgramsButton.h / 2);
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.drawString((String(state.work_program_page + 1) + "/" + String((kProgramCount + 5) / 6)).c_str(), 160, 219);
}

void drawWorkProgram(const AppState &state) {
  if (state.selected_program < 0 || state.selected_program >= kProgramCount) return;

  const WorkProgramState &program = state.programs[state.selected_program];
  M5.Display.fillScreen(BLACK);
  drawHeader(("PROGRAM " + String(state.selected_program + 1)).c_str(), true);

  const uint8_t first_step = state.work_step_page * 5;
  for (uint8_t slot = 0; slot < 5; ++slot) {
    const uint8_t step_index = first_step + slot;
    if (step_index >= kProgramStepCount) break;
    const Rect row = workStepCardRect(slot);
    const WorkStepState &step = program.steps[step_index];
    const bool empty = step.type == WorkStepType::None;

    M5.Display.fillRoundRect(row.x, row.y, row.w, row.h, 8, empty ? 0x18E3 : 0x2048);
    M5.Display.drawRoundRect(row.x, row.y, row.w, row.h, 8, WHITE);
    M5.Display.setTextColor(WHITE, empty ? 0x18E3 : 0x2048);
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextSize(1);
    M5.Display.drawString((String(step_index + 1) + ".").c_str(), row.x + 8, row.y + 7);
    M5.Display.drawString(workStepSummary(state, step).c_str(), row.x + 34, row.y + 7);
  }

  M5.Display.fillRoundRect(kWorkStepsPrevButton.x, kWorkStepsPrevButton.y, kWorkStepsPrevButton.w, kWorkStepsPrevButton.h, 8,
                           0x632C);
  M5.Display.fillRoundRect(kWorkStepsNextButton.x, kWorkStepsNextButton.y, kWorkStepsNextButton.w, kWorkStepsNextButton.h, 8,
                           0x632C);
  M5.Display.fillRoundRect(kWorkProgramEditRunButton.x, kWorkProgramEditRunButton.y, kWorkProgramEditRunButton.w,
                           kWorkProgramEditRunButton.h, 8, 0x2589);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(WHITE, 0x632C);
  M5.Display.drawString("<", kWorkStepsPrevButton.x + kWorkStepsPrevButton.w / 2,
                        kWorkStepsPrevButton.y + kWorkStepsPrevButton.h / 2);
  M5.Display.drawString(">", kWorkStepsNextButton.x + kWorkStepsNextButton.w / 2,
                        kWorkStepsNextButton.y + kWorkStepsNextButton.h / 2);
  M5.Display.setTextColor(WHITE, 0x2589);
  M5.Display.drawString("RUN", kWorkProgramEditRunButton.x + kWorkProgramEditRunButton.w / 2,
                        kWorkProgramEditRunButton.y + kWorkProgramEditRunButton.h / 2);
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.drawString((String(state.work_step_page + 1) + "/5").c_str(), 160, 219);
}

void drawWorkStep(const AppState &state) {
  if (state.selected_program < 0 || state.selected_program >= kProgramCount) return;
  if (state.selected_work_step < 0 || state.selected_work_step >= kProgramStepCount) return;

  const WorkStepState &step = state.programs[state.selected_program].steps[state.selected_work_step];

  M5.Display.fillScreen(BLACK);
  drawHeader(("STEP " + String(state.selected_work_step + 1)).c_str(), true);
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.drawString(("Program " + String(state.selected_program + 1)).c_str(), 18, 36);
  M5.Display.drawString(workStepSummary(state, step).c_str(), 18, 52);

  drawCompactValueRow("Type", workStepTypeLabel(step.type), kWorkTypeMinusButton, kWorkTypePlusButton, 86);

  String target_text = "-";
  if (step.type == WorkStepType::Pump) target_text = "P" + String((step.target_index % kPumpCount) + 1);
  else if (step.type == WorkStepType::Valve) target_text = "V" + String((step.target_index % kValveCount) + 1);
  else if (step.type == WorkStepType::Timer) target_text = "T" + String((step.target_index % kTimerCount) + 1);
  drawCompactValueRow("Target", target_text, kWorkTargetMinusButton, kWorkTargetPlusButton, 120);

  String aux_text = "-";
  if (step.type == WorkStepType::Valve) aux_text = step.aux_value == 0 ? "ZERO" : "Pos " + String(step.aux_value);
  else if (step.type == WorkStepType::Timer) aux_text = formatTimerValue(state.timers[step.target_index % kTimerCount]);
  else if (step.type == WorkStepType::Pump) aux_text = String(state.pumps[step.target_index % kPumpCount].target_volume_ml, 1) + " ml";
  drawCompactValueRow("Value", aux_text, kWorkAuxMinusButton, kWorkAuxPlusButton, 154);

  M5.Display.fillRoundRect(kWorkClearStepButton.x, kWorkClearStepButton.y, kWorkClearStepButton.w, kWorkClearStepButton.h, 8,
                           0xD145);
  M5.Display.setTextColor(WHITE, 0xD145);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString("CLEAR", kWorkClearStepButton.x + kWorkClearStepButton.w / 2,
                        kWorkClearStepButton.y + kWorkClearStepButton.h / 2);
}

void drawWorkRunBase(const AppState &state) {
  const WorkRuntimeState &runtime = state.work_runtime;
  M5.Display.fillScreen(BLACK);
  drawHeader(("RUN P" + String(runtime.program_index + 1)).c_str(), true);
  M5.Display.fillRect(0, 200, kScreenW, 40, BLACK);

  M5.Display.fillRoundRect(kWorkRunStopButton.x, kWorkRunStopButton.y, kWorkRunStopButton.w, kWorkRunStopButton.h, 8, 0xD145);
  M5.Display.fillRoundRect(kWorkRunPauseButton.x, kWorkRunPauseButton.y, kWorkRunPauseButton.w, kWorkRunPauseButton.h, 8,
                           runtime.completed ? 0x18E3 : 0x2589);
  M5.Display.fillRoundRect(kWorkRunSkipButton.x, kWorkRunSkipButton.y, kWorkRunSkipButton.w, kWorkRunSkipButton.h, 8, 0x7A40);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(WHITE, 0xD145);
  M5.Display.drawString("STOP", kWorkRunStopButton.x + kWorkRunStopButton.w / 2,
                        kWorkRunStopButton.y + kWorkRunStopButton.h / 2);
  M5.Display.setTextColor(WHITE, runtime.completed ? 0x18E3 : 0x2589);
  M5.Display.drawString(runtime.completed ? "DONE" : (runtime.paused ? "RESUME" : "PAUSE"),
                        kWorkRunPauseButton.x + kWorkRunPauseButton.w / 2,
                        kWorkRunPauseButton.y + kWorkRunPauseButton.h / 2);
  M5.Display.setTextColor(WHITE, 0x7A40);
  M5.Display.drawString("SKIP", kWorkRunSkipButton.x + kWorkRunSkipButton.w / 2,
                        kWorkRunSkipButton.y + kWorkRunSkipButton.h / 2);
}

void drawWorkRunValueRow(const String &value_text, const Rect &minus_button, const Rect &plus_button, int16_t y) {
  constexpr int16_t field_x = 86;
  constexpr int16_t field_w = 148;
  constexpr uint16_t button_fill = 0x0418;

  M5.Display.fillRoundRect(field_x, y, field_w, 28, 8, 0x18E3);
  M5.Display.drawRoundRect(field_x, y, field_w, 28, 8, WHITE);

  M5.Display.fillRoundRect(minus_button.x, minus_button.y, minus_button.w, minus_button.h, 8, button_fill);
  M5.Display.fillRoundRect(plus_button.x, plus_button.y, plus_button.w, plus_button.h, 8, button_fill);

  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(WHITE, 0x18E3);
  M5.Display.setTextSize(1);
  M5.Display.drawString(value_text.c_str(), field_x + field_w / 2, y + 14);

  M5.Display.setTextColor(WHITE, button_fill);
  M5.Display.setTextSize(2);
  M5.Display.drawString("-", minus_button.x + minus_button.w / 2, minus_button.y + minus_button.h / 2);
  M5.Display.drawString("+", plus_button.x + plus_button.w / 2, plus_button.y + plus_button.h / 2);
}

void drawWorkRunDynamic(const AppState &state) {
  const WorkRuntimeState &runtime = state.work_runtime;
  M5.Display.fillRect(0, 200, kScreenW, 40, BLACK);
  M5.Display.fillRoundRect(kWorkRunStopButton.x, kWorkRunStopButton.y, kWorkRunStopButton.w, kWorkRunStopButton.h, 8, 0xD145);
  M5.Display.fillRoundRect(kWorkRunPauseButton.x, kWorkRunPauseButton.y, kWorkRunPauseButton.w, kWorkRunPauseButton.h, 8,
                           runtime.completed ? 0x18E3 : 0x2589);
  M5.Display.fillRoundRect(kWorkRunSkipButton.x, kWorkRunSkipButton.y, kWorkRunSkipButton.w, kWorkRunSkipButton.h, 8, 0x7A40);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(WHITE, 0xD145);
  M5.Display.drawString("STOP", kWorkRunStopButton.x + kWorkRunStopButton.w / 2,
                        kWorkRunStopButton.y + kWorkRunStopButton.h / 2);
  M5.Display.setTextColor(WHITE, runtime.completed ? 0x18E3 : 0x2589);
  M5.Display.drawString(runtime.completed ? "DONE" : (runtime.paused ? "RESUME" : "PAUSE"),
                        kWorkRunPauseButton.x + kWorkRunPauseButton.w / 2,
                        kWorkRunPauseButton.y + kWorkRunPauseButton.h / 2);
  M5.Display.setTextColor(WHITE, 0x7A40);
  M5.Display.drawString("SKIP", kWorkRunSkipButton.x + kWorkRunSkipButton.w / 2,
                        kWorkRunSkipButton.y + kWorkRunSkipButton.h / 2);
  M5.Display.fillRect(0, kHeaderH, kScreenW, 176, BLACK);
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);

  if (!runtime.running || runtime.program_index >= kProgramCount) {
    M5.Display.drawString("PROGRAM IDLE", 160, 72);
    return;
  }

  if (runtime.completed) {
    M5.Display.setTextColor(0x07E0, BLACK);
    M5.Display.drawString("PROGRAM COMPLETE", 160, 76);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.drawString(("STEPS  " + String(programStepCount(state.programs[runtime.program_index]))).c_str(), 160, 102);
    M5.Display.drawString("PRESS BACK OR STOP", 160, 130);
    return;
  }

  if (runtime.step_index >= kProgramStepCount) {
    M5.Display.drawString("PROGRAM IDLE", 160, 72);
    return;
  }

  const WorkStepState &step = state.programs[runtime.program_index].steps[runtime.step_index];
  const uint32_t now = millis();
  const uint32_t elapsed = runtime.step_started && !runtime.paused ? now - runtime.step_started_ms : 0;
  const uint32_t remaining_ms = runtime.paused ? runtime.paused_remaining_ms
                                               : (runtime.step_duration_ms > elapsed ? runtime.step_duration_ms - elapsed : 0);
  const uint32_t total_remaining_ms = estimateWorkRemainingMs(state);

  M5.Display.drawString(("STEP " + String(runtime.step_index + 1) + "/" +
                         String(programStepCount(state.programs[runtime.program_index])))
                            .c_str(),
                        160, 42);
  M5.Display.setTextSize(2);
  M5.Display.drawString(workStepRunLabel(step).c_str(), 160, 64);
  M5.Display.setTextSize(1);
  if (runtime.paused) {
    M5.Display.setTextColor(0xFFE0, BLACK);
    M5.Display.drawString("PAUSED", 160, 78);
    M5.Display.setTextColor(WHITE, BLACK);
  }
  M5.Display.drawString(("STEP LEFT  " + formatDurationMs(remaining_ms)).c_str(), 160, 88);
  M5.Display.drawString(("PROGRAM LEFT  " + formatDurationMs(total_remaining_ms)).c_str(), 160, 102);

  if (step.type == WorkStepType::Pump && step.target_index < kPumpCount) {
    const PumpState &pump = state.pumps[step.target_index];
    drawWorkRunValueRow("FLOW  " + String(pump.flow_ml_hour / 60.0f, 1) + " ml/min", kWorkRunPrimaryMinusButton,
                        kWorkRunPrimaryPlusButton, 122);
    drawWorkRunValueRow("DOSE  " + String(pump.target_volume_ml, 1) + " ml", kWorkRunSecondaryMinusButton,
                        kWorkRunSecondaryPlusButton, 158);
  } else if (step.type == WorkStepType::Timer && step.target_index < kTimerCount) {
    drawWorkRunValueRow("TIME  " + formatTimerValue(state.timers[step.target_index]), kWorkRunPrimaryMinusButton,
                        kWorkRunPrimaryPlusButton, 122);
    M5.Display.setTextColor(0x03FF, BLACK);
    M5.Display.drawString("CHANGE BY 5 SEC", 160, 174);
  } else if (step.type == WorkStepType::Valve && step.target_index < kValveCount) {
    const String target_text = step.aux_value == 0 ? "TARGET  ZERO" : "TARGET  POS " + String(step.aux_value);
    drawWorkRunValueRow(target_text, kWorkRunPrimaryMinusButton, kWorkRunPrimaryPlusButton, 122);
    const ValveState &valve = state.valves[step.target_index];
    M5.Display.setTextColor(0x03FF, BLACK);
    M5.Display.drawString(("CURRENT POS  " + String(valve.position_index + 1)).c_str(), 160, 174);
  }
}

void drawWorkRun(const AppState &state) {
  drawWorkRunBase(state);
  drawWorkRunDynamic(state);
}

void drawControlRow(const char *label, float value, const char *unit, const Rect &minus_button, const Rect &plus_button, int16_t y) {
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.drawString(label, 18, y);

  M5.Display.fillRoundRect(112, y - 2, 96, 30, 8, 0x18E3);
  M5.Display.drawRoundRect(112, y - 2, 96, 30, 8, WHITE);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString((String(value, 1) + " " + unit).c_str(), 160, y + 13);

  M5.Display.fillRoundRect(minus_button.x, minus_button.y, minus_button.w, minus_button.h, 8, 0x632C);
  M5.Display.fillRoundRect(plus_button.x, plus_button.y, plus_button.w, plus_button.h, 8, 0x632C);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(3);
  M5.Display.drawString("-", minus_button.x + minus_button.w / 2, minus_button.y + minus_button.h / 2);
  M5.Display.drawString("+", plus_button.x + plus_button.w / 2, plus_button.y + plus_button.h / 2);
}

void drawControlRowText(const char *label, const String &value_text, const Rect &minus_button, const Rect &plus_button, int16_t y) {
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.drawString(label, 18, y);

  M5.Display.fillRoundRect(112, y - 2, 96, 30, 8, 0x18E3);
  M5.Display.drawRoundRect(112, y - 2, 96, 30, 8, WHITE);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(value_text.c_str(), 160, y + 13);

  M5.Display.fillRoundRect(minus_button.x, minus_button.y, minus_button.w, minus_button.h, 8, 0x632C);
  M5.Display.fillRoundRect(plus_button.x, plus_button.y, plus_button.w, plus_button.h, 8, 0x632C);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(3);
  M5.Display.drawString("-", minus_button.x + minus_button.w / 2, minus_button.y + minus_button.h / 2);
  M5.Display.drawString("+", plus_button.x + plus_button.w / 2, plus_button.y + plus_button.h / 2);
}

void drawPumpDetail(const AppState &state) {
  if (state.selected_pump < 0 || state.selected_pump >= kPumpCount) return;

  const PumpState &pump = state.pumps[state.selected_pump];
  const float progress_pct = progressPercent(pump);

  M5.Display.fillScreen(BLACK);
  drawHeader(("PUMP " + String(state.selected_pump + 1)).c_str(), true);

  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.drawString((String(pump.flow_ml_hour, 1) + " ml/h").c_str(), 18, 36);

  M5.Display.fillRoundRect(kPumpedBox.x, kPumpedBox.y, kPumpedBox.w, kPumpedBox.h, 8, 0x18E3);
  M5.Display.drawRoundRect(kPumpedBox.x, kPumpedBox.y, kPumpedBox.w, kPumpedBox.h, 8, WHITE);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString((String(pump.pumped_volume_ml, 1) + " ml").c_str(), kPumpedBox.x + kPumpedBox.w / 2,
                        kPumpedBox.y + kPumpedBox.h / 2);

  M5.Display.drawRoundRect(kProgressBar.x, kProgressBar.y, kProgressBar.w, kProgressBar.h, 6, WHITE);
  M5.Display.fillRoundRect(kProgressBar.x, kProgressBar.y,
                           static_cast<int16_t>(kProgressBar.w * (progress_pct / 100.0f)), kProgressBar.h, 6, 0x07E0);

  drawControlRow("Flow", pump.flow_ml_hour / 60.0f, "ml/min", kFlowMinusButton, kFlowPlusButton, 104);
  drawControlRow("Dose", pump.target_volume_ml, "ml", kVolumeMinusButton, kVolumePlusButton, 148);

  M5.Display.fillRoundRect(kStartStopButton.x, kStartStopButton.y, kStartStopButton.w, kStartStopButton.h, 10,
                           pump.running ? 0xD145 : 0x2589);
  M5.Display.fillRoundRect(kResetButton.x, kResetButton.y, kResetButton.w, kResetButton.h, 10, 0x7A40);
  M5.Display.fillRoundRect(kPrimeButton.x, kPrimeButton.y, kPrimeButton.w, kPrimeButton.h, 10, 0x03EF);

  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.drawString(pump.running ? "STOP" : "START", kStartStopButton.x + kStartStopButton.w / 2,
                        kStartStopButton.y + kStartStopButton.h / 2);
  M5.Display.drawString("RESET", kResetButton.x + kResetButton.w / 2, kResetButton.y + kResetButton.h / 2);
  M5.Display.drawString("PRIME", kPrimeButton.x + kPrimeButton.w / 2, kPrimeButton.y + kPrimeButton.h / 2);
}

void drawValveDots(uint8_t position_index, int16_t y) {
  const int16_t start_x = 72;

  for (uint8_t i = 0; i < 5; ++i) {
    const int16_t x = start_x + static_cast<int16_t>(i) * 44;
    const uint16_t color = i == position_index ? 0x03FF : 0x7BEF;
    M5.Display.fillCircle(x, y, 8, color);
    M5.Display.drawCircle(x, y, 8, WHITE);
  }
}

void drawValveDetail(const AppState &state) {
  if (state.selected_valve < 0 || state.selected_valve >= kValveCount) return;

  const ValveState &valve = state.valves[state.selected_valve];

  M5.Display.fillScreen(BLACK);
  drawHeader(("VALVE " + String(state.selected_valve + 1)).c_str(), true);

  M5.Display.fillRoundRect(kValveInvertToggle.x, kValveInvertToggle.y, kValveInvertToggle.w, kValveInvertToggle.h, 6,
                           valve.invert_direction ? 0x07E0 : 0x39E7);
  M5.Display.drawRoundRect(kValveInvertToggle.x, kValveInvertToggle.y, kValveInvertToggle.w, kValveInvertToggle.h, 6, WHITE);
  M5.Display.setTextDatum(middle_left);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(WHITE, valve.invert_direction ? 0x07E0 : 0x39E7);
  M5.Display.drawString("REV", kValveInvertToggle.x + 8, kValveInvertToggle.y + kValveInvertToggle.h / 2);
  if (valve.invert_direction) {
    M5.Display.drawLine(kValveInvertToggle.x + 48, kValveInvertToggle.y + 13, kValveInvertToggle.x + 54,
                        kValveInvertToggle.y + 19, WHITE);
    M5.Display.drawLine(kValveInvertToggle.x + 54, kValveInvertToggle.y + 19, kValveInvertToggle.x + 66,
                        kValveInvertToggle.y + 7, WHITE);
  }

  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.drawString(("POS " + String(valve.position_index + 1) + "/5").c_str(), 160, 48);
  M5.Display.drawString(valveMotionLabel(valve), 160, 66);

  drawValveDots(valve.position_index, 86);

  drawWideValueRow("F", String(valve.speed_steps, 0), kValveSpeedMinusButton, kValveSpeedPlusButton, 104);
  drawWideValueRow("Zero", String(valve.zero_offset_steps) + " st", kValveZeroMinusButton, kValveZeroPlusButton, 140);
  drawWideValueRow("Ratio", String(valve.ratio, 1) + ":1", kValveRatioMinusButton, kValveRatioPlusButton, 176);

  M5.Display.fillRoundRect(kValveLeftButton.x, kValveLeftButton.y, kValveLeftButton.w, kValveLeftButton.h, 10, 0x632C);
  M5.Display.fillRoundRect(kValveZeroButton.x, kValveZeroButton.y, kValveZeroButton.w, kValveZeroButton.h, 10,
                           valve.motion == ValveMotion::Zeroing ? 0xD145 : 0x2589);
  M5.Display.fillRoundRect(kValveRightButton.x, kValveRightButton.y, kValveRightButton.w, kValveRightButton.h, 10, 0x632C);

  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.drawString("LEFT", kValveLeftButton.x + kValveLeftButton.w / 2, kValveLeftButton.y + kValveLeftButton.h / 2);
  M5.Display.drawString(valve.motion == ValveMotion::Zeroing ? "STOP" : "ZERO",
                        kValveZeroButton.x + kValveZeroButton.w / 2, kValveZeroButton.y + kValveZeroButton.h / 2);
  M5.Display.drawString("RIGHT", kValveRightButton.x + kValveRightButton.w / 2,
                        kValveRightButton.y + kValveRightButton.h / 2);
}

void drawTimerAdjustColumn(const char *label, uint8_t value, const Rect &minus_button, const Rect &plus_button, int16_t x) {
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.drawString(label, x + 26, 88);

  M5.Display.fillRoundRect(minus_button.x, minus_button.y, minus_button.w, minus_button.h, 8, 0x632C);
  M5.Display.fillRoundRect(plus_button.x, plus_button.y, plus_button.w, plus_button.h, 8, 0x632C);
  M5.Display.setTextSize(2);
  M5.Display.drawString("+", plus_button.x + plus_button.w / 2, plus_button.y + plus_button.h / 2);
  M5.Display.drawString("-", minus_button.x + minus_button.w / 2, minus_button.y + minus_button.h / 2);

  M5.Display.fillRoundRect(x, 152, 52, 26, 8, 0x18E3);
  M5.Display.drawRoundRect(x, 152, 52, 26, 8, WHITE);
  M5.Display.setTextSize(2);
  char buffer[3];
  snprintf(buffer, sizeof(buffer), "%02u", value);
  M5.Display.drawString(buffer, x + 26, 165);
}

void drawTimerDetail(const AppState &state) {
  if (state.selected_timer < 0 || state.selected_timer >= kTimerCount) return;

  const TimerState &timer = state.timers[state.selected_timer];
  M5.Display.fillScreen(BLACK);
  drawHeader(("TIMER " + String(state.selected_timer + 1)).c_str(), true);

  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString(formatTimerValue(timer).c_str(), 160, 52);

  drawWideValueRow("HH", String(timer.hours), kTimerHoursMinusButton, kTimerHoursPlusButton, 94);
  drawWideValueRow("MM", String(timer.minutes), kTimerMinutesMinusButton, kTimerMinutesPlusButton, 130);
  drawWideValueRow("SS", String(timer.seconds), kTimerSecondsMinusButton, kTimerSecondsPlusButton, 166);
}

void drawScreen(const AppState &state) {
  switch (state.screen) {
    case ScreenId::Home:
      drawHome();
      break;
    case ScreenId::Pump:
      drawPumpList(state);
      break;
    case ScreenId::PumpDetail:
      drawPumpDetail(state);
      break;
    case ScreenId::Valve:
      drawValveList(state);
      break;
    case ScreenId::ValveDetail:
      drawValveDetail(state);
      break;
    case ScreenId::Timer:
      drawTimerList(state);
      break;
    case ScreenId::TimerDetail:
      drawTimerDetail(state);
      break;
    case ScreenId::Work:
      drawWorkList(state);
      break;
    case ScreenId::WorkProgram:
      drawWorkProgram(state);
      break;
    case ScreenId::WorkStep:
      drawWorkStep(state);
      break;
    case ScreenId::WorkRun:
      drawWorkRun(state);
      break;
  }
}

void beep() {
  M5.Speaker.tone(1800, 30);
}

void dosingDoneBeep() {
  M5.Speaker.tone(1200, 80);
  delay(90);
  M5.Speaker.tone(1700, 120);
  delay(130);
  M5.Speaker.tone(2400, 180);
}

void workDoneBeep() {
  M5.Speaker.tone(1000, 90);
  delay(110);
  M5.Speaker.tone(1500, 90);
  delay(110);
  M5.Speaker.tone(2200, 160);
}

void valveDoneBeep() {
  M5.Speaker.tone(1500, 60);
  delay(70);
  M5.Speaker.tone(2200, 90);
}

void workStepBeep() {
  M5.Speaker.tone(1300, 40);
  delay(50);
  M5.Speaker.tone(1900, 60);
}

void startValveMotion(ValveState &valve, ValveMotion motion, uint32_t duration_ms) {
  valve.moving = true;
  valve.motion = motion;
  valve.motion_started_ms = millis();
  valve.motion_duration_ms = duration_ms;
}

void handleHomeTap(const lgfx::v1::touch_point_t &p, AppState &state) {
  if (contains(kPumpTile, p)) state.screen = ScreenId::Pump;
  else if (contains(kValveTile, p)) state.screen = ScreenId::Valve;
  else if (contains(kTimerTile, p)) state.screen = ScreenId::Timer;
  else if (contains(kWorkTile, p)) state.screen = ScreenId::Work;
  else return;

  beep();
  state.needs_redraw = true;
}

void handlePumpListTap(const lgfx::v1::touch_point_t &p, AppState &state) {
  if (contains(kBackButton, p)) {
    state.screen = ScreenId::Home;
    state.needs_redraw = true;
    beep();
    return;
  }

  for (uint8_t i = 0; i < kPumpCount; ++i) {
    if (!contains(pumpCardRect(i), p)) continue;
    state.selected_pump = i;
    state.screen = ScreenId::PumpDetail;
    state.needs_redraw = true;
    beep();
    return;
  }
}

void handleValveListTap(const lgfx::v1::touch_point_t &p, AppState &state) {
  if (contains(kBackButton, p)) {
    state.screen = ScreenId::Home;
    state.needs_redraw = true;
    beep();
    return;
  }

  for (uint8_t i = 0; i < kValveCount; ++i) {
    if (!contains(valveCardRect(i), p)) continue;
    state.selected_valve = i;
    state.screen = ScreenId::ValveDetail;
    state.needs_redraw = true;
    beep();
    return;
  }
}

void handleTimerListTap(const lgfx::v1::touch_point_t &p, AppState &state) {
  if (contains(kBackButton, p)) {
    state.screen = ScreenId::Home;
    state.needs_redraw = true;
    beep();
    return;
  }

  for (uint8_t i = 0; i < kTimerCount; ++i) {
    if (!contains(timerCardRect(i), p)) continue;
    state.selected_timer = i;
    state.screen = ScreenId::TimerDetail;
    state.needs_redraw = true;
    beep();
    return;
  }
}

void handlePumpDetailTap(const lgfx::v1::touch_point_t &p, AppState &state) {
  if (state.selected_pump < 0 || state.selected_pump >= kPumpCount) return;
  const uint8_t pump_index = static_cast<uint8_t>(state.selected_pump);
  PumpState &pump = state.pumps[state.selected_pump];
  bool config_changed = false;

  if (contains(kBackButton, p)) {
    if (pump.running) serial_link::stopPumpDose(pump_index);
    stopPump(pump);
    state.screen = ScreenId::Pump;
  } else if (contains(kFlowMinusButton, p)) {
    pump.flow_ml_hour = clampValue(pump.flow_ml_hour - 6.0f, 9.6f, 3000.0f);
    config_changed = true;
  } else if (contains(kFlowPlusButton, p)) {
    pump.flow_ml_hour = clampValue(pump.flow_ml_hour + 6.0f, 9.6f, 3000.0f);
    config_changed = true;
  } else if (contains(kVolumeMinusButton, p)) {
    pump.target_volume_ml = clampValue(pump.target_volume_ml - 1.0f, 1.0f, 10000.0f);
    pump.pumped_volume_ml = clampValue(pump.pumped_volume_ml, 0.0f, pump.target_volume_ml);
    config_changed = true;
  } else if (contains(kVolumePlusButton, p)) {
    pump.target_volume_ml = clampValue(pump.target_volume_ml + 1.0f, 1.0f, 10000.0f);
    config_changed = true;
  } else if (contains(kStartStopButton, p)) {
    if (!pump.running && pump.pumped_volume_ml >= pump.target_volume_ml) pump.pumped_volume_ml = 0.0f;
    pump.running = !pump.running;
    if (pump.running) {
      const float remaining_dose_ml = max(pump.target_volume_ml - pump.pumped_volume_ml, 0.0f);
      serial_link::startPumpDose(pump_index, remaining_dose_ml, pump.flow_ml_hour);
    } else {
      serial_link::stopPumpDose(pump_index);
    }
  } else if (contains(kResetButton, p)) {
    if (pump.running) serial_link::stopPumpDose(pump_index);
    pump.pumped_volume_ml = 0.0f;
    stopPump(pump);
  } else if (contains(kPrimeButton, p)) {
    pump.pumped_volume_ml = clampValue(pump.pumped_volume_ml + 0.5f, 0.0f, pump.target_volume_ml);
  } else {
    return;
  }

  pump.last_redraw_ms = millis();
  pump.last_redraw_volume_ml = pump.pumped_volume_ml;
  if (config_changed) {
    pump.last_config_change_ms = millis();
    pump.config_dirty = true;
  }
  state.needs_redraw = true;
  beep();
}

void handleValveDetailTap(const lgfx::v1::touch_point_t &p, AppState &state) {
  if (state.selected_valve < 0 || state.selected_valve >= kValveCount) return;
  const uint8_t valve_index = static_cast<uint8_t>(state.selected_valve);
  ValveState &valve = state.valves[state.selected_valve];
  bool config_changed = false;

  if (contains(kBackButton, p)) {
    if (valve.moving) serial_link::stopValveMotion(valve_index);
    stopValve(valve);
    state.screen = ScreenId::Valve;
  } else if (contains(kValveInvertToggle, p)) {
    valve.invert_direction = !valve.invert_direction;
    config_changed = true;
  } else if (contains(kValveSpeedMinusButton, p)) {
    valve.speed_steps = clampValue(valve.speed_steps - 10.0f, 10.0f, 6000.0f);
    config_changed = true;
  } else if (contains(kValveSpeedPlusButton, p)) {
    valve.speed_steps = clampValue(valve.speed_steps + 10.0f, 10.0f, 6000.0f);
    config_changed = true;
  } else if (contains(kValveZeroMinusButton, p)) {
    valve.zero_offset_steps = static_cast<uint16_t>(clampValue(valve.zero_offset_steps - 10.0f, 0.0f, 20000.0f));
    config_changed = true;
  } else if (contains(kValveZeroPlusButton, p)) {
    valve.zero_offset_steps = static_cast<uint16_t>(clampValue(valve.zero_offset_steps + 10.0f, 0.0f, 20000.0f));
    config_changed = true;
  } else if (contains(kValveRatioMinusButton, p)) {
    valve.ratio = clampValue(valve.ratio - 0.1f, 1.0f, 100.0f);
    config_changed = true;
  } else if (contains(kValveRatioPlusButton, p)) {
    valve.ratio = clampValue(valve.ratio + 0.1f, 1.0f, 100.0f);
    config_changed = true;
  } else if (contains(kValveLeftButton, p) && !valve.moving) {
    if (valve.position_index > 0) {
      serial_link::startValveStep(valve_index, false, valve.ratio, valve.speed_steps, valve.invert_direction);
      startValveMotion(valve, ValveMotion::StepLeft, valveStepDurationMs(valve));
    }
  } else if (contains(kValveRightButton, p) && !valve.moving) {
    if (valve.position_index < 4) {
      serial_link::startValveStep(valve_index, true, valve.ratio, valve.speed_steps, valve.invert_direction);
      startValveMotion(valve, ValveMotion::StepRight, valveStepDurationMs(valve));
    }
  } else if (contains(kValveZeroButton, p)) {
    if (valve.motion == ValveMotion::Zeroing) {
      serial_link::stopValveMotion(valve_index);
      stopValve(valve);
    } else if (!valve.moving) {
      serial_link::zeroValve(valve_index, valve.zero_offset_steps, valve.speed_steps, valve.invert_direction);
      startValveMotion(valve, ValveMotion::Zeroing, valveZeroDurationMs(valve));
    }
  } else {
    return;
  }

  if (config_changed) {
    valve.last_config_change_ms = millis();
    valve.config_dirty = true;
  }
  state.needs_redraw = true;
  beep();
}

void handleTimerDetailTap(const lgfx::v1::touch_point_t &p, AppState &state) {
  if (state.selected_timer < 0 || state.selected_timer >= kTimerCount) return;
  TimerState &timer = state.timers[state.selected_timer];
  bool config_changed = false;

  if (contains(kBackButton, p)) {
    state.screen = ScreenId::Timer;
  } else if (contains(kTimerHoursMinusButton, p)) {
    if (timer.hours > 0) --timer.hours;
    config_changed = true;
  } else if (contains(kTimerHoursPlusButton, p)) {
    if (timer.hours < 99) ++timer.hours;
    config_changed = true;
  } else if (contains(kTimerMinutesMinusButton, p)) {
    if (timer.minutes > 0) --timer.minutes;
    config_changed = true;
  } else if (contains(kTimerMinutesPlusButton, p)) {
    if (timer.minutes < 59) ++timer.minutes;
    config_changed = true;
  } else if (contains(kTimerSecondsMinusButton, p)) {
    if (timer.seconds > 0) --timer.seconds;
    config_changed = true;
  } else if (contains(kTimerSecondsPlusButton, p)) {
    if (timer.seconds < 59) ++timer.seconds;
    config_changed = true;
  } else {
    return;
  }

  if (config_changed) {
    timer.config_dirty = true;
    timer.last_config_change_ms = millis();
  }
  state.needs_redraw = true;
  beep();
}

void handleWorkListTap(const lgfx::v1::touch_point_t &p, AppState &state) {
  if (contains(kBackButton, p)) {
    state.screen = ScreenId::Home;
    state.needs_redraw = true;
    beep();
    return;
  }

  if (contains(kWorkPrevProgramsButton, p)) {
    if (state.work_program_page > 0) --state.work_program_page;
    state.needs_redraw = true;
    beep();
    return;
  }

  if (contains(kWorkNextProgramsButton, p)) {
    const uint8_t max_page = (kProgramCount - 1) / 6;
    if (state.work_program_page < max_page) ++state.work_program_page;
    state.needs_redraw = true;
    beep();
    return;
  }

  const uint8_t start_index = state.work_program_page * 6;
  for (uint8_t slot = 0; slot < 6; ++slot) {
    const uint8_t program_index = start_index + slot;
    if (program_index >= kProgramCount) break;
    if (!contains(programCardRect(slot), p)) continue;
    state.selected_program = program_index;
    state.work_step_page = 0;
    state.screen = ScreenId::WorkProgram;
    state.needs_redraw = true;
    beep();
    return;
  }
}

void handleWorkProgramTap(const lgfx::v1::touch_point_t &p, AppState &state) {
  if (state.selected_program < 0 || state.selected_program >= kProgramCount) return;

  if (contains(kBackButton, p)) {
    state.screen = ScreenId::Work;
    state.needs_redraw = true;
    beep();
    return;
  }

  if (contains(kWorkStepsPrevButton, p)) {
    if (state.work_step_page > 0) --state.work_step_page;
    state.needs_redraw = true;
    beep();
    return;
  }

  if (contains(kWorkStepsNextButton, p)) {
    if (state.work_step_page < 4) ++state.work_step_page;
    state.needs_redraw = true;
    beep();
    return;
  }

  if (contains(kWorkProgramEditRunButton, p)) {
    if (startWorkProgramRuntime(state, static_cast<uint8_t>(state.selected_program))) {
      state.screen = ScreenId::WorkRun;
      state.needs_redraw = true;
      workStepBeep();
    }
    return;
  }

  const uint8_t first_step = state.work_step_page * 5;
  for (uint8_t slot = 0; slot < 5; ++slot) {
    const uint8_t step_index = first_step + slot;
    if (step_index >= kProgramStepCount) break;
    if (!contains(workStepCardRect(slot), p)) continue;
    state.selected_work_step = step_index;
    state.screen = ScreenId::WorkStep;
    state.needs_redraw = true;
    beep();
    return;
  }
}

void handleWorkStepTap(const lgfx::v1::touch_point_t &p, AppState &state) {
  if (state.selected_program < 0 || state.selected_program >= kProgramCount) return;
  if (state.selected_work_step < 0 || state.selected_work_step >= kProgramStepCount) return;

  WorkStepState &step = state.programs[state.selected_program].steps[state.selected_work_step];
  bool changed = false;

  if (contains(kBackButton, p)) {
    state.screen = ScreenId::WorkProgram;
    state.needs_redraw = true;
    beep();
    return;
  }

  if (contains(kWorkTypeMinusButton, p) || contains(kWorkTypePlusButton, p)) {
    int type_value = static_cast<int>(step.type);
    type_value += contains(kWorkTypePlusButton, p) ? 1 : -1;
    if (type_value < 0) type_value = static_cast<int>(WorkStepType::Timer);
    if (type_value > static_cast<int>(WorkStepType::Timer)) type_value = 0;
    step.type = static_cast<WorkStepType>(type_value);
    step.target_index = 0;
    step.aux_value = 0;
    changed = true;
  } else if (contains(kWorkTargetMinusButton, p) || contains(kWorkTargetPlusButton, p)) {
    const int delta = contains(kWorkTargetPlusButton, p) ? 1 : -1;
    if (step.type == WorkStepType::Pump) {
      step.target_index = static_cast<uint8_t>((static_cast<int>(step.target_index) + delta + kPumpCount) % kPumpCount);
      changed = true;
    } else if (step.type == WorkStepType::Valve) {
      step.target_index = static_cast<uint8_t>((static_cast<int>(step.target_index) + delta + kValveCount) % kValveCount);
      changed = true;
    } else if (step.type == WorkStepType::Timer) {
      step.target_index = static_cast<uint8_t>((static_cast<int>(step.target_index) + delta + kTimerCount) % kTimerCount);
      changed = true;
    }
  } else if (contains(kWorkAuxMinusButton, p) || contains(kWorkAuxPlusButton, p)) {
    const int delta = contains(kWorkAuxPlusButton, p) ? 1 : -1;
    if (step.type == WorkStepType::Valve) {
      step.aux_value = static_cast<uint8_t>((static_cast<int>(step.aux_value) + delta + 6) % 6);
      changed = true;
    }
  } else if (contains(kWorkClearStepButton, p)) {
    step.type = WorkStepType::None;
    step.target_index = 0;
    step.aux_value = 0;
    changed = true;
  } else {
    return;
  }

  if (changed) {
    storage::saveWorkProgram(state, static_cast<uint8_t>(state.selected_program));
    state.work_config_dirty = false;
    state.last_work_config_change_ms = 0;
    state.needs_redraw = true;
    beep();
  }
}

void handleWorkRunTap(const lgfx::v1::touch_point_t &p, AppState &state) {
  if (contains(kBackButton, p) || contains(kWorkRunStopButton, p)) {
    stopWorkRuntime(state);
    state.screen = ScreenId::WorkProgram;
    state.needs_redraw = true;
    beep();
    return;
  }

  if (contains(kWorkRunPauseButton, p)) {
    if (state.work_runtime.completed) return;
    if (state.work_runtime.paused)
      resumeWorkRuntime(state);
    else
      pauseWorkRuntime(state);
    state.needs_redraw = true;
    beep();
    return;
  }

  if (contains(kWorkRunSkipButton, p)) {
    stopRuntimeMotion(state);
    state.work_runtime.paused = false;
    state.work_runtime.completed = false;
    state.work_runtime.resume_pending = false;
    state.work_runtime.paused_remaining_ms = 0;
    state.work_runtime.step_started = false;
    ++state.work_runtime.step_index;
    state.needs_redraw = true;
    workStepBeep();
    return;
  }

  WorkRuntimeState &runtime = state.work_runtime;
  if (!runtime.running || runtime.paused || runtime.completed || runtime.program_index >= kProgramCount ||
      runtime.step_index >= kProgramStepCount)
    return;

  WorkStepState &step = state.programs[runtime.program_index].steps[runtime.step_index];
  const uint32_t now = millis();

  if (step.type == WorkStepType::Pump && step.target_index < kPumpCount) {
    PumpState &pump = state.pumps[step.target_index];
    bool changed = false;

    if (contains(kWorkRunPrimaryMinusButton, p)) {
      pump.flow_ml_hour = clampValue(pump.flow_ml_hour - 6.0f, 9.6f, 3000.0f);
      changed = true;
    } else if (contains(kWorkRunPrimaryPlusButton, p)) {
      pump.flow_ml_hour = clampValue(pump.flow_ml_hour + 6.0f, 9.6f, 3000.0f);
      changed = true;
    } else if (contains(kWorkRunSecondaryMinusButton, p)) {
      pump.target_volume_ml = clampValue(pump.target_volume_ml - 1.0f, max(1.0f, pump.pumped_volume_ml), 10000.0f);
      changed = true;
    } else if (contains(kWorkRunSecondaryPlusButton, p)) {
      pump.target_volume_ml = clampValue(pump.target_volume_ml + 1.0f, 1.0f, 10000.0f);
      changed = true;
    }

    if (!changed) return;

    applyActivePumpConfigChange(state, step.target_index, now);

    state.needs_redraw = true;
    beep();
    return;
  }

  if (step.type == WorkStepType::Timer && step.target_index < kTimerCount) {
    TimerState &timer = state.timers[step.target_index];
    int delta_seconds = 0;

    if (contains(kWorkRunPrimaryMinusButton, p)) delta_seconds = -5;
    else if (contains(kWorkRunPrimaryPlusButton, p)) delta_seconds = 5;
    else return;

    const uint32_t current_total = timer.hours * 3600UL + timer.minutes * 60UL + timer.seconds;
    int32_t updated_total = static_cast<int32_t>(current_total) + delta_seconds;
    if (updated_total < 1) updated_total = 1;
    setTimerFromSeconds(timer, static_cast<uint32_t>(updated_total));
    timer.config_dirty = true;
    timer.last_config_change_ms = now;

    const uint32_t elapsed = runtime.step_started ? now - runtime.step_started_ms : 0;
    int32_t current_remaining = static_cast<int32_t>(runtime.step_duration_ms > elapsed ? runtime.step_duration_ms - elapsed : 0);
    current_remaining += delta_seconds * 1000;
    if (current_remaining < 1000) current_remaining = 1000;
    runtime.step_started_ms = now;
    runtime.step_duration_ms = static_cast<uint32_t>(current_remaining);

    state.needs_redraw = true;
    beep();
    return;
  }

  if (step.type == WorkStepType::Valve && step.target_index < kValveCount) {
    int delta = 0;
    if (contains(kWorkRunPrimaryMinusButton, p)) delta = -1;
    else if (contains(kWorkRunPrimaryPlusButton, p)) delta = 1;
    else return;

    int target = static_cast<int>(step.aux_value) + delta;
    if (target < 0) target = 5;
    if (target > 5) target = 0;
    step.aux_value = static_cast<uint8_t>(target);
    storage::saveWorkProgram(state, runtime.program_index);
    state.work_config_dirty = false;
    state.last_work_config_change_ms = 0;

    stopRuntimeMotion(state);
    runtime.step_started = false;
    state.needs_redraw = true;
    beep();
  }
}

void handlePlaceholderTap(const lgfx::v1::touch_point_t &p, AppState &state) {
  if (!contains(kBackButton, p)) return;
  state.screen = ScreenId::Home;
  state.needs_redraw = true;
  beep();
}

void updatePumpAnimation(AppState &state) {
  const uint32_t now = millis();
  static uint32_t last_update_ms = now;
  const uint32_t elapsed_ms = now - last_update_ms;
  last_update_ms = now;
  if (elapsed_ms == 0) return;

  for (uint8_t i = 0; i < kPumpCount; ++i) {
    PumpState &pump = state.pumps[i];
    if (!pump.running) continue;

    const float delta_ml = pump.flow_ml_hour * (static_cast<float>(elapsed_ms) / 3600000.0f);
    if (delta_ml <= 0.0f) continue;

    pump.pumped_volume_ml += delta_ml;
    if (pump.pumped_volume_ml >= pump.target_volume_ml) {
      pump.pumped_volume_ml = pump.target_volume_ml;
      stopPump(pump);
      pump.last_redraw_ms = now;
      pump.last_redraw_volume_ml = pump.pumped_volume_ml;
      dosingDoneBeep();
      if (state.screen == ScreenId::Pump || state.screen == ScreenId::PumpDetail) state.needs_redraw = true;
      continue;
    }

    const bool volume_threshold_reached = (pump.pumped_volume_ml - pump.last_redraw_volume_ml) >= 0.5f;
    const bool time_threshold_reached = (now - pump.last_redraw_ms) >= 3000;

    if ((volume_threshold_reached || time_threshold_reached) &&
        (state.screen == ScreenId::Pump || state.screen == ScreenId::PumpDetail)) {
      pump.last_redraw_ms = now;
      pump.last_redraw_volume_ml = pump.pumped_volume_ml;
      state.needs_redraw = true;
    }
  }
}

void updateValveAnimation(AppState &state) {
  uint8_t valve_index = 0;
  serial_link::ValveAction action = serial_link::ValveAction::None;
  if (!serial_link::takeValveCompletion(valve_index, action)) return;
  if (valve_index >= kValveCount) return;

  ValveState &valve = state.valves[valve_index];
  if (action == serial_link::ValveAction::StepLeft && valve.position_index > 0) --valve.position_index;
  else if (action == serial_link::ValveAction::StepRight && valve.position_index < 4) ++valve.position_index;
  else if (action == serial_link::ValveAction::Zero) valve.position_index = 0;

  stopValve(valve);
  valveDoneBeep();
  if (state.screen == ScreenId::Valve || state.screen == ScreenId::ValveDetail) state.needs_redraw = true;
}

void updateWorkRuntime(AppState &state) {
  WorkRuntimeState &runtime = state.work_runtime;
  if (!runtime.running || runtime.program_index >= kProgramCount || runtime.completed) return;
  if (runtime.paused) return;

  WorkProgramState &program = state.programs[runtime.program_index];
  while (runtime.step_index < kProgramStepCount && program.steps[runtime.step_index].type == WorkStepType::None)
    ++runtime.step_index;

  if (runtime.step_index >= kProgramStepCount) {
    stopRuntimeMotion(state);
    runtime.running = true;
    runtime.step_started = false;
    runtime.pump_chunk_active = false;
    runtime.paused = false;
    runtime.completed = true;
    runtime.resume_pending = false;
    runtime.paused_remaining_ms = 0;
    workDoneBeep();
    state.needs_redraw = true;
    return;
  }

  WorkStepState &step = program.steps[runtime.step_index];
  const uint32_t now = millis();

  static uint32_t last_work_run_refresh_ms = 0;
  if (state.screen == ScreenId::WorkRun && runtime.step_started && (now - last_work_run_refresh_ms) >= 1000UL) {
    last_work_run_refresh_ms = now;
    state.needs_redraw = true;
  }

  if (!runtime.step_started) {
    const bool resume_pending = runtime.resume_pending;
    const uint32_t resume_remaining_ms = runtime.paused_remaining_ms;
    runtime.resume_pending = false;
    runtime.step_started = true;
    runtime.step_started_ms = now;
    runtime.step_duration_ms = 0;
    runtime.paused_remaining_ms = 0;
    workStepBeep();

    if (step.type == WorkStepType::Pump && step.target_index < kPumpCount) {
      PumpState &pump = state.pumps[step.target_index];
      if (!resume_pending) {
        pump.pumped_volume_ml = 0.0f;
        pump.last_redraw_volume_ml = 0.0f;
        runtime.pump_remaining_ml = pump.target_volume_ml;
      } else {
        runtime.pump_remaining_ml = max(pump.target_volume_ml - pump.pumped_volume_ml, 0.0f);
      }
      pump.last_redraw_ms = now;
      runtime.pump_chunk_active = false;
      runtime.pump_chunk_end_ms = 0;
      runtime.step_duration_ms = static_cast<uint32_t>(
          clampValue((runtime.pump_remaining_ml / max(pump.flow_ml_hour / 60.0f, 0.01f)) * 60000.0f, 500.0f,
                     86400000.0f));
      startWorkPumpChunk(state, step.target_index, now);
    } else if (step.type == WorkStepType::Valve && step.target_index < kValveCount) {
      ValveState &valve = state.valves[step.target_index];
      const uint8_t target_position = min(step.aux_value, kValvePositionCount);
      if (target_position == 0) {
        serial_link::zeroValve(step.target_index, valve.zero_offset_steps, valve.speed_steps, valve.invert_direction);
        startValveMotion(valve, ValveMotion::Zeroing, valveZeroDurationMs(valve));
        runtime.step_duration_ms = valveZeroDurationMs(valve);
      } else if (valve.position_index == (target_position - 1)) {
        runtime.step_started = false;
        ++runtime.step_index;
      } else {
        const bool move_right = valve.position_index < (target_position - 1);
        serial_link::startValveStep(step.target_index, move_right, valve.ratio, valve.speed_steps, valve.invert_direction);
        startValveMotion(valve, move_right ? ValveMotion::StepRight : ValveMotion::StepLeft, valveStepDurationMs(valve));
        runtime.step_duration_ms = valveStepDurationMs(valve);
      }
    } else if (step.type == WorkStepType::Timer && step.target_index < kTimerCount) {
      runtime.step_duration_ms =
          resume_pending && resume_remaining_ms > 0 ? resume_remaining_ms : timerDurationMs(state.timers[step.target_index]);
    } else {
      runtime.step_started = false;
      ++runtime.step_index;
    }

    state.needs_redraw = true;
    return;
  }

  if (step.type == WorkStepType::Pump && step.target_index < kPumpCount) {
    PumpState &pump = state.pumps[step.target_index];

    if (runtime.pump_chunk_active && now >= runtime.pump_chunk_end_ms) {
      runtime.pump_chunk_active = false;
      stopPump(pump);
    }

    if (!runtime.pump_chunk_active) {
      runtime.pump_remaining_ml = max(pump.target_volume_ml - pump.pumped_volume_ml, 0.0f);
      if (runtime.pump_remaining_ml <= 0.001f) {
        runtime.step_started = false;
        ++runtime.step_index;
      } else {
        startWorkPumpChunk(state, step.target_index, now);
      }
      state.needs_redraw = true;
    }
  } else if (step.type == WorkStepType::Valve && step.target_index < kValveCount) {
    ValveState &valve = state.valves[step.target_index];
    const uint8_t target_position = min(step.aux_value, kValvePositionCount);
    if (!valve.moving) {
      if ((target_position == 0 && valve.position_index == 0) ||
          (target_position > 0 && valve.position_index == (target_position - 1))) {
        runtime.step_started = false;
        ++runtime.step_index;
      } else {
        runtime.step_started = false;
      }
      state.needs_redraw = true;
    }
  } else if (step.type == WorkStepType::Timer && step.target_index < kTimerCount) {
    if ((now - runtime.step_started_ms) >= runtime.step_duration_ms) {
      runtime.step_started = false;
      ++runtime.step_index;
      dosingDoneBeep();
      state.needs_redraw = true;
    } else if ((now - runtime.step_started_ms) % 250 < 15) {
      if (state.screen == ScreenId::WorkRun) state.needs_redraw = true;
    }
  }
}

void persistPendingPumpConfig(AppState &state) {
  const uint32_t now = millis();

  for (uint8_t i = 0; i < kPumpCount; ++i) {
    PumpState &pump = state.pumps[i];
    if (!pump.config_dirty) continue;
    if ((now - pump.last_config_change_ms) < 3000) continue;
    storage::savePumpConfig(state, i);
    pump.config_dirty = false;
  }
}

void persistPendingValveConfig(AppState &state) {
  const uint32_t now = millis();

  for (uint8_t i = 0; i < kValveCount; ++i) {
    ValveState &valve = state.valves[i];
    if (!valve.config_dirty) continue;
    if ((now - valve.last_config_change_ms) < 3000) continue;
    storage::saveValveConfig(state, i);
    valve.config_dirty = false;
  }
}

void persistPendingTimerConfig(AppState &state) {
  const uint32_t now = millis();

  for (uint8_t i = 0; i < kTimerCount; ++i) {
    TimerState &timer = state.timers[i];
    if (!timer.config_dirty) continue;
    if ((now - timer.last_config_change_ms) < 3000) continue;
    storage::saveTimerConfig(state, i);
    timer.config_dirty = false;
  }
}

void persistPendingWorkConfig(AppState &state) {
  const uint32_t now = millis();
  if (!state.work_config_dirty) return;
  if ((now - state.last_work_config_change_ms) < 3000) return;
  storage::saveWorkPrograms(state);
  state.work_config_dirty = false;
}

}  // namespace

namespace ui {

void init() {
  M5.Display.setRotation(1);
  M5.Display.setFont(&fonts::Font2);
}

void tick(AppState &state) {
  static ScreenId last_drawn_screen = ScreenId::Home;
  static bool has_drawn_screen = false;

  updatePumpAnimation(state);
  updateValveAnimation(state);
  updateWorkRuntime(state);
  persistPendingPumpConfig(state);
  persistPendingValveConfig(state);
  persistPendingTimerConfig(state);
  persistPendingWorkConfig(state);

  if (state.needs_redraw) {
    if (state.screen == ScreenId::WorkRun && has_drawn_screen && last_drawn_screen == ScreenId::WorkRun)
      drawWorkRunDynamic(state);
    else
      drawScreen(state);
    last_drawn_screen = state.screen;
    has_drawn_screen = true;
    state.needs_redraw = false;
  }

  if (!M5.Touch.getCount()) return;

  auto p = M5.Touch.getDetail();
  if (!p.wasPressed()) return;

  switch (state.screen) {
    case ScreenId::Home:
      handleHomeTap(p, state);
      break;
    case ScreenId::Pump:
      handlePumpListTap(p, state);
      break;
    case ScreenId::PumpDetail:
      handlePumpDetailTap(p, state);
      break;
    case ScreenId::Valve:
      handleValveListTap(p, state);
      break;
    case ScreenId::ValveDetail:
      handleValveDetailTap(p, state);
      break;
    case ScreenId::Timer:
      handleTimerListTap(p, state);
      break;
    case ScreenId::TimerDetail:
      handleTimerDetailTap(p, state);
      break;
    case ScreenId::Work:
      handleWorkListTap(p, state);
      break;
    case ScreenId::WorkProgram:
      handleWorkProgramTap(p, state);
      break;
    case ScreenId::WorkStep:
      handleWorkStepTap(p, state);
      break;
    case ScreenId::WorkRun:
      handleWorkRunTap(p, state);
      break;
  }
}

bool startWorkProgram(AppState &state, uint8_t program_index) {
  const bool started = startWorkProgramRuntime(state, program_index);
  if (started) state.needs_redraw = true;
  return started;
}

void stopWorkProgram(AppState &state) {
  stopWorkRuntime(state);
  state.needs_redraw = true;
}

void pauseWorkProgram(AppState &state) {
  pauseWorkRuntime(state);
  state.needs_redraw = true;
}

void resumeWorkProgram(AppState &state) {
  resumeWorkRuntime(state);
  state.needs_redraw = true;
}

void skipWorkStep(AppState &state) {
  if (!state.work_runtime.running || state.work_runtime.completed) return;
  stopRuntimeMotion(state);
  state.work_runtime.paused = false;
  state.work_runtime.completed = false;
  state.work_runtime.resume_pending = false;
  state.work_runtime.paused_remaining_ms = 0;
  state.work_runtime.step_started = false;
  ++state.work_runtime.step_index;
  state.needs_redraw = true;
}

void updatePumpConfig(AppState &state, uint8_t pump_index, float flow_ml_hour, float dose_ml) {
  if (pump_index >= kPumpCount) return;
  PumpState &pump = state.pumps[pump_index];
  pump.flow_ml_hour = clampValue(flow_ml_hour, 9.6f, 3000.0f);
  pump.target_volume_ml = clampValue(dose_ml, 1.0f, 10000.0f);
  pump.pumped_volume_ml = clampValue(pump.pumped_volume_ml, 0.0f, pump.target_volume_ml);
  applyActivePumpConfigChange(state, pump_index, millis());
  state.needs_redraw = true;
}

void startManualPump(AppState &state, uint8_t pump_index) {
  if (pump_index >= kPumpCount) return;
  PumpState &pump = state.pumps[pump_index];
  if (pump.running) return;
  if (pump.pumped_volume_ml >= pump.target_volume_ml) pump.pumped_volume_ml = 0.0f;
  pump.running = true;
  const float remaining_dose_ml = max(pump.target_volume_ml - pump.pumped_volume_ml, 0.0f);
  serial_link::startPumpDose(pump_index, remaining_dose_ml, pump.flow_ml_hour);
  pump.last_redraw_ms = millis();
  pump.last_redraw_volume_ml = pump.pumped_volume_ml;
  state.needs_redraw = true;
}

void stopManualPump(AppState &state, uint8_t pump_index) {
  if (pump_index >= kPumpCount) return;
  serial_link::stopPumpDose(pump_index);
  stopPump(state.pumps[pump_index]);
  state.needs_redraw = true;
}

void updateValveConfig(AppState &state, uint8_t valve_index, float speed_steps, uint16_t zero_offset_steps, float ratio,
                       bool invert_direction) {
  if (valve_index >= kValveCount) return;
  ValveState &valve = state.valves[valve_index];
  valve.speed_steps = clampValue(speed_steps, 10.0f, 6000.0f);
  valve.zero_offset_steps = static_cast<uint16_t>(clampValue(zero_offset_steps, 0.0f, 20000.0f));
  valve.ratio = clampValue(ratio, 1.0f, 100.0f);
  valve.invert_direction = invert_direction;
  valve.config_dirty = true;
  valve.last_config_change_ms = millis();
  state.needs_redraw = true;
}

void commandManualValve(AppState &state, uint8_t valve_index, ManualValveAction action) {
  if (valve_index >= kValveCount) return;
  ValveState &valve = state.valves[valve_index];

  if (action == ManualValveAction::Stop) {
    serial_link::stopValveMotion(valve_index);
    stopValve(valve);
    state.needs_redraw = true;
    return;
  }

  if (valve.moving) return;

  if (action == ManualValveAction::Left) {
    if (valve.position_index == 0) return;
    serial_link::startValveStep(valve_index, false, valve.ratio, valve.speed_steps, valve.invert_direction);
    startValveMotion(valve, ValveMotion::StepLeft, valveStepDurationMs(valve));
  } else if (action == ManualValveAction::Right) {
    if (valve.position_index >= 4) return;
    serial_link::startValveStep(valve_index, true, valve.ratio, valve.speed_steps, valve.invert_direction);
    startValveMotion(valve, ValveMotion::StepRight, valveStepDurationMs(valve));
  } else if (action == ManualValveAction::Zero) {
    serial_link::zeroValve(valve_index, valve.zero_offset_steps, valve.speed_steps, valve.invert_direction);
    startValveMotion(valve, ValveMotion::Zeroing, valveZeroDurationMs(valve));
  }

  state.needs_redraw = true;
}

void updateTimerConfig(AppState &state, uint8_t timer_index, uint8_t hours, uint8_t minutes, uint8_t seconds) {
  if (timer_index >= kTimerCount) return;
  TimerState &timer = state.timers[timer_index];
  timer.hours = min<uint8_t>(hours, 99);
  timer.minutes = min<uint8_t>(minutes, 59);
  timer.seconds = min<uint8_t>(seconds, 59);
  timer.config_dirty = true;
  timer.last_config_change_ms = millis();
  state.needs_redraw = true;
}

}  // namespace ui
