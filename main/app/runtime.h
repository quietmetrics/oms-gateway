// Application runtime wiring: init services, radio, routing, and run RX loop.
#pragma once

#include "esp_err.h"

// Initialize subsystems and start the main receive/dispatch loop (blocking).
void app_run(void);
