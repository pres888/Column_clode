#include <M5Unified.h>

#include "app_types.h"
#include "serial_link.h"
#include "storage.h"
#include "ui.h"
#include "web_ui.h"

namespace {

AppState app_state;

}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.begin(115200);
  delay(200);

  serial_link::init();
  storage::load(app_state);
  serial_link::setPumpUnitsPerMl(app_state.system.pump_units_per_ml);
  ui::init();
  web_ui::init(app_state);
  Serial.println("M5Core2 boot OK");
}

void loop() {
  M5.update();
  serial_link::tick();
  web_ui::tick();
  ui::tick(app_state);
  delay(10);
}
