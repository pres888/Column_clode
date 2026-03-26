# API Documentation
## M5Core2 Chromatography Process Controller

Version: current firmware state as of 2026-03-19

This document describes the HTTP API implemented in the M5Core2 controller web interface.

## 1. Purpose

The API allows external software to:

- read the current state of the system;
- switch between `PROGRAM` and `MANUAL` operator modes;
- start, stop, pause, resume, and skip program execution;
- edit pump, valve, timer, and program parameters;
- run pumps and valves manually;
- change system settings.

## 2. Base Address

In access point mode:

```text
http://192.168.4.1
```

In station mode:

```text
http://<controller-ip>
```

## 3. Request Behavior

- `GET /api/status` always returns JSON.
- Many control endpoints accept both `GET` and `POST`.
- For asynchronous control, add the parameter `_async=1`.
- With `_async=1`, the response is:

```json
{"ok":true}
```

- Without `_async=1`, many endpoints return HTTP `303` redirect back to a web page.
- Validation errors return HTTP `400`.
- Internal errors return HTTP `500`.

## 4. Status Endpoint

### 4.1 Get full controller state

```text
GET /api/status
```

Example:

```bash
curl http://192.168.4.1/api/status
```

Main response structure:

```json
{
  "screen": "WORK",
  "operator_mode": "PROGRAM",
  "network": {
    "mode": "AP",
    "ip": "192.168.4.1",
    "ap_ssid": "Column-Control-XXXXXX"
  },
  "work": {
    "state": "RUNNING",
    "program": 1,
    "step": 3,
    "step_total": 8,
    "steps_done": 2,
    "active_type": "PUMP",
    "equipment": "PUMP P1",
    "equipment_state": "DOSING",
    "step_elapsed": "00:00:12",
    "step_remaining": "00:00:48",
    "program_remaining": "00:12:30",
    "detail_a": "Flow 1.0 ml/min",
    "detail_b": "Dose 5.0 ml",
    "detail_c": "Done 1.2 ml"
  },
  "pumps": [],
  "valves": [],
  "timers": [],
  "programs": [],
  "system": {
    "pump_units_per_ml": 87.040
  }
}
```

## 5. Operator Mode

### 5.1 Set operator mode

```text
GET|POST /api/operator/mode
```

Parameters:

- `mode=program`
- `mode=manual`

Example:

```bash
curl "http://192.168.4.1/api/operator/mode?mode=manual&_async=1"
```

## 6. Program Execution Control

### 6.1 Start a program

```text
GET|POST /api/work/run
```

Parameters:

- `program=1..50` optional, default is `1`

Example:

```bash
curl "http://192.168.4.1/api/work/run?program=3&_async=1"
```

### 6.2 Stop current execution

```text
GET|POST /api/work/stop
```

Example:

```bash
curl "http://192.168.4.1/api/work/stop?_async=1"
```

### 6.3 Pause current execution

```text
GET|POST /api/work/pause
```

Example:

```bash
curl "http://192.168.4.1/api/work/pause?_async=1"
```

### 6.4 Resume current execution

```text
GET|POST /api/work/resume
```

Example:

```bash
curl "http://192.168.4.1/api/work/resume?_async=1"
```

### 6.5 Skip current step

```text
GET|POST /api/work/skip
```

Example:

```bash
curl "http://192.168.4.1/api/work/skip?_async=1"
```

## 7. Pumps

### 7.1 Save pump parameters

```text
GET|POST /api/pump
```

Parameters:

- `pump=1..6` required
- `flow=<ml/min>` optional
- `dose=<ml>` optional
- `return_to=<url>` optional for web redirect only

Example:

```bash
curl "http://192.168.4.1/api/pump?pump=1&flow=1.2&dose=5.0&_async=1"
```

### 7.2 Start pump manually

```text
GET|POST /api/manual/pump/start
```

Parameters:

- `pump=1..6`

Example:

```bash
curl "http://192.168.4.1/api/manual/pump/start?pump=1&_async=1"
```

### 7.3 Stop pump manually

```text
GET|POST /api/manual/pump/stop
```

Parameters:

- `pump=1..6`

Example:

```bash
curl "http://192.168.4.1/api/manual/pump/stop?pump=1&_async=1"
```

## 8. Valves

### 8.1 Save valve parameters

```text
GET|POST /api/manual/valve/save
```

Parameters:

- `valve=1..2` required
- `speed=<steps/min>`
- `zero=<steps>`
- `ratio=<value>`
- `rev=1` enables reverse mode

Important:

- Reverse mode is determined by the presence of `rev`.
- To save `rev=false`, omit the `rev` parameter completely.

Example:

```bash
curl "http://192.168.4.1/api/manual/valve/save?valve=1&speed=1200&zero=5000&ratio=40.0&rev=1&_async=1"
```

### 8.2 Execute valve action

```text
GET|POST /api/manual/valve/action
```

Parameters:

- `valve=1..2`
- `action=left|right|zero|stop`

Example:

```bash
curl "http://192.168.4.1/api/manual/valve/action?valve=1&action=left&_async=1"
```

## 9. Timers

### 9.1 Save timer parameters

```text
GET|POST /api/manual/timer/save
```

Parameters:

- `timer=1..6`
- `hours=0..99`
- `minutes=0..59`
- `seconds=0..59`

Example:

```bash
curl "http://192.168.4.1/api/manual/timer/save?timer=2&hours=0&minutes=5&seconds=30&_async=1"
```

## 10. Program Names

### 10.1 Save program name

```text
POST /api/program/name
```

Parameters:

- `program=1..50`
- `name=<up to 20 characters>`

Example:

```bash
curl -X POST "http://192.168.4.1/api/program/name" \
  -d "program=1" \
  -d "name=Eluent A start"
```

## 11. Program Steps

### 11.1 Save or update a program step

```text
POST /api/program/step
```

Parameters:

- `program=1..50`
- `step=1..25`
- `type=pump|valve|timer|none`
- `target=<index>`
- `aux=<value>` used for valve position
- `clear=1` clears the selected step

Rules:

- For `type=pump`, `target=1..6`
- For `type=valve`, `target=1..2`, `aux=0..5`
- For `type=timer`, `target=1..6`
- For valve steps:
  - `aux=0` means `ZERO`
  - `aux=1..5` means valve position `1..5`

Examples:

Pump step:

```bash
curl -X POST "http://192.168.4.1/api/program/step" \
  -d "program=1" \
  -d "step=1" \
  -d "type=pump" \
  -d "target=2"
```

Valve step with ZERO:

```bash
curl -X POST "http://192.168.4.1/api/program/step" \
  -d "program=1" \
  -d "step=2" \
  -d "type=valve" \
  -d "target=1" \
  -d "aux=0"
```

Clear step:

```bash
curl -X POST "http://192.168.4.1/api/program/step" \
  -d "program=1" \
  -d "step=3" \
  -d "clear=1"
```

## 12. System Settings

### 12.1 Save system settings

```text
POST /api/system
```

Parameters:

- `pump_units_per_ml=<float>`
- `wifi_ssid=<string>`
- `wifi_password=<string>`

Important:

- Wi‑Fi password is updated only if a non-empty string is sent.

Example:

```bash
curl -X POST "http://192.168.4.1/api/system" \
  -d "pump_units_per_ml=87.04" \
  -d "wifi_ssid=MyWiFi" \
  -d "wifi_password=secret123"
```

## 13. Recommended Integration Pattern

For external software:

1. Poll `GET /api/status` every `0.5-1.0 s`
2. Send control commands with `_async=1`
3. Treat this API as a working internal API
4. If you later build a production integration, define a versioned API layer on top of these endpoints

## 14. Short Notes About Formats

### OpenAPI / Swagger

This is a formal machine-readable API description format. It is useful when you want:

- automatic API documentation;
- client code generation;
- import into Swagger UI or other API tools.

### Postman Collection

This is a request collection format for Postman. It is useful when you want:

- click-ready test requests;
- grouped manual API testing;
- a ready set of calls for debugging.

For printing and keeping as a technical document, this Markdown file is the most practical format.
