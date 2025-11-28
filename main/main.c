#include <stdio.h>

#include "esp_log.h"
#include "app/runtime.h"

void app_main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("app", ESP_LOG_INFO);
    esp_log_level_set("wmbus_rx", ESP_LOG_DEBUG);
    app_run();
}
