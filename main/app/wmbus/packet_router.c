#include "app/wmbus/packet_router.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "packet_router";

#define MAX_SINKS 4

typedef struct
{
    wmbus_packet_sink_fn fn;
    void *user;
} sink_entry_t;

static sink_entry_t s_sinks[MAX_SINKS];

esp_err_t wmbus_packet_router_init(void)
{
    memset(s_sinks, 0, sizeof(s_sinks));
    return ESP_OK;
}

esp_err_t wmbus_packet_router_register(wmbus_packet_sink_fn fn, void *user)
{
    if (!fn)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < MAX_SINKS; i++)
    {
        if (!s_sinks[i].fn)
        {
            s_sinks[i].fn = fn;
            s_sinks[i].user = user;
            return ESP_OK;
        }
    }

    ESP_LOGW(TAG, "No slot left to register sink");
    return ESP_ERR_NO_MEM;
}

void wmbus_packet_router_dispatch(const WmbusPacketEvent *evt)
{
    if (!evt)
    {
        return;
    }

    for (size_t i = 0; i < MAX_SINKS; i++)
    {
        if (s_sinks[i].fn)
        {
            s_sinks[i].fn(evt, s_sinks[i].user);
        }
    }
}
