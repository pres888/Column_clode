#include "serial_link.h"

#include <Arduino.h>

#include "app_types.h"

namespace {

HardwareSerial monster_serial(2);

constexpr int kMonsterBaudrate = 115200;
constexpr int kMonsterRxPin = 13;  // Core2 RXD2 / Port C G13
constexpr int kMonsterTxPin = 14;  // Core2 TXD2 / Port C G14
constexpr char kPumpAxes[] = {'Z', 'A', 'B', 'C', 'U', 'V'};
constexpr char kValveAxes[] = {'X', 'Y'};
float g_pump_units_per_ml = 87.04f;                 // Measured: 10 ml/min command -> 1 rev / 94 s, 0.18 ml/rev
constexpr float kPumpMinFlowMlMin = 0.16f;
constexpr float kPumpMaxFlowMlMin = 50.0f;
constexpr float kDefaultAxisMaxUnitsSec = 50.0f;    // Current Marlin DEFAULT_MAX_FEEDRATE
constexpr float kValveMotorFullStepsPerRev = 200.0f;
constexpr float kValveMicrosteps = 16.0f;
constexpr float kMarlinValveStepsPerUnit = 100.0f;

String rx_line;
bool intro_sent = false;
uint32_t boot_ms = 0;
int8_t pending_valve_index = -1;
serial_link::ValveAction pending_valve_action = serial_link::ValveAction::None;
uint8_t pending_ok_count = 0;
bool valve_completion_ready = false;
uint8_t completed_valve_index = 0;
serial_link::ValveAction completed_valve_action = serial_link::ValveAction::None;

void logLine(const String &line) {
  if (!line.length()) return;
  Serial.print("[MONSTER] ");
  Serial.println(line);
}

char pumpAxis(uint8_t pump_index) {
  if (pump_index >= sizeof(kPumpAxes)) return '?';
  return kPumpAxes[pump_index];
}

char valveAxis(uint8_t valve_index) {
  if (valve_index >= sizeof(kValveAxes)) return '?';
  return kValveAxes[valve_index];
}

float valvePositionUnits(float ratio) {
  const float microsteps_per_position = kValveMotorFullStepsPerRev * kValveMicrosteps * ratio * (72.0f / 360.0f) * kValveStepCalibration;
  return microsteps_per_position / kMarlinValveStepsPerUnit;
}

}  // namespace

namespace serial_link {

void init() {
  monster_serial.begin(kMonsterBaudrate, SERIAL_8N1, kMonsterRxPin, kMonsterTxPin);
  boot_ms = millis();
  rx_line.reserve(160);
  Serial.println("Serial link ready on UART2 TX=14 RX=13 @115200");
  Serial.printf("Pump calibration: %.3f axis units/ml\n", g_pump_units_per_ml);
}

void setPumpUnitsPerMl(float value) {
  g_pump_units_per_ml = constrain(value, 0.001f, 100000.0f);
  Serial.printf("Pump calibration updated: %.3f axis units/ml\n", g_pump_units_per_ml);
}

float getPumpUnitsPerMl() {
  return g_pump_units_per_ml;
}

void tick() {
  while (monster_serial.available() > 0) {
    const char ch = static_cast<char>(monster_serial.read());
    if (ch == '\r') continue;
    if (ch == '\n') {
      logLine(rx_line);
      if (rx_line == "ok" && pending_ok_count > 0) {
        --pending_ok_count;
        if (pending_ok_count == 0 && pending_valve_index >= 0) {
          valve_completion_ready = true;
          completed_valve_index = pending_valve_index;
          completed_valve_action = pending_valve_action;
          pending_valve_index = -1;
          pending_valve_action = ValveAction::None;
        }
      }
      rx_line = "";
      continue;
    }
    if (rx_line.length() < 159) rx_line += ch;
  }

  if (!intro_sent && millis() - boot_ms > 1500) {
    sendCommand("M115");
    sendCommand("M114");
    intro_sent = true;
  }
}

void sendCommand(const char *command) {
  if (!command || !*command) return;
  monster_serial.print(command);
  monster_serial.print('\n');
  Serial.print("[SEND] ");
  Serial.println(command);
}

void startPumpDose(uint8_t pump_index, float dose_ml, float flow_ml_hour) {
  const char axis = pumpAxis(pump_index);
  if (axis == '?') return;

  const float feedrate_ml_min = constrain(flow_ml_hour / 60.0f, kPumpMinFlowMlMin, kPumpMaxFlowMlMin);
  const float distance_units = max(dose_ml, 0.0f) * g_pump_units_per_ml;
  const float feedrate_units_min = feedrate_ml_min * g_pump_units_per_ml;
  const float max_axis_units_sec = max(kDefaultAxisMaxUnitsSec, (feedrate_units_min / 60.0f) * 1.2f);

  char command[64];

  snprintf(command, sizeof(command), "M203 %c%.3f", axis, max_axis_units_sec);
  sendCommand(command);
  snprintf(command, sizeof(command), "M17 %c", axis);
  sendCommand(command);
  sendCommand("G91");
  snprintf(command, sizeof(command), "G0 %c%.3f F%.3f", axis, distance_units, feedrate_units_min);
  sendCommand(command);
  Serial.printf("[PUMP%u] axis=%c dose=%.3f ml flow=%.3f ml/min -> %.3f units, F%.3f\n", pump_index + 1, axis,
                max(dose_ml, 0.0f), feedrate_ml_min, distance_units, feedrate_units_min);
}

void stopPumpDose(uint8_t pump_index) {
  const char axis = pumpAxis(pump_index);
  if (axis == '?') return;

  char command[24];
  sendCommand("M410");
  sendCommand("M400");
  snprintf(command, sizeof(command), "M18 %c", axis);
  sendCommand(command);
}

void startValveStep(uint8_t valve_index, bool move_right, float ratio, float speed_steps_min, bool invert_direction) {
  const char axis = valveAxis(valve_index);
  if (axis == '?') return;

  const float direction = (move_right ? 1.0f : -1.0f) * (invert_direction ? -1.0f : 1.0f);
  const float distance_units = valvePositionUnits(ratio) * direction;
  const float feedrate_units_min = constrain(speed_steps_min, 10.0f, 6000.0f);
  const float max_axis_units_sec = max(kDefaultAxisMaxUnitsSec, (feedrate_units_min / 60.0f) * 1.2f);
  char command[64];

  snprintf(command, sizeof(command), "M203 %c%.3f", axis, max_axis_units_sec);
  sendCommand(command);
  snprintf(command, sizeof(command), "M17 %c", axis);
  sendCommand(command);
  sendCommand("G91");
  snprintf(command, sizeof(command), "G0 %c%.3f F%.3f", axis, distance_units, feedrate_units_min);
  sendCommand(command);
  sendCommand("M400");
  pending_valve_index = valve_index;
  pending_valve_action = move_right ? ValveAction::StepRight : ValveAction::StepLeft;
  pending_ok_count = 5;

  Serial.printf("[VALVE%u] axis=%c %s ratio=%.2f speed=%.1f -> %.3f units\n", valve_index + 1, axis,
                move_right ? "RIGHT" : "LEFT", ratio, feedrate_units_min, distance_units);
}

void zeroValve(uint8_t valve_index, uint16_t zero_offset_steps, float speed_steps_min, bool invert_direction) {
  const char axis = valveAxis(valve_index);
  if (axis == '?') return;

  // After homing to the MIN endstop, always move away from the switch.
  const float offset_units = static_cast<float>(zero_offset_steps) / kMarlinValveStepsPerUnit;
  const float feedrate_units_min = constrain(speed_steps_min, 10.0f, 6000.0f);
  const float max_axis_units_sec = max(kDefaultAxisMaxUnitsSec, (feedrate_units_min / 60.0f) * 1.2f);
  char command[64];

  snprintf(command, sizeof(command), "M203 %c%.3f", axis, max_axis_units_sec);
  sendCommand(command);
  snprintf(command, sizeof(command), "M17 %c", axis);
  sendCommand(command);
  snprintf(command, sizeof(command), "G28 %c", axis);
  sendCommand(command);

  pending_ok_count = 3;
  if (zero_offset_steps > 0) {
    sendCommand("G91");
    snprintf(command, sizeof(command), "G0 %c%.3f F%.3f", axis, offset_units, feedrate_units_min);
    sendCommand(command);
    pending_ok_count += 2;
  }
  sendCommand("M400");
  ++pending_ok_count;
  pending_valve_index = valve_index;
  pending_valve_action = ValveAction::Zero;

  Serial.printf("[VALVE%u] axis=%c ZERO offset=%u steps -> %.3f units\n", valve_index + 1, axis, zero_offset_steps,
                offset_units);
}

void stopValveMotion(uint8_t valve_index) {
  const char axis = valveAxis(valve_index);
  if (axis == '?') return;

  char command[24];
  sendCommand("M410");
  sendCommand("M400");
  snprintf(command, sizeof(command), "M18 %c", axis);
  sendCommand(command);
  pending_valve_index = -1;
  pending_valve_action = ValveAction::None;
  pending_ok_count = 0;
}

bool takeValveCompletion(uint8_t &valve_index, ValveAction &action) {
  if (!valve_completion_ready) return false;
  valve_index = completed_valve_index;
  action = completed_valve_action;
  valve_completion_ready = false;
  completed_valve_action = ValveAction::None;
  return true;
}

}  // namespace serial_link
