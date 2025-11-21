# CMakeLists.txt for whitelist component

idf_component_register(
    SRCS "whitelist_manager.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_common esp_log
)