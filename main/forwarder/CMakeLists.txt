# CMakeLists.txt for forwarder component

idf_component_register(
    SRCS "forwarder.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS ""
    PRIV_REQUIRES esp_http_client esp-tls nvs_flash json
)