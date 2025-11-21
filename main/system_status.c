#include "system_status.h"
#include <string.h>

static radio_status_info_t radio_status = {
    .state = RADIO_STATUS_UNINITIALIZED,
    .message = "Radio not initialized",
    .last_error = ESP_OK
};

void system_status_set_radio(radio_status_state_t state, const char *message, esp_err_t err)
{
    radio_status.state = state;
    if (message && message[0] != '\0') {
        strncpy(radio_status.message, message, sizeof(radio_status.message) - 1);
        radio_status.message[sizeof(radio_status.message) - 1] = '\0';
    } else {
        strncpy(radio_status.message, "No status provided", sizeof(radio_status.message) - 1);
        radio_status.message[sizeof(radio_status.message) - 1] = '\0';
    }
    radio_status.last_error = err;
}

const radio_status_info_t* system_status_get_radio(void)
{
    return &radio_status;
}
