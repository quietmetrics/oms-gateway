#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include <esp_err.h>

typedef enum {
    RADIO_STATUS_UNINITIALIZED = 0,
    RADIO_STATUS_READY,
    RADIO_STATUS_ERROR
} radio_status_state_t;

typedef struct {
    radio_status_state_t state;
    char message[96];
    esp_err_t last_error;
} radio_status_info_t;

void system_status_set_radio(radio_status_state_t state, const char *message, esp_err_t err);
const radio_status_info_t* system_status_get_radio(void);

#endif // SYSTEM_STATUS_H
