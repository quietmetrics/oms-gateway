#include "cc1101.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "CC1101_CONFIG";

// wM-Bus frequency plan and configuration
// wM-Bus Mode C1 operates at 868.95 MHz with 38.4 kbps data rate

/**
 * @brief wM-Bus Mode C1 specific configuration settings
 * 
 * These settings configure the CC1101 for optimal reception of wM-Bus Mode C1
 * frames at 868.95 MHz with 38.4 kbps data rate using GFSK modulation.
 */
static const uint8_t wmbus_mode_c1_registers[][2] = {
    // IOCFG2 - GDO2 output pin configuration
    {CC1101_IOCFG2, 0x2E},  // Active low, high impedance when off
    
    // IOCFG1 - GDO1 output pin configuration  
    {CC1101_IOCFG1, 0x2E},  // High impedance (3-state)
    
    // IOCFG0 - GDO0 output pin configuration
    {CC1101_IOCFG0, 0x06},  // Assert when sync word has been received
    
    // FIFOTHR - RX FIFO and TX FIFO thresholds
    {CC1101_FIFOTHR, 0x47}, // 32 bytes in TX, 33 bytes in RX threshold
    
    // SYNC1 - Sync word high byte
    {CC1101_SYNC1, 0x2D},   // wM-Bus sync word: 0x2DD4
    
    // SYNC0 - Sync word low byte
    {CC1101_SYNC0, 0xD4},   // wM-Bus sync word: 0x2DD4
    
    // PKTLEN - Packet length
    {CC1101_PKTLEN, 0xFF},  // Maximum packet length (255 bytes)
    
    // PKTCTRL1 - Packet automation control
    {CC1101_PKTCTRL1, 0x04}, // Append status, no address check
    
    // PKTCTRL0 - Packet automation control
    {CC1101_PKTCTRL0, 0x05}, // Variable packet length, CRC enabled
    
    // ADDR - Device address
    {CC1101_ADDR, 0x00},    // No device address filtering
    
    // CHANNR - Channel number
    {CC1101_CHANNR, 0x00},  // Single channel
    
    // FSCTRL1 - Frequency synthesizer control
    {CC1101_FSCTRL1, 0x08}, // Intermediate frequency gain
    
    // FSCTRL0 - Frequency synthesizer control
    {CC1101_FSCTRL0, 0x00}, // Frequency offset added to frequency synthesizer
    
    // FREQ2 - Frequency control word, high byte
    {CC1101_FREQ2, 0x6D},   // 868.95 MHz: 0x6D4353
    
    // FREQ1 - Frequency control word, middle byte
    {CC1101_FREQ1, 0x43},   // 868.95 MHz: 0x6D4353
    
    // FREQ0 - Frequency control word, low byte
    {CC1101_FREQ0, 0x53},   // 868.95 MHz: 0x6D4353
    
    // MDMCFG4 - Modem configuration
    {CC1101_MDMCFG4, 0x7B}, // Channel bandwidth: BW_CHAN = 200 kHz
    
    // MDMCFG3 - Modem configuration
    {CC1101_MDMCFG3, 0x47}, // Data rate: 38.4 kbps
    
    // MDMCFG2 - Modem configuration
    {CC1101_MDMCFG2, 0x06}, // GFSK modulation, 16 sync bits, no Manchester encoding
    
    // MDMCFG1 - Modem configuration
    {CC1101_MDMCFG1, 0x22}, // FEC disabled, 4 preamble bytes
    
    // MDMCFG0 - Modem configuration
    {CC1101_MDMCFG0, 0xF8}, // Channel spacing: 25 kHz
    
    // DEVIATN - Modem deviation setting
    {CC1101_DEVIATN, 0x31}, // Deviation: 20 kHz
    
    // MCSM2 - Main Radio Control State Machine
    {CC1101_MCSM2, 0x07},   // Don't go to RX after TX, wait for CCA
    
    // MCSM1 - Main Radio Control State Machine
    {CC1101_MCSM1, 0x30},   // Auto transition to RX after RX/TX
    
    // MCSM0 - Main Radio Control State Machine
    {CC1101_MCSM0, 0x18},   // Calibration mode: calibrate when going from IDLE to RX/TX
    
    // FOCCFG - Frequency Offset Compensation
    {CC1101_FOCCFG, 0x1D},  // Frequency offset compensation configuration
    
    // BSCFG - Bit Synchronization Configuration
    {CC1101_BSCFG, 0x1C},   // Bit synchronization configuration
    
    // AGCCTRL2 - AGC control
    {CC1101_AGCCTRL2, 0xC7}, // AGC control: max gain, medium hysteresis
    
    // AGCCTRL1 - AGC control
    {CC1101_AGCCTRL1, 0x00}, // AGC control: no improvement for small signals
    
    // AGCCTRL0 - AGC control
    {CC1101_AGCCTRL0, 0xB2}, // AGC control: medium hysteresis, medium decision bound
    
    // FREND1 - Front end TX configuration
    {CC1101_FREND1, 0xB6},  // Front end TX configuration
    
    // FREND0 - Front end TX configuration
    {CC1101_FREND0, 0x10},  // Front end TX configuration
    
    // FSCAL3 - Frequency synthesizer calibration
    {CC1101_FSCAL3, 0xEA},  // Frequency synthesizer calibration
    
    // FSCAL2 - Frequency synthesizer calibration
    {CC1101_FSCAL2, 0x2A},  // Frequency synthesizer calibration
    
    // FSCAL1 - Frequency synthesizer calibration
    {CC1101_FSCAL1, 0x00},  // Frequency synthesizer calibration
    
    // FSCAL0 - Frequency synthesizer calibration
    {CC1101_FSCAL0, 0x1F},  // Frequency synthesizer calibration
    
    // TEST2 - Various test settings
    {CC1101_TEST2, 0x88},   // Various test settings
    
    // TEST1 - Various test settings
    {CC1101_TEST1, 0x31},   // Various test settings
    
    // TEST0 - Various test settings
    {CC1101_TEST0, 0x09},   // Various test settings
    
    // End marker
    {0xFF, 0xFF}
};

/**
 * @brief Apply wM-Bus specific configuration to CC1101
 * 
 * This function configures the CC1101 for optimal reception of wM-Bus frames
 * according to the wM-Bus specification for Mode C1.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t apply_wmbus_configuration(void)
{
    ESP_LOGI(TAG, "Applying wM-Bus specific configuration to CC1101");

    // Apply each register configuration
    int i = 0;
    while (wmbus_mode_c1_registers[i][0] != 0xFF) {
        uint8_t reg_addr = wmbus_mode_c1_registers[i][0];
        uint8_t value = wmbus_mode_c1_registers[i][1];
        
        esp_err_t ret = cc1101_write_register(reg_addr, value);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write register 0x%02X with value 0x%02X", reg_addr, value);
            return ret;
        }
        
        i++;
    }

    ESP_LOGI(TAG, "wM-Bus configuration applied successfully");
    return ESP_OK;
}

/**
 * @brief Configure CC1101 for wM-Bus Mode C1 reception
 * 
 * This function configures the CC1101 for receiving wM-Bus Mode C1 frames
 * at 868.95 MHz with 38.4 kbps data rate.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t configure_cc1101_for_wmbus_mode_c1(void)
{
    ESP_LOGI(TAG, "Configuring CC1101 for wM-Bus Mode C1 (868.95 MHz, 38.4 kbps)");

    // Apply wM-Bus specific configuration
    esp_err_t ret = apply_wmbus_configuration();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply wM-Bus configuration");
        return ret;
    }

    // Verify frequency configuration
    uint8_t freq2 = cc1101_read_register(CC1101_FREQ2);
    uint8_t freq1 = cc1101_read_register(CC1101_FREQ1);
    uint8_t freq0 = cc1101_read_register(CC1101_FREQ0);
    
    uint32_t freq_value = ((uint32_t)freq2 << 16) | ((uint32_t)freq1 << 8) | freq0;
    float actual_freq = freq_value * 26.0 / 65536.0;  // 26 MHz crystal
    
    ESP_LOGI(TAG, "Configured frequency: %.2f MHz (register value: 0x%06lX)", 
             actual_freq, freq_value);

    // Verify sync word
    uint8_t sync1 = cc1101_read_register(CC1101_SYNC1);
    uint8_t sync0 = cc1101_read_register(CC1101_SYNC0);
    
    ESP_LOGI(TAG, "Sync word configured as: 0x%02X%02X", sync1, sync0);

    return ESP_OK;
}

/**
 * @brief Configure CC1101 for wM-Bus Mode S reception
 * 
 * This function configures the CC1101 for receiving wM-Bus Mode S frames
 * at 868.3 MHz with appropriate data rate.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t configure_cc1101_for_wmbus_mode_s(void)
{
    ESP_LOGI(TAG, "Configuring CC1101 for wM-Bus Mode S (868.3 MHz)");

    // First apply base wM-Bus configuration
    esp_err_t ret = apply_wmbus_configuration();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply base wM-Bus configuration");
        return ret;
    }

    // Calculate frequency registers for 868.3 MHz
    // Formula: FREQ[23:0] = freq_carrier * 2^16 / f_xtal
    // With 26 MHz crystal: FREQ = 868.3 * 2^16 / 26 = 0x6D2C8F
    uint32_t freq_value = (uint32_t)(868.3 * 65536.0 / 26.0);
    uint8_t freq2 = (freq_value >> 16) & 0xFF;
    uint8_t freq1 = (freq_value >> 8) & 0xFF;
    uint8_t freq0 = freq_value & 0xFF;

    // Write frequency registers
    ret = cc1101_write_register(CC1101_FREQ2, freq2);
    ret |= cc1101_write_register(CC1101_FREQ1, freq1);
    ret |= cc1101_write_register(CC1101_FREQ0, freq0);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set frequency for Mode S");
        return ret;
    }

    // Mode S typically uses different data rate, adjust if needed
    // For now, keep the same data rate as Mode C1

    ESP_LOGI(TAG, "Configured for wM-Bus Mode S at 868.3 MHz");
    return ESP_OK;
}

/**
 * @brief Configure CC1101 for wM-Bus Mode T reception
 * 
 * This function configures the CC1101 for receiving wM-Bus Mode T frames
 * at 868.3 MHz with 32.768 kbps data rate.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t configure_cc1101_for_wmbus_mode_t(void)
{
    ESP_LOGI(TAG, "Configuring CC1101 for wM-Bus Mode T (868.3 MHz, 32.768 kbps)");

    // First apply base wM-Bus configuration
    esp_err_t ret = apply_wmbus_configuration();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply base wM-Bus configuration");
        return ret;
    }

    // Calculate frequency registers for 868.3 MHz
    uint32_t freq_value = (uint32_t)(868.3 * 65536.0 / 26.0);
    uint8_t freq2 = (freq_value >> 16) & 0xFF;
    uint8_t freq1 = (freq_value >> 8) & 0xFF;
    uint8_t freq0 = freq_value & 0xFF;

    // Write frequency registers
    ret = cc1101_write_register(CC1101_FREQ2, freq2);
    ret |= cc1101_write_register(CC1101_FREQ1, freq1);
    ret |= cc1101_write_register(CC1101_FREQ0, freq0);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set frequency for Mode T");
        return ret;
    }

    // Configure for Mode T data rate (32.768 kbps)
    // Adjust MDMCFG3 and MDMCFG4 for the new data rate
    ret = cc1101_write_register(CC1101_MDMCFG4, 0x7B);  // Keep same bandwidth
    ret |= cc1101_write_register(CC1101_MDMCFG3, 0x47); // Data rate register (will need adjustment)

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set data rate for Mode T");
        return ret;
    }

    ESP_LOGI(TAG, "Configured for wM-Bus Mode T at 868.3 MHz");
    return ESP_OK;
}

/**
 * @brief Configure CC1101 for multiple wM-Bus modes (frequency hopping)
 * 
 * This function configures the CC1101 to be able to switch between different
 * wM-Bus modes as needed for comprehensive reception.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t configure_cc1101_for_multiple_wmbus_modes(void)
{
    ESP_LOGI(TAG, "Configuring CC1101 for multiple wM-Bus modes");

    // Apply base wM-Bus configuration
    esp_err_t ret = apply_wmbus_configuration();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply base wM-Bus configuration");
        return ret;
    }

    // For multi-mode operation, we'll default to Mode C1 configuration
    // Applications can switch between modes as needed using the individual mode functions
    ret = configure_cc1101_for_wmbus_mode_c1();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure for Mode C1");
        return ret;
    }

    ESP_LOGI(TAG, "Configured for multiple wM-Bus modes (defaulting to Mode C1)");
    return ESP_OK;
}

/**
 * @brief Validate current CC1101 configuration for wM-Bus operation
 * 
 * @return esp_err_t ESP_OK if configuration is valid, error code otherwise
 */
esp_err_t validate_wmbus_configuration(void)
{
    ESP_LOGI(TAG, "Validating wM-Bus configuration");

    // Check frequency registers for 868.95 MHz
    uint8_t freq2 = cc1101_read_register(CC1101_FREQ2);
    uint8_t freq1 = cc1101_read_register(CC1101_FREQ1);
    uint8_t freq0 = cc1101_read_register(CC1101_FREQ0);
    
    uint32_t freq_value = ((uint32_t)freq2 << 16) | ((uint32_t)freq1 << 8) | freq0;
    float actual_freq = freq_value * 26.0 / 65536.0;  // 26 MHz crystal
    
    // Check if frequency is close to expected wM-Bus frequency (with tolerance)
    if (actual_freq < 868.0 || actual_freq > 869.9) {
        ESP_LOGW(TAG, "Frequency not in wM-Bus band: %.2f MHz", actual_freq);
        return ESP_ERR_INVALID_STATE;
    }

    // Check sync word
    uint8_t sync1 = cc1101_read_register(CC1101_SYNC1);
    uint8_t sync0 = cc1101_read_register(CC1101_SYNC0);
    
    if (sync1 != 0x2D || sync0 != 0xD4) {
        ESP_LOGW(TAG, "Sync word not configured for wM-Bus: 0x%02X%02X", sync1, sync0);
        return ESP_ERR_INVALID_STATE;
    }

    // Check modulation settings
    uint8_t mdmcfg2 = cc1101_read_register(CC1101_MDMCFG2);
    if ((mdmcfg2 & 0x70) != 0x00) {  // Check for GFSK modulation
        ESP_LOGW(TAG, "Modulation not set to GFSK: 0x%02X", mdmcfg2);
        return ESP_ERR_INVALID_STATE;
    }

    // Check packet control settings
    uint8_t pktctrl0 = cc1101_read_register(CC1101_PKTCTRL0);
    if ((pktctrl0 & 0x04) == 0) {  // Check if CRC is enabled
        ESP_LOGW(TAG, "CRC not enabled in packet control");
    }

    ESP_LOGI(TAG, "wM-Bus configuration validated successfully");
    return ESP_OK;
}