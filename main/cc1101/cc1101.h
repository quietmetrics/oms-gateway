#pragma once

#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"
#include <stdint.h>

// CC1101 command strobes
#define CC1101_SRES 0x30
#define CC1101_SFSTXON 0x31
#define CC1101_SXOFF 0x32
#define CC1101_SCAL 0x33
#define CC1101_SRX 0x34
#define CC1101_STX 0x35
#define CC1101_SIDLE 0x36
#define CC1101_SWOR 0x38
#define CC1101_SPWD 0x39
#define CC1101_SFRX 0x3A
#define CC1101_SFTX 0x3B
#define CC1101_SWORRST 0x3C
#define CC1101_SNOP 0x3D

// Registers
#define CC1101_IOCFG2 0x00
#define CC1101_IOCFG1 0x01
#define CC1101_IOCFG0 0x02
#define CC1101_FIFOTHR 0x03
#define CC1101_SYNC1 0x04
#define CC1101_SYNC0 0x05
#define CC1101_PKTLEN 0x06
#define CC1101_PKTCTRL1 0x07
#define CC1101_PKTCTRL0 0x08
#define CC1101_ADDR 0x09
#define CC1101_CHANNR 0x0A
#define CC1101_FSCTRL1 0x0B
#define CC1101_FSCTRL0 0x0C
#define CC1101_FREQ2 0x0D
#define CC1101_FREQ1 0x0E
#define CC1101_FREQ0 0x0F
#define CC1101_MDMCFG4 0x10
#define CC1101_MDMCFG3 0x11
#define CC1101_MDMCFG2 0x12
#define CC1101_MDMCFG1 0x13
#define CC1101_MDMCFG0 0x14
#define CC1101_DEVIATN 0x15
#define CC1101_MCSM2 0x16
#define CC1101_MCSM1 0x17
#define CC1101_MCSM0 0x18
#define CC1101_FOCCFG 0x19
#define CC1101_BSCFG 0x1A
#define CC1101_AGCCTRL2 0x1B
#define CC1101_AGCCTRL1 0x1C
#define CC1101_AGCCTRL0 0x1D
#define CC1101_WOREVT1 0x1E
#define CC1101_WOREVT0 0x1F
#define CC1101_WORCTRL 0x20
#define CC1101_FREND1 0x21
#define CC1101_FREND0 0x22
#define CC1101_FSCAL3 0x23
#define CC1101_FSCAL2 0x24
#define CC1101_FSCAL1 0x25
#define CC1101_FSCAL0 0x26
#define CC1101_RCCTRL1 0x27
#define CC1101_RCCTRL0 0x28
#define CC1101_FSTEST 0x29
#define CC1101_PTEST 0x2A
#define CC1101_AGCTEST 0x2B
#define CC1101_TEST2 0x2C
#define CC1101_TEST1 0x2D
#define CC1101_TEST0 0x2E
#define CC1101_RXBYTES 0x3B
#define CC1101_RSSI 0x34
#define CC1101_LQI 0x33
#define CC1101_MARCSTATE 0x35
#define CC1101_PKTSTATUS 0x38
#define CC1101_RXFIFO 0x3F

// SPI read/write masks
#define CC1101_WRITE_BURST 0x40
#define CC1101_READ_SINGLE 0x80
#define CC1101_READ_BURST 0xC0

typedef struct {
    spi_host_device_t host;
    gpio_num_t pin_miso;
    gpio_num_t pin_mosi;
    gpio_num_t pin_sclk;
    gpio_num_t pin_cs;
    gpio_num_t pin_gdo0;
    gpio_num_t pin_gdo2;
} cc1101_pins_t;

typedef struct {
    spi_device_handle_t spi;
    cc1101_pins_t pins;
    SemaphoreHandle_t lock;
} cc1101_handle_t;

typedef struct {
    // MDMCFG2: Sync mode / modulation specifics. Default 0x05 (16/16 sync, T-mode).
    uint8_t mdmcfg2;
    // PKTCTRL1: Preamble quality threshold (PQT bits 7:5) etc. Default 0x00 (PQT disabled).
    uint8_t pktctrl1;
    // MDMCFG4: RX channel bandwidth / DRATE_E. Default 0x5C (~150 kHz BW for T-mode).
    uint8_t mdmcfg4;
    // AGCCTRL1: Carrier sense / AGC wait time. Default 0x09 (example lib).
    uint8_t agcctrl1;
    // AGCCTRL2: AGC target/step sizes. Default 0x43 (example lib).
    uint8_t agcctrl2;
} cc1101_filter_cfg_t;

#define CC1101_FILTER_CFG_DEFAULT   \
    (cc1101_filter_cfg_t) {         \
        .mdmcfg2 = 0x05,            \
        .pktctrl1 = 0x00,           \
        .mdmcfg4 = 0x5C,            \
        .agcctrl1 = 0x09,           \
        .agcctrl2 = 0x43,           \
    }

esp_err_t cc1101_init(const cc1101_pins_t *pins, cc1101_handle_t *out);
esp_err_t cc1101_init_registers(cc1101_handle_t *handle, const cc1101_filter_cfg_t *filter_cfg);
// Apply new filter/modem settings at runtime: caller should ensure RX is idle/paused.
esp_err_t cc1101_set_filter(cc1101_handle_t *handle, const cc1101_filter_cfg_t *filter_cfg);
esp_err_t cc1101_enter_rx(cc1101_handle_t *handle);
esp_err_t cc1101_idle(cc1101_handle_t *handle);
esp_err_t cc1101_read_rssi_lqi(cc1101_handle_t *handle, float *rssi_dbm, uint8_t *lqi);
esp_err_t cc1101_get_status(cc1101_handle_t *handle, uint8_t *status_out);
esp_err_t cc1101_strobe(cc1101_handle_t *handle, uint8_t cmd);
esp_err_t cc1101_read_burst(cc1101_handle_t *handle, uint8_t reg, uint8_t *buf, size_t len);
esp_err_t cc1101_write_reg(cc1101_handle_t *handle, uint8_t reg, uint8_t value);
esp_err_t cc1101_read_reg(cc1101_handle_t *handle, uint8_t reg, uint8_t *out);
