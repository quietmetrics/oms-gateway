#pragma once

#include "esp_err.h"
#include "app/services.h"

// Start HTTP server (port 80) serving UI + REST. Keeps pointer to services.
esp_err_t http_server_start(services_state_t *svc);
// Stop server (optional; no-op if not started).
void http_server_stop(void);
