# CMakeLists.txt for the config component

set(srcs "config_manager.c" "http_handlers.c" "web_ui.c")

set(include_dirs ".")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    PRIV_REQUIRES esp_wifi esp_http_server nvs_flash json
)