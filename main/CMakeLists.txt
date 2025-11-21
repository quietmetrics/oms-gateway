# CMakeLists.txt for the main component
# This file defines the main application component

idf_component_register(
    SRCS
        "app_main.c"
        "cc1101/cc1101_driver.c"
        "cc1101/cc1101_config.c"
        "wmbus/wmbus_protocol.c"
        "wmbus/three_of_six.c"
        "wmbus/wmbus_receiver.c"
        "whitelist/whitelist_manager.c"
        "forwarder/forwarder.c"
        "config/config_manager.c"
        "config/http_handlers.c"
        "config/web_ui.c"
        "nvs_manager/nvs_manager.c"
        "system_status.c"
    INCLUDE_DIRS
        "."
        "cc1101"
        "wmbus"
        "whitelist"
        "forwarder"
        "config"
        "nvs_manager"
    PRIV_REQUIRES
        driver
        spi_flash
        esp_timer
        log
        nvs_flash
        esp_http_client
        esp-tls
        json
        esp_http_server
        esp_wifi
)
