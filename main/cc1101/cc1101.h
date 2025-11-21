#ifndef CC1101_H
#define CC1101_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

// CC1101 SPI Configuration
#define CC1101_SPI_HOST SPI2_HOST
#define CC1101_SPI_CLOCK_SPEED_HZ (1000000) // 1 MHz for safe communication
#define CC1101_SPI_MODE 0

// CC1101 GPIO Configuration (adjust these according to your hardware setup)
#define CC1101_MOSI_GPIO 6
#define CC1101_MISO_GPIO 5
#define CC1101_SCLK_GPIO 4
#define CC1101_CS_GPIO 7
#define CC1101_GDO0_GPIO 2 // Used for data input/output
#define CC1101_GDO2_GPIO 3 // Used for status signals

// CC1101 Register Definitions
#define CC1101_IOCFG2 0x00         // GDO2 output pin configuration
#define CC1101_IOCFG1 0x01         // GDO1 output pin configuration
#define CC1101_IOCFG0 0x02         // GDO0 output pin configuration
#define CC1101_FIFOTHR 0x03        // RX FIFO and TX FIFO thresholds
#define CC1101_SYNC1 0x04          // Sync word, high byte
#define CC1101_SYNC0 0x05          // Sync word, low byte
#define CC1101_PKTLEN 0x06         // Packet length
#define CC1101_PKTCTRL1 0x07       // Packet automation control
#define CC1101_PKTCTRL0 0x08       // Packet automation control
#define CC1101_ADDR 0x09           // Device address
#define CC1101_CHANNR 0x0A         // Channel number
#define CC1101_FSCTRL1 0x0B        // Frequency synthesizer control
#define CC1101_FSCTRL0 0x0C        // Frequency synthesizer control
#define CC1101_FREQ2 0x0D          // Frequency control word, high byte
#define CC1101_FREQ1 0x0E          // Frequency control word, middle byte
#define CC1101_FREQ0 0x0F          // Frequency control word, low byte
#define CC1101_MDMCFG4 0x10        // Modem configuration
#define CC1101_MDMCFG3 0x11        // Modem configuration
#define CC1101_MDMCFG2 0x12        // Modem configuration
#define CC1101_MDMCFG1 0x13        // Modem configuration
#define CC1101_MDMCFG0 0x14        // Modem configuration
#define CC1101_DEVIATN 0x15        // Modem deviation setting
#define CC1101_MCSM2 0x16          // Main Radio Control State Machine
#define CC1101_MCSM1 0x17          // Main Radio Control State Machine
#define CC1101_MCSM0 0x18          // Main Radio Control State Machine
#define CC1101_FOCCFG 0x19         // Frequency Offset Compensation
#define CC1101_BSCFG 0x1A          // Bit Synchronization Configuration
#define CC1101_AGCCTRL2 0x1B       // AGC control
#define CC1101_AGCCTRL1 0x1C       // AGC control
#define CC1101_AGCCTRL0 0x1D       // AGC control
#define CC1101_WOREVT1 0x1E        // High byte Event 0 timeout
#define CC1101_WOREVT0 0x1F        // Low byte Event 0 timeout
#define CC1101_WORCTRL 0x20        // Wake On Radio control
#define CC1101_FREND1 0x21         // Front end TX configuration
#define CC1101_FREND0 0x22         // Front end TX configuration
#define CC1101_FSCAL3 0x23         // Frequency synthesizer calibration
#define CC1101_FSCAL2 0x24         // Frequency synthesizer calibration
#define CC1101_FSCAL1 0x25         // Frequency synthesizer calibration
#define CC1101_FSCAL0 0x26         // Frequency synthesizer calibration
#define CC1101_RCCTRL1 0x27        // RC oscillator configuration
#define CC1101_RCCTRL0 0x28        // RC oscillator configuration
#define CC1101_FSTEST 0x29         // Frequency synthesizer calibration control
#define CC1101_PTEST 0x2A          // Production test
#define CC1101_AGCTEST 0x2B        // AGC test
#define CC1101_TEST2 0x2C          // Various test settings
#define CC1101_TEST1 0x2D          // Various test settings
#define CC1101_TEST0 0x2E          // Various test settings
#define CC1101_PARTNUM 0x30        // Chip part number
#define CC1101_VERSION 0x31        // Chip version number
#define CC1101_FREQEST 0x32        // Frequency offset estimate
#define CC1101_LQI 0x33            // Demodulator estimate for link quality
#define CC1101_RSSI 0x34           // Received signal strength indication
#define CC1101_MARCSTATE 0x35      // Main Radio Control State Machine state
#define CC1101_WORTIME1 0x36       // High byte of WOR time
#define CC1101_WORTIME0 0x37       // Low byte of WOR time
#define CC1101_PKTSTATUS 0x38      // Current GDOx status and packet status
#define CC1101_VCO_VC_DAC 0x39     // Current setting from PLL calibration module
#define CC1101_TXBYTES 0x3A        // Underflow and number of bytes in TX FIFO
#define CC1101_RXBYTES 0x3B        // Overflow and number of bytes in RX FIFO
#define CC1101_RCCTRL1_STATUS 0x3C // Last RC oscillator calibration result
#define CC1101_RCCTRL0_STATUS 0x3D // Last RC oscillator calibration result
#define CC1101_RXFIFO 0x7F         // RX FIFO access address

// CC1101 Command Strobes
#define CC1101_SRES 0x30    // Reset chip
#define CC1101_SFSTXON 0x31 // Enable and calibrate frequency synthesizer
#define CC1101_SXOFF 0x32   // Turn off crystal oscillator
#define CC1101_SCAL 0x33    // Calibrate frequency synthesizer and turn it off
#define CC1101_SRX 0x34     // Enable RX
#define CC1101_STX 0x35     // Enable TX
#define CC1101_SIDLE 0x36   // Exit RX / TX
#define CC1101_SAFC 0x37    // AFC adjustment of frequency synthesizer
#define CC1101_SWOR 0x38    // Start automatic RX polling sequence
#define CC1101_SPWD 0x39    // Enter power down mode
#define CC1101_SFRX 0x3A    // Flush the RX FIFO buffer
#define CC1101_SFTX 0x3B    // Flush the TX FIFO buffer
#define CC1101_SWORRST 0x3C // Reset real time clock
#define CC1101_SNOP 0x3D    // No operation

// wM-Bus specific configurations
#define WM_BUS_FREQ_868_MHZ 868.95      // wM-Bus frequency in MHz
#define WM_BUS_DATA_RATE_38_KBAUD 38400 // 38.4 kbps for Mode C1

    // CC1101 Operating Modes
    typedef enum
    {
        CC1101_MODE_IDLE = 0,
        CC1101_MODE_RX,
        CC1101_MODE_TX,
        CC1101_MODE_FSTXON,
        CC1101_MODE_CALIBRATE,
        CC1101_MODE_POWER_DOWN
    } cc1101_mode_t;

    // CC1101 Initialization Status
    typedef enum
    {
        CC1101_INIT_SUCCESS = 0,
        CC1101_INIT_SPI_ERROR,
        CC1101_INIT_CHIP_NOT_FOUND,
        CC1101_INIT_CONFIG_ERROR
    } cc1101_init_status_t;

    // Structure to hold CC1101 configuration
    typedef struct
    {
        float frequency_mhz;     // Operating frequency in MHz
        uint32_t data_rate_bps;  // Data rate in bits per second
        uint8_t modulation_type; // Modulation type
        uint8_t sync_mode;       // Sync mode configuration
        uint8_t packet_length;   // Fixed packet length
        bool use_crc;            // Whether to use CRC
        bool append_status;      // Whether to append status bytes
    } cc1101_config_t;

    /**
     * @brief Initialize the CC1101 radio module
     *
     * This function initializes the SPI interface and configures the CC1101
     * with default settings for wM-Bus reception.
     *
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t initialize_cc1101(void);

    /**
     * @brief Configure RF parameters for wM-Bus operation
     *
     * Configures the CC1101 for 868.95MHz operation with appropriate
     * modulation settings for wM-Bus Mode C1 (38.4 kbps).
     *
     * @param config Pointer to configuration structure
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t configure_rf_parameters(const cc1101_config_t *config);

    /**
     * @brief Receive a packet from CC1101
     *
     * Reads a packet from the CC1101 RX FIFO. The function blocks until
     * a packet is received or a timeout occurs.
     *
     * @param buffer Buffer to store received packet data
     * @param buffer_size Size of the buffer
     * @param packet_size Pointer to store actual packet size received
     * @param timeout_ms Timeout in milliseconds
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t receive_packet(uint8_t *buffer, size_t buffer_size, size_t *packet_size, uint32_t timeout_ms);

    /**
     * @brief Set CC1101 operating mode
     *
     * Changes the operating mode of the CC1101 (RX, TX, IDLE, etc.)
     *
     * @param mode Desired operating mode
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t set_cc1101_mode(cc1101_mode_t mode);

    /**
     * @brief Read a register from CC1101
     *
     * @param reg_addr Register address to read from
     * @return uint8_t Register value
     */
    uint8_t cc1101_read_register(uint8_t reg_addr);

    /**
     * @brief Write a register to CC1101
     *
     * @param reg_addr Register address to write to
     * @param value Value to write
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t cc1101_write_register(uint8_t reg_addr, uint8_t value);

    /**
     * @brief Send a command strobe to CC1101
     *
     * @param strobe_cmd Strobe command to send
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t cc1101_send_command_strobe(uint8_t strobe_cmd);

    /**
     * @brief Read CC1101 RSSI value
     *
     * @return int8_t RSSI value in dBm
     */
    int8_t cc1101_read_rssi(void);

    /**
     * @brief Read CC1101 LQI value
     *
     * @return uint8_t Link Quality Indicator value
     */
    uint8_t cc1101_read_lqi(void);

#ifdef __cplusplus
}
#endif

#endif /* CC1101_H */