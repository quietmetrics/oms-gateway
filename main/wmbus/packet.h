// Wireless M-Bus packet utilities (T-mode only)
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define WMBUS_PACKET_C_FIELD  0x44
#define WMBUS_MAN_CODE        0x0CAE
#define WMBUS_MAN_NUMBER      0x12345678
#define WMBUS_MAN_ID          0x01
#define WMBUS_MAN_VER         0x07
#define WMBUS_PACKET_CI_FIELD 0x78

#define WMBUS_PKT_OK            0
#define WMBUS_PKT_CODING_ERROR  1
#define WMBUS_PKT_CRC_ERROR     2

// Bytes counted by the L-field before any application payload:
// C + M(2) + ID(4) + version + device type + CI = 10 bytes
#define WMBUS_L_FIELD_FIXED_BYTES 10
#define WMBUS_FIXED_HEADER_BYTES  (WMBUS_L_FIELD_FIXED_BYTES + 1) // L + fixed header fields (no CRC)

/*
 * Logical representation of the unencrypted wireless M-Bus / OMS link-layer header.
 * The struct models the bytes as they appear on air after preamble/sync removal
 * and with CRC bytes stripped.
 *
 * Layout (Format A, common for OMS C1/T1):
 *
 *   L | C | M (2B LE) | ID (4B BCD LSB) | Version | Device type | CI | payload…
 *   1 | 1 |    2      |        4        |    1    |      1      | 1  |   n
 */
typedef struct
{
    uint8_t length;          // L-field: number of bytes after this field (CRC excluded)
    uint8_t control;         // C-field: link-layer control (e.g., 0x44 = SND-NR)
    uint16_t manufacturer_le;// M-field in little-endian as transmitted on air (DIN: Manufacturer ID)
    uint8_t id[4];           // Identification number (BCD-coded, least significant byte first) (DIN: Fabrication Number)
    uint8_t version;         // Version / production code (DIN: Fabrication Block)
    uint8_t device_type;     // Device type / medium (maps to OBIS category per EN 13757-7/OMS)
    uint8_t ci_field;        // CI-field selecting the application-layer format
    uint8_t payload[];       // Remainder of frame (access number/status/security/application data)
} WmbusFrameHeaderRaw;

uint16_t wmbus_packet_size(uint8_t l_field);
uint16_t wmbus_byte_size_tmode(bool transmit, uint16_t packet_size);

void wmbus_encode_tx_packet(uint8_t *packet, const uint8_t *data, uint8_t data_size);
void wmbus_encode_tx_packet_with_header(uint8_t *packet, const WmbusFrameHeaderRaw *header, const uint8_t *data, uint8_t data_size);
void wmbus_encode_tx_bytes_tmode(uint8_t *encoded, const uint8_t *packet, uint16_t packet_size);
uint16_t wmbus_decode_rx_bytes_tmode(const uint8_t *encoded, uint8_t *packet, uint16_t packet_size);

// Copy a decoded on-air packet (with CRC bytes) into a logical layout without CRCs.
// Returns the number of bytes written (should be header->length + 1) or 0 on error.
uint16_t wmbus_strip_crc_blocks(const uint8_t *packet_with_crc, uint16_t packet_with_crc_len, uint8_t *packet_no_crc, uint16_t packet_no_crc_capacity);

// Parse the fixed header fields from a CRC-free packet (as produced by wmbus_strip_crc_blocks).
// Optionally returns payload pointer/length when provided.
bool wmbus_parse_frame_header(const uint8_t *packet_no_crc, uint16_t packet_len, WmbusFrameHeaderRaw *out_header, const uint8_t **payload, uint16_t *payload_len);

// Populate a header struct with the default/demo values used by the example encoder.
void wmbus_build_default_header(WmbusFrameHeaderRaw *header, uint8_t payload_len);

typedef struct
{
    bool parsed;
    uint16_t logical_len; // L+1 bytes after CRCs are removed
    uint16_t payload_len;
    WmbusFrameHeaderRaw header;
} WmbusFrameInfo;

// Convenience: strip CRCs and parse the fixed header in one call.
// Returns true on successful parse, false otherwise. The scratch buffer must
// hold at least packet_len bytes.
bool wmbus_extract_frame_info(const uint8_t *packet, uint16_t packet_len, uint8_t *scratch, uint16_t scratch_len, WmbusFrameInfo *info);
