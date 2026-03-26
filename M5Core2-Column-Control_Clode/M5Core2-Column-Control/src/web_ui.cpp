#include "web_ui.h"

#include <WebServer.h>
#include <WiFi.h>

#include "serial_link.h"
#include "storage.h"
#include "ui.h"

namespace {

WebServer server(80);
AppState *g_state = nullptr;
bool g_ap_mode = false;
String g_ap_ssid;
String g_ap_password;

const char *screenLabel(ScreenId screen) {
  switch (screen) {
    case ScreenId::Home: return "HOME";
    case ScreenId::Pump: return "PUMP LIST";
    case ScreenId::PumpDetail: return "PUMP DETAIL";
    case ScreenId::Valve: return "VALVE LIST";
    case ScreenId::ValveDetail: return "VALVE DETAIL";
    case ScreenId::Timer: return "TIMER LIST";
    case ScreenId::TimerDetail: return "TIMER DETAIL";
    case ScreenId::Work: return "WORK LIST";
    case ScreenId::WorkProgram: return "PROGRAM EDIT";
    case ScreenId::WorkStep: return "STEP EDIT";
    case ScreenId::WorkRun: return "WORK RUN";
    default: return "UNKNOWN";
  }
}

String workStateLabel(const AppState &state) {
  const WorkRuntimeState &runtime = state.work_runtime;
  if (!runtime.running) return "IDLE";
  if (runtime.completed) return "COMPLETE";
  if (runtime.paused) return "PAUSED";
  return "RUNNING";
}

uint8_t workProgramStepCount(const WorkProgramState &program) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < kProgramStepCount; ++i) {
    if (program.steps[i].type != WorkStepType::None) count = i + 1;
  }
  return count;
}

String jsonBool(bool value) {
  return value ? "true" : "false";
}

String htmlEscape(const String &input) {
  String out;
  out.reserve(input.length() + 16);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}

String jsonEscape(const String &input) {
  String out;
  out.reserve(input.length() + 16);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

String programLabel(const AppState &state, uint8_t program_index) {
  String label = "P" + String(program_index + 1);
  if (program_index < kProgramCount && state.program_names[program_index][0] != '\0')
    label += " - " + String(state.program_names[program_index]);
  return label;
}

String programOptionsHtml(const AppState &state, uint8_t selected_index) {
  String html;
  html.reserve(2200);
  for (uint8_t i = 0; i < kProgramCount; ++i) {
    html += "<option value=\"" + String(i + 1) + "\"";
    if (i == selected_index) html += " selected";
    html += ">" + htmlEscape(programLabel(state, i)) + "</option>";
  }
  return html;
}

const char *workStepTypeToken(WorkStepType type) {
  switch (type) {
    case WorkStepType::Pump: return "pump";
    case WorkStepType::Valve: return "valve";
    case WorkStepType::Timer: return "timer";
    case WorkStepType::None:
    default: return "none";
  }
}

String stepOptionsHtml(uint8_t selected_index) {
  String html;
  html.reserve(1100);
  for (uint8_t i = 0; i < kProgramStepCount; ++i) {
    html += "<option value=\"" + String(i + 1) + "\"";
    if (i == selected_index) html += " selected";
    html += ">Step " + String(i + 1) + "</option>";
  }
  return html;
}

String stepTypeOptionsHtml(WorkStepType selected_type) {
  String html;
  const struct {
    WorkStepType type;
    const char *label;
  } options[] = {
      {WorkStepType::None, "None"},
      {WorkStepType::Pump, "Pump"},
      {WorkStepType::Valve, "Valve"},
      {WorkStepType::Timer, "Timer"},
  };

  for (const auto &option : options) {
    html += "<option value=\"" + String(workStepTypeToken(option.type)) + "\"";
    if (option.type == selected_type) html += " selected";
    html += ">" + String(option.label) + "</option>";
  }
  return html;
}

String targetOptionsHtml(WorkStepType type, uint8_t selected_value) {
  String html;
  uint8_t count = 1;
  const char *prefix = "Not used";

  if (type == WorkStepType::Pump) {
    count = kPumpCount;
    prefix = "Pump ";
  } else if (type == WorkStepType::Valve) {
    count = kValveCount;
    prefix = "Valve ";
  } else if (type == WorkStepType::Timer) {
    count = kTimerCount;
    prefix = "Timer ";
  }

  for (uint8_t i = 0; i < count; ++i) {
    html += "<option value=\"" + String(i + 1) + "\"";
    if (i == selected_value) html += " selected";
    html += ">";
    if (type == WorkStepType::None) html += prefix;
    else html += String(prefix) + String(i + 1);
    html += "</option>";
  }
  return html;
}

String auxOptionsHtml(WorkStepType type, uint8_t selected_value) {
  String html;
  if (type == WorkStepType::Valve) {
    html += "<option value=\"0\"";
    if (selected_value == 0) html += " selected";
    html += ">ZERO</option>";
    for (uint8_t i = 1; i <= 5; ++i) {
      html += "<option value=\"" + String(i) + "\"";
      if (selected_value == i) html += " selected";
      html += ">Position " + String(i) + "</option>";
    }
    return html;
  }

  html += "<option value=\"0\" selected>Not used</option>";
  return html;
}

String currentIpString() {
  return g_ap_mode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

String networkModeString() {
  return g_ap_mode ? "AP MODE" : "WIFI CLIENT";
}

bool isAsyncRequest() {
  return server.hasArg("_async");
}

void completeRequest(const char *redirect_location) {
  if (isAsyncRequest()) {
    server.send(200, "application/json; charset=utf-8", "{\"ok\":true}");
    return;
  }
  server.sendHeader("Location", redirect_location, true);
  server.send(303, "text/plain", "");
}

String workStepTypeLabel(WorkStepType type) {
  switch (type) {
    case WorkStepType::Pump: return "PUMP";
    case WorkStepType::Valve: return "VALVE";
    case WorkStepType::Timer: return "TIMER";
    case WorkStepType::None:
    default: return "NONE";
  }
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

uint32_t currentStepRemainingMs(const AppState &state) {
  const WorkRuntimeState &runtime = state.work_runtime;
  if (!runtime.running || runtime.completed || !runtime.step_started) return 0;
  if (runtime.paused) return runtime.paused_remaining_ms;

  const uint32_t elapsed = millis() - runtime.step_started_ms;
  return runtime.step_duration_ms > elapsed ? runtime.step_duration_ms - elapsed : 0;
}

uint32_t currentStepElapsedMs(const AppState &state) {
  const WorkRuntimeState &runtime = state.work_runtime;
  if (!runtime.running || runtime.completed || !runtime.step_started) return 0;
  if (runtime.paused) return runtime.step_duration_ms > runtime.paused_remaining_ms
                           ? runtime.step_duration_ms - runtime.paused_remaining_ms
                           : 0;
  return millis() - runtime.step_started_ms;
}

uint32_t programRemainingMs(const AppState &state) {
  const WorkRuntimeState &runtime = state.work_runtime;
  if (!runtime.running || runtime.program_index >= kProgramCount || runtime.completed) return 0;

  const WorkProgramState &program = state.programs[runtime.program_index];
  uint32_t total = 0;
  for (uint8_t i = runtime.step_index; i < kProgramStepCount; ++i) {
    const WorkStepState &step = program.steps[i];
    if (step.type == WorkStepType::None) continue;

    if (i == runtime.step_index && runtime.step_started) {
      total += currentStepRemainingMs(state);
      continue;
    }

    if (step.type == WorkStepType::Pump && step.target_index < kPumpCount) {
      const PumpState &pump = state.pumps[step.target_index];
      total += static_cast<uint32_t>((pump.target_volume_ml / max(pump.flow_ml_hour / 60.0f, 0.01f)) * 60000.0f);
    } else if (step.type == WorkStepType::Timer && step.target_index < kTimerCount) {
      const TimerState &timer = state.timers[step.target_index];
      total += (static_cast<uint32_t>(timer.hours) * 3600UL + static_cast<uint32_t>(timer.minutes) * 60UL +
                static_cast<uint32_t>(timer.seconds)) *
               1000UL;
    } else if (step.type == WorkStepType::Valve && step.target_index < kValveCount) {
      const ValveState &valve = state.valves[step.target_index];
      const float position_units = 6.4f * valve.ratio * kValveStepCalibration;
      const float speed = max(valve.speed_steps, 10.0f);
      if (step.aux_value == 0) {
        // Distance to travel back to zero: current position steps + offset
        const float dist = (valve.position_index * position_units) +
                           (static_cast<float>(valve.zero_offset_steps) / 100.0f);
        total += static_cast<uint32_t>(max(dist, position_units) / speed * 60000.0f);
      } else {
        // One position step worth of travel
        total += static_cast<uint32_t>(position_units / speed * 60000.0f);
      }
    }
  }
  return total;
}

String activeEquipmentLabel(const AppState &state) {
  const WorkRuntimeState &runtime = state.work_runtime;
  if (!runtime.running || runtime.program_index >= kProgramCount || runtime.step_index >= kProgramStepCount) return "-";
  const WorkStepState &step = state.programs[runtime.program_index].steps[runtime.step_index];
  if (step.type == WorkStepType::Pump) return "PUMP P" + String(step.target_index + 1);
  if (step.type == WorkStepType::Valve) return "VALVE V" + String(step.target_index + 1);
  if (step.type == WorkStepType::Timer) return "TIMER T" + String(step.target_index + 1);
  return "-";
}

String activeEquipmentState(const AppState &state) {
  const WorkRuntimeState &runtime = state.work_runtime;
  if (!runtime.running) return "IDLE";
  if (runtime.completed) return "COMPLETE";
  if (runtime.paused) return "PAUSED";
  if (runtime.program_index >= kProgramCount || runtime.step_index >= kProgramStepCount) return "IDLE";

  const WorkStepState &step = state.programs[runtime.program_index].steps[runtime.step_index];
  if (step.type == WorkStepType::Pump && step.target_index < kPumpCount)
    return state.pumps[step.target_index].running ? "DOSING" : "WAIT";
  if (step.type == WorkStepType::Valve && step.target_index < kValveCount)
    return state.valves[step.target_index].moving ? "MOVING" : "WAIT";
  if (step.type == WorkStepType::Timer) return "TIMING";
  return "IDLE";
}

String buildStatusJson() {
  if (!g_state) return "{}";

  const AppState &state = *g_state;
  String detail_a = "-";
  String detail_b = "-";
  String detail_c = "-";
  String active_type = "NONE";

  if (state.work_runtime.program_index < kProgramCount && state.work_runtime.step_index < kProgramStepCount) {
    const WorkStepState &active_step = state.programs[state.work_runtime.program_index].steps[state.work_runtime.step_index];
    active_type = workStepTypeLabel(active_step.type);

    if (active_step.type == WorkStepType::Pump && active_step.target_index < kPumpCount) {
      const PumpState &pump = state.pumps[active_step.target_index];
      detail_a = "Flow " + String(pump.flow_ml_hour / 60.0f, 1) + " ml/min";
      detail_b = "Dose " + String(pump.target_volume_ml, 1) + " ml";
      detail_c = "Done " + String(pump.pumped_volume_ml, 1) + " ml";
    } else if (active_step.type == WorkStepType::Valve && active_step.target_index < kValveCount) {
      const ValveState &valve = state.valves[active_step.target_index];
      detail_a = "Target " + String(active_step.aux_value == 0 ? "ZERO" : ("POS " + String(active_step.aux_value)));
      detail_b = "Current " + String(valve.position_index + 1);
      detail_c = "F " + String(valve.speed_steps, 0);
    } else if (active_step.type == WorkStepType::Timer && active_step.target_index < kTimerCount) {
      const TimerState &timer = state.timers[active_step.target_index];
      char buffer[9];
      snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", timer.hours, timer.minutes, timer.seconds);
      detail_a = "Set " + String(buffer);
      detail_b = "Elapsed " + formatDurationMs(currentStepElapsedMs(state));
      detail_c = "Left " + formatDurationMs(currentStepRemainingMs(state));
    }
  }

  String json;
  json.reserve(4096);

  json += "{";
  json += "\"screen\":\"" + String(screenLabel(state.screen)) + "\",";
  json += "\"operator_mode\":\"" + String(state.operator_manual_mode ? "MANUAL" : "PROGRAM") + "\",";
  json += "\"network\":{";
  json += "\"mode\":\"" + networkModeString() + "\",";
  json += "\"ip\":\"" + currentIpString() + "\",";
  json += "\"ap_ssid\":\"" + jsonEscape(g_ap_ssid) + "\"";
  json += "},";
  json += "\"work\":{";
  json += "\"state\":\"" + workStateLabel(state) + "\",";
  json += "\"program\":" + String(state.work_runtime.program_index + 1) + ",";
  json += "\"step\":" + String(state.work_runtime.step_index + 1) + ",";
  json += "\"step_total\":" +
          String(state.work_runtime.program_index < kProgramCount
                     ? workProgramStepCount(state.programs[state.work_runtime.program_index])
                     : 0) +
          ",";
  json += "\"steps_done\":" +
          String(state.work_runtime.completed
                     ? (state.work_runtime.program_index < kProgramCount
                            ? workProgramStepCount(state.programs[state.work_runtime.program_index])
                            : 0)
                     : min<uint8_t>(state.work_runtime.step_index, kProgramStepCount)) +
          ",";
  json += "\"active_type\":\"" + active_type + "\",";
  json += "\"equipment\":\"" + activeEquipmentLabel(state) + "\",";
  json += "\"equipment_state\":\"" + activeEquipmentState(state) + "\",";
  json += "\"step_elapsed\":\"" + formatDurationMs(currentStepElapsedMs(state)) + "\",";
  json += "\"step_remaining\":\"" + formatDurationMs(currentStepRemainingMs(state)) + "\",";
  json += "\"program_remaining\":\"" + formatDurationMs(programRemainingMs(state)) + "\",";
  json += "\"detail_a\":\"" + detail_a + "\",";
  json += "\"detail_b\":\"" + detail_b + "\",";
  json += "\"detail_c\":\"" + detail_c + "\"";
  json += "},";

  json += "\"pumps\":[";
  for (uint8_t i = 0; i < kPumpCount; ++i) {
    if (i) json += ",";
    const PumpState &pump = state.pumps[i];
    json += "{";
    json += "\"id\":" + String(i + 1) + ",";
    json += "\"flow\":" + String(pump.flow_ml_hour / 60.0f, 1) + ",";
    json += "\"dose\":" + String(pump.target_volume_ml, 1) + ",";
    json += "\"pumped\":" + String(pump.pumped_volume_ml, 1) + ",";
    json += "\"running\":" + jsonBool(pump.running);
    json += "}";
  }
  json += "],";

  json += "\"valves\":[";
  for (uint8_t i = 0; i < kValveCount; ++i) {
    if (i) json += ",";
    const ValveState &valve = state.valves[i];
    json += "{";
    json += "\"id\":" + String(i + 1) + ",";
    json += "\"position\":" + String(valve.position_index + 1) + ",";
    json += "\"speed\":" + String(valve.speed_steps, 0) + ",";
    json += "\"zero\":" + String(valve.zero_offset_steps) + ",";
    json += "\"ratio\":" + String(valve.ratio, 1) + ",";
    json += "\"rev\":" + jsonBool(valve.invert_direction) + ",";
    json += "\"moving\":" + jsonBool(valve.moving);
    json += "}";
  }
  json += "],";

  json += "\"timers\":[";
  for (uint8_t i = 0; i < kTimerCount; ++i) {
    if (i) json += ",";
    const TimerState &timer = state.timers[i];
    char buffer[9];
    snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", timer.hours, timer.minutes, timer.seconds);
    json += "{";
    json += "\"id\":" + String(i + 1) + ",";
    json += "\"value\":\"" + String(buffer) + "\"";
    json += "}";
  }
  json += "],";

  json += "\"programs\":[";
  for (uint8_t i = 0; i < kProgramCount; ++i) {
    if (i) json += ",";
    json += "{";
    json += "\"id\":" + String(i + 1) + ",";
    json += "\"steps\":" + String(workProgramStepCount(state.programs[i])) + ",";
    json += "\"name\":\"" + jsonEscape(String(state.program_names[i])) + "\"";
    json += "}";
  }
  json += "],";

  json += "\"system\":{";
  json += "\"pump_units_per_ml\":" + String(state.system.pump_units_per_ml, 3);
  json += "}";
  json += "}";

  return json;
}

String buildPage() {
  if (!g_state) return "<html><body>No state</body></html>";

  const AppState &state = *g_state;
  const String ssid_value = htmlEscape(String(state.system.wifi_ssid));
  const String pump_k = String(state.system.pump_units_per_ml, 3);
  const uint8_t selected_program =
      state.selected_program >= 0 && state.selected_program < kProgramCount ? static_cast<uint8_t>(state.selected_program) : 0;
  const String program_options = programOptionsHtml(state, selected_program);
  uint8_t editor_program = selected_program;
  uint8_t editor_step =
      state.selected_work_step >= 0 && state.selected_work_step < kProgramStepCount ? static_cast<uint8_t>(state.selected_work_step) : 0;
  if (server.hasArg("program")) {
    const int requested = server.arg("program").toInt();
    if (requested >= 1 && requested <= kProgramCount) editor_program = static_cast<uint8_t>(requested - 1);
  }
  if (server.hasArg("step")) {
    const int requested = server.arg("step").toInt();
    if (requested >= 1 && requested <= kProgramStepCount) editor_step = static_cast<uint8_t>(requested - 1);
  }
  const WorkStepState &editor_step_state = state.programs[editor_program].steps[editor_step];
  const String editor_program_options = programOptionsHtml(state, editor_program);
  const String step_options = stepOptionsHtml(editor_step);
  const String step_type_options = stepTypeOptionsHtml(editor_step_state.type);
  const String target_options = targetOptionsHtml(editor_step_state.type, editor_step_state.target_index);
  const String aux_options = auxOptionsHtml(editor_step_state.type, editor_step_state.aux_value);

  String html;
  html.reserve(12000);
  html += F(R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Column Control</title>
  <style>
    :root {
      --bg: #09131a;
      --panel: rgba(15, 30, 39, 0.92);
      --panel-2: rgba(21, 42, 54, 0.92);
      --line: rgba(122, 184, 219, 0.18);
      --text: #eaf4f8;
      --muted: #8fa8b5;
      --accent: #31b8d8;
      --accent-2: #0d6d8d;
      --good: #33c36b;
      --warn: #f7b955;
      --danger: #ee6a5f;
      --shadow: 0 18px 40px rgba(0, 0, 0, 0.28);
    }
    * { box-sizing: border-box; }
    html, body { margin: 0; padding: 0; }
    body {
      font-family: "IBM Plex Sans", "Segoe UI", sans-serif;
      color: var(--text);
      background:
        radial-gradient(circle at top left, rgba(49,184,216,0.18), transparent 28%),
        linear-gradient(180deg, #081118 0%, #0a171f 100%);
      min-height: 100vh;
    }
    body::before {
      content: "";
      position: fixed;
      inset: 0;
      pointer-events: none;
      background-image:
        linear-gradient(rgba(255,255,255,0.03) 1px, transparent 1px),
        linear-gradient(90deg, rgba(255,255,255,0.03) 1px, transparent 1px);
      background-size: 28px 28px;
      mask-image: linear-gradient(180deg, rgba(0,0,0,0.55), transparent 90%);
    }
    .shell {
      width: min(1240px, calc(100% - 32px));
      margin: 18px auto 28px;
    }
    .hero {
      display: grid;
      grid-template-columns: 1.4fr 1fr;
      gap: 16px;
      margin-bottom: 16px;
    }
    .hero-card, .panel {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 18px;
      box-shadow: var(--shadow);
      backdrop-filter: blur(10px);
    }
    .hero-card {
      padding: 22px 24px;
    }
    .eyebrow {
      color: var(--accent);
      text-transform: uppercase;
      letter-spacing: 0.18em;
      font-size: 11px;
      margin-bottom: 10px;
    }
    h1 {
      margin: 0 0 10px;
      font-size: 34px;
      line-height: 1.05;
      font-weight: 700;
    }
    .sub {
      margin: 0;
      color: var(--muted);
      line-height: 1.55;
      max-width: 50ch;
    }
    .status-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 12px;
    }
    .status-card {
      background: var(--panel-2);
      border: 1px solid var(--line);
      border-radius: 14px;
      padding: 14px 16px;
    }
    .status-label {
      color: var(--muted);
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.12em;
      margin-bottom: 8px;
    }
    .status-value {
      font-size: 20px;
      font-weight: 700;
    }
    .nav {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      margin: 0 0 16px;
    }
    .nav a {
      text-decoration: none;
      color: var(--text);
      padding: 10px 14px;
      border-radius: 999px;
      background: rgba(49,184,216,0.1);
      border: 1px solid rgba(49,184,216,0.18);
      font-size: 13px;
      letter-spacing: 0.05em;
      text-transform: uppercase;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(12, minmax(0, 1fr));
      gap: 16px;
    }
    .col-12 { grid-column: span 12; }
    .col-8 { grid-column: span 8; }
    .col-6 { grid-column: span 6; }
    .col-4 { grid-column: span 4; }
    .panel {
      padding: 18px;
    }
    .panel-head {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 12px;
      margin-bottom: 14px;
    }
    .panel h2 {
      margin: 0;
      font-size: 18px;
      letter-spacing: 0.04em;
      text-transform: uppercase;
    }
    .badge {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      font-size: 12px;
      color: var(--muted);
      padding: 8px 10px;
      border-radius: 999px;
      background: rgba(255,255,255,0.04);
      border: 1px solid var(--line);
    }
    table {
      width: 100%;
      border-collapse: collapse;
      font-size: 14px;
    }
    th, td {
      text-align: left;
      padding: 11px 10px;
      border-bottom: 1px solid rgba(255,255,255,0.06);
    }
    th {
      color: var(--muted);
      font-weight: 600;
      text-transform: uppercase;
      font-size: 11px;
      letter-spacing: 0.12em;
    }
    .state-pill {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      padding: 6px 10px;
      border-radius: 999px;
      font-size: 12px;
      min-width: 76px;
      font-weight: 700;
      letter-spacing: 0.05em;
    }
    .run { background: rgba(51,195,107,0.14); color: #8ef0af; }
    .idle { background: rgba(255,255,255,0.08); color: #d0dce2; }
    .warn { background: rgba(247,185,85,0.14); color: #ffd388; }
    .danger { background: rgba(238,106,95,0.14); color: #ff9b92; }
    form {
      display: grid;
      gap: 14px;
    }
    .form-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 14px;
    }
    label {
      display: grid;
      gap: 8px;
      color: var(--muted);
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.1em;
    }
    input, select {
      width: 100%;
      border: 1px solid rgba(255,255,255,0.08);
      background: rgba(255,255,255,0.05);
      color: var(--text);
      border-radius: 12px;
      padding: 13px 14px;
      font-size: 15px;
      outline: none;
    }
    input:focus, select:focus {
      border-color: rgba(49,184,216,0.55);
      box-shadow: 0 0 0 3px rgba(49,184,216,0.14);
    }
    .btn {
      appearance: none;
      border: 0;
      border-radius: 12px;
      padding: 13px 18px;
      background: linear-gradient(135deg, #1390af 0%, #31b8d8 100%);
      color: #051219;
      font-size: 14px;
      font-weight: 700;
      letter-spacing: 0.06em;
      text-transform: uppercase;
      cursor: pointer;
      width: fit-content;
    }
    .hint {
      color: var(--muted);
      font-size: 13px;
      line-height: 1.5;
      margin: 0;
    }
    .control-bar {
      display: grid;
      grid-template-columns: 130px repeat(5, minmax(0, 1fr));
      gap: 10px;
      align-items: end;
      margin-top: 16px;
    }
    .control-bar input {
      padding: 12px 14px;
      height: 46px;
    }
    .btn.alt {
      background: rgba(255,255,255,0.06);
      color: var(--text);
      border: 1px solid var(--line);
    }
    .btn.warn {
      background: linear-gradient(135deg, #d69a2f 0%, #f7c96e 100%);
      color: #261805;
    }
    .btn.danger {
      background: linear-gradient(135deg, #c44c42 0%, #ee6a5f 100%);
      color: #1b0808;
    }
    .metric-grid {
      display: grid;
      grid-template-columns: repeat(4, minmax(0, 1fr));
      gap: 12px;
      margin-top: 16px;
    }
    .metric-card {
      padding: 14px 16px;
      border-radius: 14px;
      background: var(--panel-2);
      border: 1px solid var(--line);
    }
    .metric-card .status-label {
      margin-bottom: 6px;
    }
    .metric-card .status-value {
      font-size: 18px;
    }
    .detail-stack {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 12px;
      margin-top: 12px;
    }
    .pump-editor-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 14px;
    }
    .editor-card {
      padding: 14px;
      border-radius: 14px;
      background: var(--panel-2);
      border: 1px solid var(--line);
    }
    .editor-card h3 {
      margin: 0 0 12px;
      font-size: 15px;
      letter-spacing: 0.06em;
      text-transform: uppercase;
    }
    .editor-card form {
      gap: 10px;
    }
    .editor-card .btn {
      width: 100%;
      justify-content: center;
    }
    .program-name-form {
      grid-template-columns: 260px minmax(0, 1fr) 180px;
      align-items: end;
      margin-bottom: 14px;
    }
    .program-step-form {
      display: grid;
      grid-template-columns: 220px 180px 180px 180px 180px;
      gap: 14px;
      align-items: end;
      margin-bottom: 14px;
    }
    .program-step-actions {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
      align-self: end;
    }
    @media (max-width: 980px) {
      .hero, .grid, .form-grid { grid-template-columns: 1fr; }
      .control-bar, .metric-grid, .detail-stack, .pump-editor-grid, .program-name-form, .program-step-form, .program-step-actions { grid-template-columns: 1fr; }
      .col-8, .col-6, .col-4, .col-12 { grid-column: auto; }
    }
  </style>
</head>
<body>
  <div class="shell">
    <section class="hero">
      <article class="hero-card">
        <div class="eyebrow">Engineering Dashboard</div>
        <h1>Column Control Web UI</h1>
        <p class="sub">Operational control stays on the M5 panel. Service settings, calibration values and engineering review move here, where numeric input is faster and clearer from a keyboard.</p>
      </article>
      <article class="hero-card">
        <div class="status-grid">
          <div class="status-card">
            <div class="status-label">Network Mode</div>
            <div class="status-value" id="network-mode">--</div>
          </div>
          <div class="status-card">
            <div class="status-label">IP Address</div>
            <div class="status-value" id="network-ip">--</div>
          </div>
          <div class="status-card">
            <div class="status-label">Screen</div>
            <div class="status-value" id="screen-name">--</div>
          </div>
          <div class="status-card">
            <div class="status-label">Work State</div>
            <div class="status-value" id="work-state">--</div>
          </div>
        </div>
      </article>
    </section>

    <nav class="nav">
      <a href="/process">Process</a>
      <a href="#overview">Overview</a>
      <a href="#pumps">Pumps</a>
      <a href="#valves">Valves</a>
      <a href="#timers">Timers</a>
      <a href="#programs">Programs</a>
      <a href="#system">System</a>
    </nav>

    <section class="grid">
      <article class="panel col-12" id="overview">
        <div class="panel-head">
          <h2>Overview</h2>
          <div class="badge">Live refresh: 1 s</div>
        </div>
        <table>
          <thead>
            <tr><th>Active Program</th><th>Active Step</th><th>Screen</th><th>Mode</th></tr>
          </thead>
          <tbody>
            <tr>
              <td id="overview-program">--</td>
              <td id="overview-step">--</td>
              <td id="overview-screen">--</td>
              <td id="overview-mode">--</td>
            </tr>
          </tbody>
        </table>
        <div class="metric-grid">
          <div class="metric-card">
            <div class="status-label">Equipment</div>
            <div class="status-value" id="active-equipment">--</div>
          </div>
          <div class="metric-card">
            <div class="status-label">Equipment State</div>
            <div class="status-value" id="active-equipment-state">--</div>
          </div>
          <div class="metric-card">
            <div class="status-label">Step Elapsed</div>
            <div class="status-value" id="active-step-elapsed">--</div>
          </div>
          <div class="metric-card">
            <div class="status-label">Step Remaining</div>
            <div class="status-value" id="active-step-left">--</div>
          </div>
        </div>
        <div class="metric-grid">
          <div class="metric-card">
            <div class="status-label">Program Remaining</div>
            <div class="status-value" id="program-left">--</div>
          </div>
          <div class="metric-card">
            <div class="status-label">Step Type</div>
            <div class="status-value" id="active-step-type">--</div>
          </div>
          <div class="metric-card">
            <div class="status-label">Steps Done</div>
            <div class="status-value" id="steps-done">--</div>
          </div>
          <div class="metric-card">
            <div class="status-label">Steps Total</div>
            <div class="status-value" id="steps-total">--</div>
          </div>
        </div>
        <div class="detail-stack">
          <div class="metric-card">
            <div class="status-label">Detail A</div>
            <div class="status-value" id="detail-a">--</div>
          </div>
          <div class="metric-card">
            <div class="status-label">Detail B</div>
            <div class="status-value" id="detail-b">--</div>
          </div>
          <div class="metric-card">
            <div class="status-label">Detail C</div>
            <div class="status-value" id="detail-c">--</div>
          </div>
        </div>
      </article>

      <article class="panel col-8" id="pumps">
        <div class="panel-head">
          <h2>Pumps</h2>
          <div class="badge">Flow in ml/min</div>
        </div>
        <table id="pumps-table"></table>
      </article>

      <article class="panel col-4" id="pump-config">
        <div class="panel-head">
          <h2>Pump Config</h2>
          <div class="badge">Keyboard input</div>
        </div>
        <div class="pump-editor-grid">
)rawliteral");
  for (uint8_t i = 0; i < kPumpCount; ++i) {
    const PumpState &pump = state.pumps[i];
    html += "<div class=\"editor-card\"><h3>PUMP " + String(i + 1) + "</h3>";
    html += "<form method=\"post\" action=\"/api/pump\">";
    html += "<input type=\"hidden\" name=\"pump\" value=\"" + String(i + 1) + "\">";
    html += "<input type=\"hidden\" name=\"return_to\" value=\"/dashboard#pump-config\">";
    html += "<label>Flow ml/min<input type=\"number\" step=\"0.1\" min=\"0.1\" name=\"flow\" value=\"" +
            String(pump.flow_ml_hour / 60.0f, 1) + "\"></label>";
    html += "<label>Dose ml<input type=\"number\" step=\"0.1\" min=\"1\" name=\"dose\" value=\"" +
            String(pump.target_volume_ml, 1) + "\"></label>";
    html += "<button class=\"btn\" type=\"submit\">Save Pump</button>";
    html += "</form></div>";
  }
  html += F(R"rawliteral(
        </div>
      </article>

      <article class="panel col-4" id="system">
        <div class="panel-head">
          <h2>System</h2>
          <div class="badge">Service settings</div>
        </div>
        <form method="post" action="/api/system">
          <label>
            Pump Units / ml
            <input type="number" step="0.001" min="0.001" name="pump_units_per_ml" value=")rawliteral");
  html += pump_k;
  html += F(R"rawliteral(">
          </label>
          <label>
            Wi‑Fi SSID
            <input type="text" maxlength="32" name="wifi_ssid" value=")rawliteral");
  html += ssid_value;
  html += F(R"rawliteral(">
          </label>
          <label>
            Wi‑Fi Password
            <input type="password" maxlength="64" name="wifi_password" placeholder="Leave empty to keep current">
          </label>
          <button class="btn" type="submit">Save System Settings</button>
          <p class="hint">If Wi‑Fi credentials change, the controller saves them immediately. Reboot afterwards to reconnect in station mode. If no credentials are stored, the controller starts its own setup access point.</p>
        </form>
      </article>

      <article class="panel col-6" id="valves">
        <div class="panel-head">
          <h2>Valves</h2>
          <div class="badge">Mechanical state</div>
        </div>
        <table id="valves-table"></table>
      </article>

      <article class="panel col-6" id="timers">
        <div class="panel-head">
          <h2>Timers</h2>
          <div class="badge">Stored runtime values</div>
        </div>
        <table id="timers-table"></table>
      </article>

      <article class="panel col-12" id="programs">
        <div class="panel-head">
          <h2>Programs</h2>
          <div class="badge">Names and step count</div>
        </div>
        <form method="post" action="/api/program/name" class="program-name-form">
          <label>
            Program
            <select name="program" id="program-name-select">
)rawliteral");
  html += program_options;
  html += F(R"rawliteral(
            </select>
          </label>
          <label>
            Program Name
            <input type="text" maxlength="20" name="name" id="program-name-input" placeholder="Up to 20 characters">
          </label>
          <button class="btn" type="submit">Save Name</button>
        </form>
        <form method="post" action="/api/program/step" class="program-step-form">
          <label>
            Program
            <select name="program" id="program-step-program">
)rawliteral");
  html += editor_program_options;
  html += F(R"rawliteral(
            </select>
          </label>
          <label>
            Step
            <select name="step" id="program-step-select">
)rawliteral");
  html += step_options;
  html += F(R"rawliteral(
            </select>
          </label>
          <label>
            Type
            <select name="type" id="program-step-type">
)rawliteral");
  html += step_type_options;
  html += F(R"rawliteral(
            </select>
          </label>
          <label>
            Target
            <select name="target" id="program-step-target">
)rawliteral");
  html += target_options;
  html += F(R"rawliteral(
            </select>
          </label>
          <label>
            Valve Position
            <select name="aux" id="program-step-aux">
)rawliteral");
  html += aux_options;
  html += F(R"rawliteral(
            </select>
          </label>
          <div class="program-step-actions">
            <button class="btn alt" type="submit" formmethod="get" formaction="/dashboard">Load Step</button>
            <button class="btn" type="submit">Save Step</button>
            <button class="btn alt" type="submit" name="clear" value="1">Clear Step</button>
          </div>
        </form>
        <table id="programs-table"></table>
      </article>
    </section>
  </div>

  <script>
    function pillClass(label) {
      if (label === 'RUNNING' || label === 'true') return 'state-pill run';
      if (label === 'PAUSED') return 'state-pill warn';
      if (label === 'COMPLETE') return 'state-pill good';
      if (label === 'MOVING') return 'state-pill warn';
      return 'state-pill idle';
    }

    function row(html) {
      return `<tr>${html}</tr>`;
    }

    function renderPumps(pumps) {
      const head = `<thead><tr><th>Pump</th><th>Flow</th><th>Dose</th><th>Pumped</th><th>State</th></tr></thead>`;
      const body = pumps.map(p => row(
        `<td>P${p.id}</td><td>${p.flow.toFixed(1)}</td><td>${p.dose.toFixed(1)}</td><td>${p.pumped.toFixed(1)}</td><td><span class="${pillClass(String(p.running))}">${p.running ? 'RUN' : 'IDLE'}</span></td>`
      )).join('');
      document.getElementById('pumps-table').innerHTML = head + `<tbody>${body}</tbody>`;
    }

    function renderValves(valves) {
      const head = `<thead><tr><th>Valve</th><th>Pos</th><th>F</th><th>Zero</th><th>Ratio</th><th>REV</th><th>State</th></tr></thead>`;
      const body = valves.map(v => row(
        `<td>V${v.id}</td><td>${v.position}</td><td>${v.speed}</td><td>${v.zero}</td><td>${v.ratio.toFixed(1)}:1</td><td>${v.rev ? 'ON' : 'OFF'}</td><td><span class="${pillClass(v.moving ? 'MOVING' : 'IDLE')}">${v.moving ? 'MOVING' : 'IDLE'}</span></td>`
      )).join('');
      document.getElementById('valves-table').innerHTML = head + `<tbody>${body}</tbody>`;
    }

    function renderTimers(timers) {
      const head = `<thead><tr><th>Timer</th><th>Value</th></tr></thead>`;
      const body = timers.map(t => row(`<td>T${t.id}</td><td>${t.value}</td>`)).join('');
      document.getElementById('timers-table').innerHTML = head + `<tbody>${body}</tbody>`;
    }

    function renderPrograms(programs) {
      const head = `<thead><tr><th>Program</th><th>Name</th><th>Steps</th></tr></thead>`;
      const body = programs.map(p => row(`<td>P${p.id}</td><td>${p.name || '—'}</td><td>${p.steps}</td>`)).join('');
      document.getElementById('programs-table').innerHTML = head + `<tbody>${body}</tbody>`;
    }

    function updateProgramNameEditor(programs) {
      const select = document.getElementById('program-name-select');
      const input = document.getElementById('program-name-input');
      if (!select || !input) return;
      const selectedId = Number(select.value || 1);
      const liveProgram = programs.find(p => p.id === selectedId);
      input.value = liveProgram ? (liveProgram.name || '') : '';
    }

    function optionsHtml(items, selectedValue) {
      return items.map(item => `<option value="${item.value}" ${String(item.value) === String(selectedValue) ? 'selected' : ''}>${item.label}</option>`).join('');
    }

    function targetOptions(type) {
      if (type === 'pump') return Array.from({ length: 6 }, (_, i) => ({ value: i + 1, label: `Pump ${i + 1}` }));
      if (type === 'valve') return Array.from({ length: 2 }, (_, i) => ({ value: i + 1, label: `Valve ${i + 1}` }));
      if (type === 'timer') return Array.from({ length: 6 }, (_, i) => ({ value: i + 1, label: `Timer ${i + 1}` }));
      return [{ value: 1, label: 'Not used' }];
    }

    function auxOptions(type) {
      if (type === 'valve') {
        return [
          { value: 0, label: 'ZERO' },
          { value: 1, label: 'Position 1' },
          { value: 2, label: 'Position 2' },
          { value: 3, label: 'Position 3' },
          { value: 4, label: 'Position 4' },
          { value: 5, label: 'Position 5' }
        ];
      }
      return [{ value: 0, label: 'Not used' }];
    }

    function onProgramTypeChange() {
      const type = document.getElementById('program-step-type').value;
      document.getElementById('program-step-target').innerHTML = optionsHtml(targetOptions(type), 1);
      document.getElementById('program-step-aux').innerHTML = optionsHtml(auxOptions(type), 0);
    }

    async function refresh() {
      const activeTag = document.activeElement ? document.activeElement.tagName : '';
      if (activeTag === 'INPUT' || activeTag === 'SELECT' || activeTag === 'TEXTAREA' || activeTag === 'BUTTON') return;
      const res = await fetch('/api/status', { cache: 'no-store' });
      const data = await res.json();

      document.getElementById('network-mode').textContent = data.network.mode;
      document.getElementById('network-ip').textContent = data.network.ip;
      document.getElementById('screen-name').textContent = data.screen;
      document.getElementById('work-state').textContent = data.work.state;
      document.getElementById('overview-program').textContent = `P${data.work.program}`;
      document.getElementById('overview-step').textContent = `STEP ${data.work.step}`;
      document.getElementById('overview-screen').textContent = data.screen;
      document.getElementById('overview-mode').textContent = data.network.mode;
      document.getElementById('active-equipment').textContent = data.work.equipment;
      document.getElementById('active-equipment-state').textContent = data.work.equipment_state;
      document.getElementById('active-step-elapsed').textContent = data.work.step_elapsed;
      document.getElementById('active-step-left').textContent = data.work.step_remaining;
      document.getElementById('program-left').textContent = data.work.program_remaining;
      document.getElementById('active-step-type').textContent = data.work.active_type;
      document.getElementById('steps-done').textContent = data.work.steps_done;
      document.getElementById('steps-total').textContent = data.work.step_total;
      document.getElementById('detail-a').textContent = data.work.detail_a;
      document.getElementById('detail-b').textContent = data.work.detail_b;
      document.getElementById('detail-c').textContent = data.work.detail_c;

      renderPumps(data.pumps);
      renderValves(data.valves);
      renderTimers(data.timers);
      renderPrograms(data.programs);
      if (activeTag !== 'INPUT') updateProgramNameEditor(data.programs);
    }

    document.getElementById('program-name-select').addEventListener('change', async () => {
      const res = await fetch('/api/status', { cache: 'no-store' });
      const data = await res.json();
      updateProgramNameEditor(data.programs);
    });
    document.getElementById('program-step-type').addEventListener('change', onProgramTypeChange);

    refresh();
    setInterval(refresh, 1000);
  </script>
</body>
</html>
)rawliteral");

  return html;
}

String buildProcessPage() {
  const AppState &state = *g_state;
  const uint8_t selected_program =
      state.work_runtime.running && state.work_runtime.program_index < kProgramCount
          ? state.work_runtime.program_index
          : (state.selected_program >= 0 && state.selected_program < kProgramCount ? static_cast<uint8_t>(state.selected_program) : 0);
  const String program_options = programOptionsHtml(state, selected_program);

  String html;
  html.reserve(18000);
  html += F(R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Process View</title>
  <style>
    :root {
      --bg: #0a1217;
      --panel: rgba(18, 29, 38, 0.94);
      --panel-2: rgba(28, 43, 54, 0.94);
      --line: rgba(122, 184, 219, 0.22);
      --text: #eaf4f8;
      --muted: #97aebb;
      --idle: #7a8790;
      --pump-active: #4fe06f;
      --valve-active: #38d26b;
      --valve-moving: #3cbef0;
      --timer-active: #f0c35a;
      --shadow: 0 20px 44px rgba(0,0,0,0.3);
    }
    * { box-sizing: border-box; }
    html, body { margin: 0; padding: 0; }
    body {
      font-family: "IBM Plex Sans", "Segoe UI", sans-serif;
      color: var(--text);
      min-height: 100vh;
      background:
        radial-gradient(circle at top left, rgba(60,190,240,0.18), transparent 24%),
        linear-gradient(180deg, #0b141a 0%, #0f1b23 100%);
    }
    .shell {
      width: min(1280px, calc(100% - 28px));
      margin: 16px auto 26px;
    }
    .frame {
      background: linear-gradient(180deg, rgba(24,36,46,0.96), rgba(16,25,33,0.96));
      border: 1px solid var(--line);
      border-radius: 20px;
      box-shadow: var(--shadow);
      padding: 14px;
    }
    .title-bar {
      display: grid;
      gap: 10px;
      margin-bottom: 12px;
    }
    .title-main {
      background: linear-gradient(180deg, #b7d7ea 0%, #95c4df 100%);
      color: #081219;
      border-radius: 12px;
      padding: 10px 18px;
      text-align: center;
      font-size: clamp(26px, 3vw, 44px);
      font-weight: 800;
      letter-spacing: 0.02em;
    }
    .title-sub {
      background: rgba(0,0,0,0.22);
      border: 1px solid rgba(255,255,255,0.05);
      border-radius: 10px;
      padding: 10px 18px;
      text-align: center;
      font-size: clamp(18px, 1.6vw, 28px);
      color: #f1f6f9;
    }
    .top-links {
      display: flex;
      justify-content: flex-end;
      gap: 10px;
      margin-bottom: 10px;
    }
    .top-links a {
      text-decoration: none;
      color: var(--text);
      border: 1px solid var(--line);
      background: rgba(255,255,255,0.05);
      padding: 9px 12px;
      border-radius: 999px;
      font-size: 13px;
      text-transform: uppercase;
      letter-spacing: 0.08em;
    }
    .mode-switch {
      display: flex;
      gap: 10px;
      margin: 6px 0 14px;
    }
    .mode-switch form {
      margin: 0;
    }
    .mode-btn {
      appearance: none;
      border: 1px solid var(--line);
      background: rgba(255,255,255,0.05);
      color: var(--text);
      border-radius: 999px;
      padding: 10px 16px;
      font-size: 13px;
      font-weight: 700;
      letter-spacing: 0.08em;
      text-transform: uppercase;
      cursor: pointer;
    }
    .mode-btn.active {
      background: linear-gradient(135deg, #1390af 0%, #31b8d8 100%);
      color: #051219;
      border-color: transparent;
    }
    .process-controls {
      display: grid;
      grid-template-columns: 120px repeat(5, minmax(0, 1fr));
      gap: 10px;
      margin: 10px 0 16px;
      align-items: end;
    }
    .process-controls label {
      display: grid;
      gap: 8px;
      color: var(--muted);
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.1em;
    }
    .process-controls input {
      width: 100%;
      border: 1px solid rgba(255,255,255,0.08);
      background: rgba(255,255,255,0.05);
      color: var(--text);
      border-radius: 12px;
      padding: 12px 14px;
      font-size: 15px;
      outline: none;
      height: 46px;
    }
    .process-btn {
      appearance: none;
      border: 0;
      border-radius: 12px;
      padding: 13px 18px;
      font-size: 14px;
      font-weight: 700;
      letter-spacing: 0.06em;
      text-transform: uppercase;
      cursor: pointer;
      width: 100%;
      height: 46px;
    }
    .process-btn.run {
      background: linear-gradient(135deg, #1493b2 0%, #31b8d8 100%);
      color: #051219;
    }
    .process-btn.stop {
      background: linear-gradient(135deg, #c44c42 0%, #ee6a5f 100%);
      color: #1b0808;
    }
    .process-btn.pause {
      background: linear-gradient(135deg, #d69a2f 0%, #f7c96e 100%);
      color: #261805;
    }
    .process-btn.ghost {
      background: rgba(255,255,255,0.06);
      color: var(--text);
      border: 1px solid var(--line);
    }
    .pump-grid {
      display: grid;
      grid-template-columns: repeat(6, minmax(0, 1fr));
      gap: 14px;
      margin: 14px 0 18px;
    }
    .pump-card {
      display: grid;
      gap: 10px;
      justify-items: center;
      color: var(--muted);
    }
    .pump-num {
      font-size: 18px;
      color: #dbe8ef;
    }
    .pump-visual {
      width: 122px;
      height: 122px;
      border-radius: 22px;
      border: 2px solid rgba(255,255,255,0.16);
      background:
        radial-gradient(circle at 50% 34%, rgba(255,255,255,0.22), transparent 18%),
        linear-gradient(180deg, #c9d1d7 0%, #99a3aa 100%);
      position: relative;
      box-shadow: inset 0 8px 12px rgba(255,255,255,0.18), inset 0 -14px 18px rgba(0,0,0,0.25);
      overflow: hidden;
    }
    .pump-visual::before {
      content: "";
      position: absolute;
      inset: 12px;
      border-radius: 50%;
      border: 6px solid rgba(0,0,0,0.36);
      background:
        radial-gradient(circle, rgba(255,255,255,0.26) 0 14%, rgba(0,0,0,0.18) 15% 27%, rgba(255,255,255,0.08) 28% 36%, transparent 37%),
        radial-gradient(circle at 36% 62%, rgba(255,255,255,0.18) 0 8%, rgba(0,0,0,0.2) 9% 18%, transparent 19%),
        radial-gradient(circle at 64% 62%, rgba(255,255,255,0.18) 0 8%, rgba(0,0,0,0.2) 9% 18%, transparent 19%),
        radial-gradient(circle at 50% 28%, rgba(255,255,255,0.18) 0 8%, rgba(0,0,0,0.2) 9% 18%, transparent 19%),
        radial-gradient(circle at 50% 50%, rgba(255,255,255,0.2), rgba(0,0,0,0.12));
    }
    .pump-visual::after {
      content: "";
      position: absolute;
      left: -6px;
      right: -6px;
      top: 50%;
      height: 10px;
      border-radius: 999px;
      transform: translateY(-50%);
      background: linear-gradient(90deg, rgba(255,255,255,0.22), rgba(255,255,255,0.82), rgba(255,255,255,0.22));
      opacity: 0.74;
    }
    .pump-card.active .pump-visual {
      border-color: rgba(79,224,111,0.7);
      box-shadow:
        inset 0 8px 12px rgba(255,255,255,0.22),
        inset 0 -14px 18px rgba(0,0,0,0.22),
        0 0 22px rgba(79,224,111,0.5);
      background:
        radial-gradient(circle at 50% 34%, rgba(255,255,255,0.22), transparent 18%),
        linear-gradient(180deg, #9ef2a8 0%, #46bf61 100%);
    }
    .pump-card .pump-meta {
      text-align: left;
      width: 122px;
      line-height: 1.25;
      font-size: 13px;
    }
    .pump-card .pump-meta strong {
      color: #edf6fa;
      font-weight: 600;
    }
    .equipment-row {
      display: grid;
      grid-template-columns: 1fr 1fr 0.72fr;
      gap: 18px;
      align-items: start;
      margin: 10px 0 16px;
    }
    .valve-panel, .timer-panel, .status-banner, .process-table {
      background: rgba(255,255,255,0.03);
      border: 1px solid rgba(255,255,255,0.08);
      border-radius: 16px;
      padding: 14px;
    }
    .valve-head, .timer-head {
      text-align: center;
      font-size: 15px;
      color: #d5e4ec;
      margin-bottom: 8px;
      text-transform: uppercase;
      letter-spacing: 0.08em;
    }
    .valve-wrap {
      position: relative;
      width: 240px;
      height: 240px;
      margin: 0 auto 8px;
    }
    .valve-core {
      position: absolute;
      inset: 38px;
      border-radius: 50%;
      background: linear-gradient(180deg, #dce4ea 0%, #aab4bb 100%);
      border: 2px solid rgba(255,255,255,0.16);
      box-shadow: inset 0 8px 12px rgba(255,255,255,0.16), inset 0 -12px 18px rgba(0,0,0,0.22);
    }
    .valve-center {
      position: absolute;
      left: 50%;
      top: 50%;
      width: 24px;
      height: 24px;
      margin-left: -12px;
      margin-top: -12px;
      border-radius: 50%;
      background: #5a636a;
      border: 1px solid rgba(0,0,0,0.45);
      z-index: 3;
    }
    .valve-port, .valve-node, .valve-path {
      position: absolute;
    }
    .valve-port {
      width: 16px;
      height: 36px;
      border-radius: 8px;
      background: linear-gradient(180deg, #dbe2e8, #9da8b0);
      border: 1px solid rgba(0,0,0,0.3);
      z-index: 1;
      transform: translate(-50%, -50%);
    }
    .valve-path {
      width: 12px;
      height: 78px;
      border-radius: 999px;
      background: rgba(160, 170, 176, 0.32);
      z-index: 2;
      transform-origin: 50% 100%;
    }
    .valve-path.active {
      background: linear-gradient(180deg, rgba(56,210,107,0.95), rgba(56,210,107,0.5));
      box-shadow: 0 0 16px rgba(56,210,107,0.4);
    }
    .valve-path.moving {
      background: linear-gradient(180deg, rgba(60,190,240,0.95), rgba(60,190,240,0.5));
      box-shadow: 0 0 16px rgba(60,190,240,0.42);
    }
    .valve-node {
      width: 30px;
      height: 30px;
      border-radius: 50%;
      border: 1px solid rgba(0,0,0,0.45);
      background: #727e86;
      color: white;
      display: grid;
      place-items: center;
      font-size: 18px;
      z-index: 4;
      transform: translate(-50%, -50%);
    }
    .valve-node.active {
      background: linear-gradient(180deg, #6af084, #33b153);
      box-shadow: 0 0 16px rgba(79,224,111,0.45);
    }
    .valve-node.moving {
      background: linear-gradient(180deg, #6bd7ff, #2694bc);
      box-shadow: 0 0 16px rgba(60,190,240,0.5);
    }
    .timer-disc {
      width: 206px;
      height: 206px;
      margin: 18px auto 12px;
      border-radius: 50%;
      border: 2px solid rgba(255,255,255,0.18);
      background: linear-gradient(180deg, #d5dbe0 0%, #acb4ba 100%);
      box-shadow: inset 0 10px 14px rgba(255,255,255,0.18), inset 0 -14px 20px rgba(0,0,0,0.2);
      display: grid;
      place-items: center;
      color: #24313a;
    }
    .timer-disc.active {
      background: linear-gradient(180deg, #ffe39b 0%, #d8a83a 100%);
      box-shadow: inset 0 10px 14px rgba(255,255,255,0.22), inset 0 -14px 20px rgba(0,0,0,0.18), 0 0 24px rgba(240,195,90,0.35);
    }
    .timer-main {
      text-align: center;
    }
    .timer-label {
      font-size: 20px;
      margin-bottom: 6px;
    }
    .timer-value {
      font-size: 34px;
      font-weight: 700;
    }
    .status-banner {
      margin-bottom: 12px;
      font-size: 24px;
      color: #edf7fb;
    }
    .status-banner strong {
      color: #9ad8ff;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      font-size: 15px;
    }
    th, td {
      border: 1px solid rgba(120,160,190,0.25);
      padding: 11px 12px;
      text-align: left;
    }
    th {
      background: rgba(74, 119, 146, 0.52);
      color: #f3f9fc;
      font-size: 16px;
    }
    td {
      background: rgba(16, 26, 34, 0.56);
    }
    .muted {
      color: var(--muted);
    }
    .hidden {
      display: none !important;
    }
    .compact-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
    }
    .triple-grid {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 10px;
    }
    .manual-label {
      display: grid;
      gap: 6px;
      color: var(--muted);
      font-size: 11px;
      text-transform: uppercase;
      letter-spacing: 0.1em;
    }
    .manual-input {
      width: 100%;
      border: 1px solid rgba(255,255,255,0.08);
      background: rgba(255,255,255,0.05);
      color: var(--text);
      border-radius: 10px;
      padding: 10px 12px;
      font-size: 14px;
      outline: none;
    }
    .manual-input:focus {
      border-color: rgba(49,184,216,0.55);
      box-shadow: 0 0 0 3px rgba(49,184,216,0.14);
    }
    .inline-actions {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
    }
    .quad-actions {
      display: grid;
      grid-template-columns: repeat(4, minmax(0, 1fr));
      gap: 10px;
    }
    .mini-btn {
      appearance: none;
      border: 0;
      border-radius: 10px;
      padding: 10px 12px;
      font-size: 12px;
      font-weight: 700;
      letter-spacing: 0.06em;
      text-transform: uppercase;
      cursor: pointer;
      width: 100%;
    }
    .mini-btn.run { background: linear-gradient(135deg, #1493b2 0%, #31b8d8 100%); color: #051219; }
    .mini-btn.stop { background: linear-gradient(135deg, #c44c42 0%, #ee6a5f 100%); color: #1b0808; }
    .mini-btn.pause { background: linear-gradient(135deg, #d69a2f 0%, #f7c96e 100%); color: #261805; }
    .mini-btn.ghost { background: rgba(255,255,255,0.06); color: var(--text); border: 1px solid var(--line); }
    .pump-toolbar {
      width: 122px;
      display: grid;
      grid-template-columns: 1fr;
      gap: 8px;
    }
    .pump-card.manual .pump-num {
      display: none;
    }
    .pump-form {
      width: 122px;
      display: grid;
      gap: 8px;
    }
    .pump-form .compact-grid {
      grid-template-columns: 1fr;
      gap: 8px;
    }
    .pump-visual.active {
      animation: pumpPulse 1.5s ease-in-out infinite;
    }
    .pump-visual.active::after {
      background:
        repeating-linear-gradient(90deg,
          rgba(255,255,255,0.05) 0 10px,
          rgba(255,255,255,0.65) 10px 18px,
          rgba(255,255,255,0.05) 18px 28px);
      animation: flowMove 1s linear infinite;
      opacity: 0.9;
    }
    .valve-actions,
    .valve-config,
    .timer-config {
      display: grid;
      gap: 10px;
      margin-bottom: 10px;
    }
    .valve-actions .quad-actions {
      grid-template-columns: repeat(4, minmax(0, 1fr));
    }
    .valve-path.active,
    .valve-path.moving {
      animation: valvePulse 1.3s ease-in-out infinite;
    }
    .timer-config .triple-grid {
      margin-bottom: 2px;
    }
    @keyframes flowMove {
      from { background-position: 0 0; }
      to { background-position: 56px 0; }
    }
    @keyframes pumpPulse {
      0%, 100% { box-shadow: inset 0 8px 12px rgba(255,255,255,0.22), inset 0 -14px 18px rgba(0,0,0,0.22), 0 0 18px rgba(79,224,111,0.35); }
      50% { box-shadow: inset 0 8px 12px rgba(255,255,255,0.24), inset 0 -14px 18px rgba(0,0,0,0.2), 0 0 28px rgba(79,224,111,0.65); }
    }
    @keyframes valvePulse {
      0%, 100% { filter: saturate(1); }
      50% { filter: saturate(1.45) brightness(1.12); }
    }
    @media (max-width: 1160px) {
      .pump-grid { grid-template-columns: repeat(3, minmax(0, 1fr)); }
      .equipment-row { grid-template-columns: 1fr; }
      .process-controls { grid-template-columns: repeat(3, minmax(0, 1fr)); }
    }
    @media (max-width: 720px) {
      .pump-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
      .title-main { font-size: 24px; }
      .status-banner { font-size: 18px; }
      th, td { font-size: 13px; }
      .process-controls { grid-template-columns: 1fr; }
      .compact-grid, .triple-grid, .inline-actions, .quad-actions, .valve-actions .quad-actions { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="shell">
    <div class="top-links">
      <a href="/dashboard">Dashboard</a>
      <a href="/dashboard#system">System</a>
    </div>
    <div class="frame">
      <div class="title-bar">
        <div class="title-main">Chromatography Process Controller</div>
        <div class="title-sub">Pumping System &amp; Flow Control</div>
      </div>

      <div class="mode-switch">
        <button id="mode-program" class="mode-btn" type="button">Program Mode</button>
        <button id="mode-manual" class="mode-btn" type="button">Manual Mode</button>
      </div>

      <form class="process-controls" id="program-controls" method="post">
        <label>
          Program
          <select name="program" id="program-select">
)rawliteral");
  html += program_options;
  html += F(R"rawliteral(
          </select>
        </label>
        <button class="process-btn run" type="submit" formaction="/api/work/run">Run</button>
        <button class="process-btn stop" type="submit" formaction="/api/work/stop">Stop</button>
        <button class="process-btn pause" type="submit" formaction="/api/work/pause">Pause</button>
        <button class="process-btn ghost" type="submit" formaction="/api/work/resume">Resume</button>
        <button class="process-btn ghost" type="submit" formaction="/api/work/skip">Skip</button>
      </form>

      <section class="pump-grid" id="pump-grid"></section>

      <section class="equipment-row">
        <div class="valve-panel">
          <div class="valve-head">Valve 1</div>
          <div class="hidden" id="valve-1-controls"></div>
          <div class="valve-wrap" id="valve-1"></div>
          <div class="muted" id="valve-1-meta">Position: --</div>
        </div>
        <div class="valve-panel">
          <div class="valve-head">Valve 2</div>
          <div class="hidden" id="valve-2-controls"></div>
          <div class="valve-wrap" id="valve-2"></div>
          <div class="muted" id="valve-2-meta">Position: --</div>
        </div>
        <div class="timer-panel">
          <div class="timer-head">Timer</div>
          <div class="hidden" id="timer-controls"></div>
          <div class="timer-disc" id="timer-disc">
            <div class="timer-main">
              <div class="timer-label" id="timer-label">IDLE</div>
              <div class="timer-value" id="timer-value">00:00:00</div>
            </div>
          </div>
          <div class="muted" id="timer-meta">No active timer step</div>
        </div>
      </section>

      <div class="status-banner" id="process-status">Current Status: <strong>Idle</strong></div>

      <section class="process-table">
        <table>
          <thead>
            <tr>
              <th>Program №</th>
              <th>Equipment</th>
              <th>State</th>
              <th>Execution Time</th>
              <th>Time Remaining</th>
              <th>Total Steps</th>
              <th>Steps Completed</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td id="tbl-program">--</td>
              <td id="tbl-equipment">--</td>
              <td id="tbl-state">--</td>
              <td id="tbl-elapsed">--</td>
              <td id="tbl-remaining">--</td>
              <td id="tbl-total">--</td>
              <td id="tbl-done">--</td>
            </tr>
            <tr>
              <th>Detail A</th>
              <th>Detail B</th>
              <th>Detail C</th>
              <th>Screen</th>
              <th>Network</th>
              <th colspan="2">IP</th>
            </tr>
            <tr>
              <td id="tbl-detail-a">--</td>
              <td id="tbl-detail-b">--</td>
              <td id="tbl-detail-c">--</td>
              <td id="tbl-screen">--</td>
              <td id="tbl-network">--</td>
              <td colspan="2" id="tbl-ip">--</td>
            </tr>
          </tbody>
        </table>
      </section>
    </div>
  </div>

  <script>
    function pumpCard(pump, manualMode) {
      const active = pump.running ? 'active' : '';
      const manualClass = manualMode ? 'manual' : '';
      const flowText = `${pump.flow.toFixed(1)} ml/min`;
      const manualToolbar = manualMode ? `
        <div class="pump-toolbar">
          <button class="mini-btn ${pump.running ? 'stop' : 'run'}" type="button" onclick="manualPumpToggle(${pump.id}, ${pump.running ? 'true' : 'false'})">${pump.running ? 'Stop' : 'Start'}</button>
        </div>
      ` : `<div class="pump-num">${pump.id}</div>`;
      const manualForm = manualMode ? `
        <div class="pump-form">
          <div class="compact-grid">
            <label class="manual-label">Flow (ml/min)<input class="manual-input" id="pump-flow-${pump.id}" type="number" step="0.1" min="0.1" value="${pump.flow.toFixed(1)}"></label>
            <label class="manual-label">Dose (ml)<input class="manual-input" id="pump-dose-${pump.id}" type="number" step="0.1" min="0.1" value="${pump.dose.toFixed(1)}"></label>
          </div>
          <button class="mini-btn run" type="button" onclick="manualPumpSave(${pump.id})">Save</button>
        </div>
      ` : `
        <div class="pump-meta">
          <div><strong>Flow:</strong> ${flowText}</div>
          <div><strong>Set Vol:</strong> ${pump.dose.toFixed(1)} ml</div>
          <div><strong>Done:</strong> ${pump.pumped.toFixed(1)} ml</div>
        </div>
      `;
      return `
        <article class="pump-card ${active} ${manualClass}">
          ${manualToolbar}
          <div class="pump-visual ${active}"></div>
          ${manualForm}
        </article>
      `;
    }

    function valveWidget(valve) {
      const angles = [-90, -18, 54, 126, 198];
      const center = 120;
      const nodeRadius = 84;
      const portRadius = 108;
      const isMoving = valve.moving;
      const activeClass = isMoving ? 'moving' : 'active';
      const pathClass = isMoving ? 'moving' : 'active';
      let html = '<div class="valve-core"></div><div class="valve-center"></div>';
      for (let i = 0; i < 5; i++) {
        const angle = angles[i];
        const rad = angle * Math.PI / 180;
        const nodeX = center + Math.cos(rad) * nodeRadius;
        const nodeY = center + Math.sin(rad) * nodeRadius;
        const portX = center + Math.cos(rad) * portRadius;
        const portY = center + Math.sin(rad) * portRadius;
        const nodeClass = (i + 1 === valve.position) ? `valve-node ${activeClass}` : 'valve-node';
        const segmentClass = (i + 1 === valve.position) ? `valve-path ${pathClass}` : 'valve-path';
        html += `<div class="valve-port" style="left:${portX}px; top:${portY}px; transform:translate(-50%,-50%) rotate(${angle + 90}deg);"></div>`;
        html += `<div class="${segmentClass}" style="left:${center}px; top:${center}px; transform:translate(-50%,-100%) rotate(${angle + 90}deg);"></div>`;
        html += `<div class="${nodeClass}" style="left:${nodeX}px; top:${nodeY}px;">${i + 1}</div>`;
      }
      return html;
    }

    function valveControls(valve) {
      return `
        <div class="valve-actions">
          <div class="quad-actions">
            <button class="mini-btn ghost" type="button" onclick="manualValveAction(${valve.id}, 'left')">Left</button>
            <button class="mini-btn run" type="button" onclick="manualValveAction(${valve.id}, 'zero')">Zero</button>
            <button class="mini-btn ghost" type="button" onclick="manualValveAction(${valve.id}, 'right')">Right</button>
            <button class="mini-btn stop" type="button" onclick="manualValveAction(${valve.id}, 'stop')">Stop</button>
          </div>
          <div class="valve-config">
            <div class="triple-grid">
              <label class="manual-label">F (steps/min)<input class="manual-input" id="valve-speed-${valve.id}" type="number" step="1" min="10" value="${valve.speed}"></label>
              <label class="manual-label">Zero (steps)<input class="manual-input" id="valve-zero-${valve.id}" type="number" step="10" min="0" value="${valve.zero}"></label>
              <label class="manual-label">Ratio (:1)<input class="manual-input" id="valve-ratio-${valve.id}" type="number" step="0.1" min="1" value="${valve.ratio.toFixed(1)}"></label>
            </div>
            <label class="manual-label">REV<input class="manual-input" id="valve-rev-${valve.id}" type="checkbox" ${valve.rev ? 'checked' : ''}></label>
            <button class="mini-btn run" type="button" onclick="manualValveSave(${valve.id})">Save</button>
          </div>
        </div>
      `;
    }

    function renderTimerControls(timers) {
      document.getElementById('timer-controls').innerHTML = timerControls(timers);
    }

    function timerControls(timers) {
      const activeOption = Number(window.selectedTimerId || 1);
      const timer = timers.find(t => t.id === Number(activeOption)) || timers[0];
      const parts = timer.value.split(':');
      const options = timers.map(t => `<option value="${t.id}" ${t.id === timer.id ? 'selected' : ''}>Timer ${t.id}</option>`).join('');
      return `
        <div class="timer-config">
          <label class="manual-label">Timer Select
            <select class="manual-input" id="timer-select" onchange="window.selectedTimerId=Number(this.value); if(window.latestData) renderTimerControls(window.latestData.timers);">
              ${options}
            </select>
          </label>
          <div class="triple-grid">
            <label class="manual-label">Hours<input class="manual-input" id="timer-hours" type="number" min="0" max="99" value="${parts[0]}"></label>
            <label class="manual-label">Minutes<input class="manual-input" id="timer-minutes" type="number" min="0" max="59" value="${parts[1]}"></label>
            <label class="manual-label">Seconds<input class="manual-input" id="timer-seconds" type="number" min="0" max="59" value="${parts[2]}"></label>
          </div>
          <button class="mini-btn run" type="button" onclick="manualTimerSave()">Save</button>
        </div>
      `;
    }

    function updateProcessStatus(data) {
      window.latestData = data;
      const pumpGrid = document.getElementById('pump-grid');
      const manualMode = data.operator_mode === 'MANUAL';
      pumpGrid.innerHTML = data.pumps.map(p => pumpCard(p, manualMode)).join('');
      document.getElementById('program-controls').classList.toggle('hidden', manualMode);
      document.getElementById('mode-program').classList.toggle('active', !manualMode);
      document.getElementById('mode-manual').classList.toggle('active', manualMode);
      document.getElementById('valve-1-controls').classList.toggle('hidden', !manualMode);
      document.getElementById('valve-2-controls').classList.toggle('hidden', !manualMode);
      document.getElementById('timer-controls').classList.toggle('hidden', !manualMode);
      document.getElementById('valve-1-controls').innerHTML = manualMode ? valveControls(data.valves[0]) : '';
      document.getElementById('valve-2-controls').innerHTML = manualMode ? valveControls(data.valves[1]) : '';
      if (manualMode && !window.selectedTimerId) window.selectedTimerId = 1;
      document.getElementById('timer-controls').innerHTML = manualMode ? timerControls(data.timers) : '';

      document.getElementById('valve-1').innerHTML = valveWidget(data.valves[0]);
      document.getElementById('valve-2').innerHTML = valveWidget(data.valves[1]);
      document.getElementById('valve-1-meta').textContent =
        `Position: ${data.valves[0].position} | F: ${data.valves[0].speed} | Ratio: ${data.valves[0].ratio.toFixed(1)}:1`;
      document.getElementById('valve-2-meta').textContent =
        `Position: ${data.valves[1].position} | F: ${data.valves[1].speed} | Ratio: ${data.valves[1].ratio.toFixed(1)}:1`;

      const timerActive = data.work.active_type === 'TIMER';
      const timerDisc = document.getElementById('timer-disc');
      timerDisc.classList.toggle('active', timerActive);
      document.getElementById('timer-label').textContent = timerActive ? 'ACTIVE' : 'STANDBY';
      document.getElementById('timer-value').textContent = timerActive ? data.work.step_remaining : data.timers[0].value;
      document.getElementById('timer-meta').textContent = timerActive ? data.work.detail_a : 'No active timer step';

      document.getElementById('process-status').innerHTML =
        `Current Status: <strong>${data.work.equipment}</strong> | ${data.work.equipment_state} | ${data.work.detail_a}`;

      document.getElementById('tbl-program').textContent = `№${data.work.program}`;
      document.getElementById('tbl-equipment').textContent = data.work.equipment;
      document.getElementById('tbl-state').textContent = data.work.state;
      document.getElementById('tbl-elapsed').textContent = data.work.step_elapsed;
      document.getElementById('tbl-remaining').textContent = data.work.program_remaining;
      document.getElementById('tbl-total').textContent = data.work.step_total;
      document.getElementById('tbl-done').textContent = data.work.steps_done;
      document.getElementById('tbl-detail-a').textContent = data.work.detail_a;
      document.getElementById('tbl-detail-b').textContent = data.work.detail_b;
      document.getElementById('tbl-detail-c').textContent = data.work.detail_c;
      document.getElementById('tbl-screen').textContent = data.screen;
      document.getElementById('tbl-network').textContent = data.network.mode;
      document.getElementById('tbl-ip').textContent = data.network.ip;
    }

    function markInteraction() {
      window.lastProcessInteractionAt = Date.now();
    }

    async function submitProcessForm(form, submitter) {
      const action = submitter && submitter.formAction ? submitter.formAction : form.action;
      const formData = new FormData(form);
      formData.append('_async', '1');
      if (submitter && submitter.name) formData.append(submitter.name, submitter.value);
      const query = new URLSearchParams();
      for (const [key, value] of formData.entries()) {
        query.append(key, value);
      }

      window.processFormBusy = true;
      try {
        await fetch(`${action}?${query.toString()}`, {
          method: 'GET',
          cache: 'no-store',
          headers: { 'X-Requested-With': 'fetch' }
        });
      } finally {
        window.processFormBusy = false;
      }

      await refresh(true);
    }

    async function postAction(url, values) {
      const query = new URLSearchParams();
      query.append('_async', '1');
      Object.entries(values || {}).forEach(([key, value]) => query.append(key, value));

      window.processFormBusy = true;
      try {
        await fetch(`${url}?${query.toString()}`, {
          method: 'GET',
          cache: 'no-store',
          headers: { 'X-Requested-With': 'fetch' }
        });
      } finally {
        window.processFormBusy = false;
      }

      await refresh(true);
    }

    async function manualPumpToggle(id, running) {
      markInteraction();
      await postAction(running ? '/api/manual/pump/stop' : '/api/manual/pump/start', { pump: String(id) });
    }

    async function manualPumpSave(id) {
      markInteraction();
      await postAction('/api/pump', {
        pump: String(id),
        flow: document.getElementById(`pump-flow-${id}`).value,
        dose: document.getElementById(`pump-dose-${id}`).value,
        return_to: '/process'
      });
    }

    async function manualValveAction(id, action) {
      markInteraction();
      await postAction('/api/manual/valve/action', { valve: String(id), action });
    }

    async function manualValveSave(id) {
      markInteraction();
      await postAction('/api/manual/valve/save', {
        valve: String(id),
        speed: document.getElementById(`valve-speed-${id}`).value,
        zero: document.getElementById(`valve-zero-${id}`).value,
        ratio: document.getElementById(`valve-ratio-${id}`).value,
        rev: document.getElementById(`valve-rev-${id}`).checked ? 'on' : ''
      });
    }

    async function manualTimerSave() {
      markInteraction();
      await postAction('/api/manual/timer/save', {
        timer: document.getElementById('timer-select').value,
        hours: document.getElementById('timer-hours').value,
        minutes: document.getElementById('timer-minutes').value,
        seconds: document.getElementById('timer-seconds').value
      });
    }

    function attachModeSwitch() {
      if (window.modeSwitchAttached) return;
      document.getElementById('mode-program').addEventListener('click', async () => {
        await postAction('/api/operator/mode', { mode: 'program' });
      });
      document.getElementById('mode-manual').addEventListener('click', async () => {
        await postAction('/api/operator/mode', { mode: 'manual' });
      });
      window.modeSwitchAttached = true;
    }

    function attachProcessForms() {
      if (window.processFormsAttached) return;
      document.addEventListener('pointerdown', markInteraction, true);
      document.addEventListener('keydown', markInteraction, true);
      document.addEventListener('input', markInteraction, true);
      document.addEventListener('change', markInteraction, true);
      document.addEventListener('click', event => {
        const button = event.target.closest('button[type="submit"], input[type="submit"]');
        if (!button || !button.form) return;
        markInteraction();
        button.form._lastSubmitter = button;
      });
      document.addEventListener('submit', async event => {
        const form = event.target;
        if (!(form instanceof HTMLFormElement)) return;
        markInteraction();
        event.preventDefault();
        const submitter = event.submitter || form._lastSubmitter || null;
        await submitProcessForm(form, submitter);
        form._lastSubmitter = null;
      });
      window.processFormsAttached = true;
    }

    async function refresh(force = false) {
      if (window.processFormBusy) return;
      const activeTag = document.activeElement ? document.activeElement.tagName : '';
      const recentInteraction = Date.now() - (window.lastProcessInteractionAt || 0) < 1200;
      if (!force && recentInteraction) return;
      if (!force && (activeTag === 'INPUT' || activeTag === 'SELECT' || activeTag === 'TEXTAREA')) return;
      const response = await fetch('/api/status', { cache: 'no-store' });
      const data = await response.json();
      updateProcessStatus(data);
    }

    attachModeSwitch();
    attachProcessForms();
    refresh();
    setInterval(refresh, 1000);
  </script>
</body>
</html>
)rawliteral");

  return html;
}

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", buildPage());
}

void handleProcessPage() {
  server.send(200, "text/html; charset=utf-8", buildProcessPage());
}

void handleStatus() {
  server.send(200, "application/json; charset=utf-8", buildStatusJson());
}

void handleWorkRun() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }

  uint8_t program_index = 0;
  if (server.hasArg("program")) {
    const int requested = server.arg("program").toInt();
    if (requested >= 1 && requested <= kProgramCount) program_index = static_cast<uint8_t>(requested - 1);
  }

  g_state->operator_manual_mode = false;
  ui::startWorkProgram(*g_state, program_index);
  completeRequest("/process");
}

void handleWorkStop() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }
  ui::stopWorkProgram(*g_state);
  completeRequest("/process");
}

void handleWorkPause() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }
  ui::pauseWorkProgram(*g_state);
  completeRequest("/process");
}

void handleWorkResume() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }
  ui::resumeWorkProgram(*g_state);
  completeRequest("/process");
}

void handleWorkSkip() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }
  ui::skipWorkStep(*g_state);
  completeRequest("/process");
}

void handleOperatorMode() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }

  const String mode = server.arg("mode");
  if (mode == "manual") {
    ui::stopWorkProgram(*g_state);
    g_state->operator_manual_mode = true;
  } else {
    g_state->operator_manual_mode = false;
  }
  completeRequest("/process");
}

void handlePumpSave() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }

  if (!server.hasArg("pump")) {
    server.send(400, "text/plain", "Pump index missing");
    return;
  }

  const int requested = server.arg("pump").toInt();
  if (requested < 1 || requested > kPumpCount) {
    server.send(400, "text/plain", "Pump index out of range");
    return;
  }

  const uint8_t pump_index = static_cast<uint8_t>(requested - 1);
  const float flow_ml_min = server.hasArg("flow") ? server.arg("flow").toFloat() : (g_state->pumps[pump_index].flow_ml_hour / 60.0f);
  const float dose_ml = server.hasArg("dose") ? server.arg("dose").toFloat() : g_state->pumps[pump_index].target_volume_ml;

  ui::updatePumpConfig(*g_state, pump_index, flow_ml_min * 60.0f, dose_ml);
  storage::savePumpConfig(*g_state, pump_index);
  g_state->pumps[pump_index].config_dirty = false;
  completeRequest(server.hasArg("return_to") ? server.arg("return_to").c_str() : "/dashboard#pump-config");
}

void handleManualPumpStart() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }
  const int requested = server.arg("pump").toInt();
  if (requested < 1 || requested > kPumpCount) {
    server.send(400, "text/plain", "Pump index out of range");
    return;
  }
  g_state->operator_manual_mode = true;
  ui::stopWorkProgram(*g_state);
  ui::startManualPump(*g_state, static_cast<uint8_t>(requested - 1));
  completeRequest("/process");
}

void handleManualPumpStop() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }
  const int requested = server.arg("pump").toInt();
  if (requested < 1 || requested > kPumpCount) {
    server.send(400, "text/plain", "Pump index out of range");
    return;
  }
  g_state->operator_manual_mode = true;
  ui::stopManualPump(*g_state, static_cast<uint8_t>(requested - 1));
  completeRequest("/process");
}

void handleValveSave() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }
  const int requested = server.arg("valve").toInt();
  if (requested < 1 || requested > kValveCount) {
    server.send(400, "text/plain", "Valve index out of range");
    return;
  }
  const uint8_t valve_index = static_cast<uint8_t>(requested - 1);
  const float speed = server.hasArg("speed") ? server.arg("speed").toFloat() : g_state->valves[valve_index].speed_steps;
  const uint16_t zero = server.hasArg("zero") ? static_cast<uint16_t>(server.arg("zero").toInt())
                                              : g_state->valves[valve_index].zero_offset_steps;
  const float ratio = server.hasArg("ratio") ? server.arg("ratio").toFloat() : g_state->valves[valve_index].ratio;
  const bool rev = server.hasArg("rev") && server.arg("rev").length() > 0;

  g_state->operator_manual_mode = true;
  ui::updateValveConfig(*g_state, valve_index, speed, zero, ratio, rev);
  storage::saveValveConfig(*g_state, valve_index);
  g_state->valves[valve_index].config_dirty = false;
  completeRequest("/process");
}

void handleValveAction() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }
  const int requested = server.arg("valve").toInt();
  if (requested < 1 || requested > kValveCount) {
    server.send(400, "text/plain", "Valve index out of range");
    return;
  }
  const String action = server.arg("action");
  ui::stopWorkProgram(*g_state);
  g_state->operator_manual_mode = true;
  const uint8_t valve_index = static_cast<uint8_t>(requested - 1);

  if (action == "left")
    ui::commandManualValve(*g_state, valve_index, ui::ManualValveAction::Left);
  else if (action == "right")
    ui::commandManualValve(*g_state, valve_index, ui::ManualValveAction::Right);
  else if (action == "zero")
    ui::commandManualValve(*g_state, valve_index, ui::ManualValveAction::Zero);
  else
    ui::commandManualValve(*g_state, valve_index, ui::ManualValveAction::Stop);

  completeRequest("/process");
}

void handleTimerSave() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }
  const int requested = server.arg("timer").toInt();
  if (requested < 1 || requested > kTimerCount) {
    server.send(400, "text/plain", "Timer index out of range");
    return;
  }
  const uint8_t timer_index = static_cast<uint8_t>(requested - 1);
  const uint8_t hours = static_cast<uint8_t>(server.hasArg("hours") ? server.arg("hours").toInt() : g_state->timers[timer_index].hours);
  const uint8_t minutes =
      static_cast<uint8_t>(server.hasArg("minutes") ? server.arg("minutes").toInt() : g_state->timers[timer_index].minutes);
  const uint8_t seconds =
      static_cast<uint8_t>(server.hasArg("seconds") ? server.arg("seconds").toInt() : g_state->timers[timer_index].seconds);

  g_state->operator_manual_mode = true;
  ui::updateTimerConfig(*g_state, timer_index, hours, minutes, seconds);
  storage::saveTimerConfig(*g_state, timer_index);
  g_state->timers[timer_index].config_dirty = false;
  completeRequest("/process");
}

void handleProgramNameSave() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }
  const int requested = server.arg("program").toInt();
  if (requested < 1 || requested > kProgramCount) {
    server.send(400, "text/plain", "Program index out of range");
    return;
  }

  const uint8_t program_index = static_cast<uint8_t>(requested - 1);
  const String name = server.hasArg("name") ? server.arg("name") : "";
  name.toCharArray(g_state->program_names[program_index], sizeof(g_state->program_names[program_index]));
  storage::saveProgramNames(*g_state);
  completeRequest("/dashboard#programs");
}

void handleProgramStepSave() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }

  const int requested_program = server.arg("program").toInt();
  const int requested_step = server.arg("step").toInt();
  if (requested_program < 1 || requested_program > kProgramCount || requested_step < 1 || requested_step > kProgramStepCount) {
    server.send(400, "text/plain", "Program or step index out of range");
    return;
  }

  const uint8_t program_index = static_cast<uint8_t>(requested_program - 1);
  const uint8_t step_index = static_cast<uint8_t>(requested_step - 1);
  WorkStepState &step = g_state->programs[program_index].steps[step_index];

  g_state->selected_program = program_index;
  g_state->selected_work_step = step_index;

  if (server.hasArg("clear")) {
    step.type = WorkStepType::None;
    step.target_index = 0;
    step.aux_value = 0;
    storage::saveWorkProgram(*g_state, program_index);
    const String redirect = "/dashboard?program=" + String(requested_program) + "&step=" + String(requested_step) + "#programs";
    completeRequest(redirect.c_str());
    return;
  }

  const String type_arg = server.arg("type");
  if (type_arg == "pump") {
    step.type = WorkStepType::Pump;
    step.target_index = static_cast<uint8_t>(constrain(server.arg("target").toInt(), 1, kPumpCount) - 1);
    step.aux_value = 0;
  } else if (type_arg == "valve") {
    step.type = WorkStepType::Valve;
    step.target_index = static_cast<uint8_t>(constrain(server.arg("target").toInt(), 1, kValveCount) - 1);
    step.aux_value = static_cast<uint8_t>(constrain(server.arg("aux").toInt(), 0, 5));
  } else if (type_arg == "timer") {
    step.type = WorkStepType::Timer;
    step.target_index = static_cast<uint8_t>(constrain(server.arg("target").toInt(), 1, kTimerCount) - 1);
    step.aux_value = 0;
  } else {
    step.type = WorkStepType::None;
    step.target_index = 0;
    step.aux_value = 0;
  }

  storage::saveWorkProgram(*g_state, program_index);
  const String redirect = "/dashboard?program=" + String(requested_program) + "&step=" + String(requested_step) + "#programs";
  completeRequest(redirect.c_str());
}

void handleSystemSave() {
  if (!g_state) {
    server.send(500, "text/plain", "State unavailable");
    return;
  }

  AppState &state = *g_state;
  if (server.hasArg("pump_units_per_ml"))
    state.system.pump_units_per_ml = server.arg("pump_units_per_ml").toFloat();

  if (server.hasArg("wifi_ssid")) {
    const String ssid = server.arg("wifi_ssid");
    ssid.toCharArray(state.system.wifi_ssid, sizeof(state.system.wifi_ssid));
  }

  if (server.hasArg("wifi_password")) {
    const String password = server.arg("wifi_password");
    if (password.length() > 0)
      password.toCharArray(state.system.wifi_password, sizeof(state.system.wifi_password));
  }

  storage::saveSystemSettings(state);
  serial_link::setPumpUnitsPerMl(state.system.pump_units_per_ml);

  completeRequest("/dashboard#system");
}

void startAccessPoint() {
  const uint32_t chip = static_cast<uint32_t>(ESP.getEfuseMac());
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06lX", static_cast<unsigned long>(chip & 0xFFFFFF));
  g_ap_ssid = "Column-Control-" + String(suffix);
  g_ap_password = "column123";
  WiFi.mode(WIFI_AP);
  WiFi.softAP(g_ap_ssid.c_str(), g_ap_password.c_str());
  g_ap_mode = true;
  Serial.printf("WEB AP started: SSID=%s PASS=%s IP=%s\n", g_ap_ssid.c_str(), g_ap_password.c_str(),
                WiFi.softAPIP().toString().c_str());
}

void startNetwork() {
  if (!g_state) return;

  const String ssid = String(g_state->system.wifi_ssid);
  const String password = String(g_state->system.wifi_password);

  if (ssid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.printf("Connecting to Wi-Fi SSID=%s\n", ssid.c_str());

    const uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < 10000UL) {
      delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
      g_ap_mode = false;
      g_ap_ssid = "";
      g_ap_password = "";
      Serial.printf("WEB STA connected: IP=%s\n", WiFi.localIP().toString().c_str());
      return;
    }
  }

  startAccessPoint();
}

}  // namespace

namespace web_ui {

void init(AppState &state) {
  g_state = &state;
  startNetwork();

  server.on("/", HTTP_GET, handleProcessPage);
  server.on("/dashboard", HTTP_GET, handleRoot);
  server.on("/process", HTTP_GET, handleProcessPage);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/operator/mode", HTTP_POST, handleOperatorMode);
  server.on("/api/operator/mode", HTTP_GET, handleOperatorMode);
  server.on("/api/work/run", HTTP_POST, handleWorkRun);
  server.on("/api/work/run", HTTP_GET, handleWorkRun);
  server.on("/api/work/stop", HTTP_POST, handleWorkStop);
  server.on("/api/work/stop", HTTP_GET, handleWorkStop);
  server.on("/api/work/pause", HTTP_POST, handleWorkPause);
  server.on("/api/work/pause", HTTP_GET, handleWorkPause);
  server.on("/api/work/resume", HTTP_POST, handleWorkResume);
  server.on("/api/work/resume", HTTP_GET, handleWorkResume);
  server.on("/api/work/skip", HTTP_POST, handleWorkSkip);
  server.on("/api/work/skip", HTTP_GET, handleWorkSkip);
  server.on("/api/pump", HTTP_POST, handlePumpSave);
  server.on("/api/pump", HTTP_GET, handlePumpSave);
  server.on("/api/manual/pump/start", HTTP_POST, handleManualPumpStart);
  server.on("/api/manual/pump/start", HTTP_GET, handleManualPumpStart);
  server.on("/api/manual/pump/stop", HTTP_POST, handleManualPumpStop);
  server.on("/api/manual/pump/stop", HTTP_GET, handleManualPumpStop);
  server.on("/api/manual/valve/save", HTTP_POST, handleValveSave);
  server.on("/api/manual/valve/save", HTTP_GET, handleValveSave);
  server.on("/api/manual/valve/action", HTTP_POST, handleValveAction);
  server.on("/api/manual/valve/action", HTTP_GET, handleValveAction);
  server.on("/api/manual/timer/save", HTTP_POST, handleTimerSave);
  server.on("/api/manual/timer/save", HTTP_GET, handleTimerSave);
  server.on("/api/program/name", HTTP_POST, handleProgramNameSave);
  server.on("/api/program/step", HTTP_POST, handleProgramStepSave);
  server.on("/api/system", HTTP_POST, handleSystemSave);
  server.begin();

  Serial.println("Web UI ready on port 80");
}

void tick() {
  server.handleClient();
}

}  // namespace web_ui
