// Pin and SPI host mapping for CC1101 on ESP32-C3
#pragma once

#include "driver/spi_common.h"
#include "driver/gpio.h"

#define CC1101_SPI_HOST      SPI2_HOST
#define CC1101_PIN_MISO      GPIO_NUM_5
#define CC1101_PIN_MOSI      GPIO_NUM_6
#define CC1101_PIN_SCLK      GPIO_NUM_4
#define CC1101_PIN_CS        GPIO_NUM_7
#define CC1101_PIN_GDO0      GPIO_NUM_2  // FIFO threshold / RX FIFO event
#define CC1101_PIN_GDO2      GPIO_NUM_3  // Sync/packet done

typedef struct
{
    spi_host_device_t host;
    gpio_num_t miso;
    gpio_num_t mosi;
    gpio_num_t sclk;
    gpio_num_t cs;
    gpio_num_t gdo0;
    gpio_num_t gdo2;
} cc1101_pin_config_t;

static inline cc1101_pin_config_t cc1101_default_pins(void)
{
    cc1101_pin_config_t pins = {
        .host = CC1101_SPI_HOST,
        .miso = CC1101_PIN_MISO,
        .mosi = CC1101_PIN_MOSI,
        .sclk = CC1101_PIN_SCLK,
        .cs = CC1101_PIN_CS,
        .gdo0 = CC1101_PIN_GDO0,
        .gdo2 = CC1101_PIN_GDO2,
    };
    return pins;
}
