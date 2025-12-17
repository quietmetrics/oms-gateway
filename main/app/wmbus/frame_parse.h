// Layered parsing helpers for wM-Bus / OMS frames
// Provides a small Transport Layer (TPL) view independent of application payload parsing.
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "wmbus/packet.h"

// CI-classification to keep parsing modular.
typedef enum
{
    WMBUS_CI_CLASS_UNKNOWN = 0,
    WMBUS_CI_CLASS_TPL,          // Standard OMS/EN13757 TPL headers (short/long/none)
    WMBUS_CI_CLASS_DATA_MBUS,    // M-Bus data (DIF/VIF) frames
    WMBUS_CI_CLASS_DATA_OTHER,   // DLMS/SML or other app protocols
    WMBUS_CI_CLASS_ELL,          // Extended Link Layer (ELL)
    WMBUS_CI_CLASS_AFL,          // Authentication & Fragmentation Layer (0x90)
    WMBUS_CI_CLASS_NET_OR_TPL,   // Network / pure transport mgmt
    WMBUS_CI_CLASS_SECURITY_MGMT,// Security control (5F/9E/9F)
    WMBUS_CI_CLASS_LINK_CTRL,    // Link control (baud rate etc.)
    WMBUS_CI_CLASS_IMAGE_OR_INFO,// Image/security info (C0..C7/CF)
    WMBUS_CI_CLASS_MBAL          // MBAL (CF)
} wmbus_ci_class_t;

// TPL header type derived from CI-field
typedef enum
{
    WMBUS_TPL_HDR_NONE = 0,  // No TPL header after CI
    WMBUS_TPL_HDR_SHORT = 1, // Short TPL header (ACC/Status/Config, 4 bytes)
    WMBUS_TPL_HDR_LONG = 2   // Long TPL header (repeated addr + ACC/Status/Config, 12 bytes)
} wmbus_tpl_header_type_t;

typedef struct
{
    uint8_t mode;                    // Security mode parsed from CFG (0/5/7/10/13)
    uint8_t content;                 // Content bits (CC) when available
    uint8_t content_index;           // Content index (IIII) when available
    uint8_t n_field;                 // Raw N bits (blocks or bytes depending on mode)
    uint16_t encrypted_length_bytes; // Derived encrypted length if N encodes size
    bool has_link_ctrl;              // True when B/A/S/R/H bits were decoded
    bool link_b;
    bool link_a;
    bool link_s;
    bool link_r;
    bool link_h;
    bool has_cfg_ext;                // True when a configuration extension exists
    uint16_t cfg_ext_offset;         // Offset of the configuration extension
    uint8_t cfg_ext_len;             // Length of configuration extension bytes
    bool has_decryption_verification;// True when mode 0/5/7 appended 2F 2F trailer
    uint16_t dv_offset;              // Offset of the decryption verification field
    uint8_t dv_len;                  // Length of DV field (usually 2)
    bool has_message_counter;        // True when a TPL message counter was parsed (mode 10)
    uint16_t msg_counter_offset;     // Offset of the message counter
    uint32_t msg_counter;            // Parsed message counter value
    bool has_key_version;            // True when a KeyVersion byte followed the CFE (mode 10)
    uint16_t key_version_offset;     // Offset of the KeyVersion byte
    uint8_t key_version;             // Parsed KeyVersion value
} wmbus_tpl_cfg_info_t;

typedef struct
{
    wmbus_tpl_header_type_t header_type; // Parsed header classification
    bool has_tpl;                        // True if a known TPL header was parsed
    uint8_t ci;                          // CI byte that introduced this TPL (may differ from DLL CI)
    uint16_t ci_offset;                  // Offset of the CI byte within the logical frame
    uint8_t acc;                         // Access number (if has_tpl)
    uint8_t status;                      // Status byte (if has_tpl)
    uint16_t cfg;                        // Config/Signature field (if has_tpl)
    uint16_t payload_offset;             // Offset to payload start after TPL header (0 if unknown)
    uint16_t payload_len;                // Remaining bytes after header (0 if unknown)
    wmbus_tpl_cfg_info_t cfg_info;       // Decoded configuration/meta information
} wmbus_tpl_meta_t;

// Coarse security mode hints
typedef enum
{
    WMBUS_SEC_MODE_NONE = 0,           // No encryption indicated
    WMBUS_SEC_MODE_CFG_PRESENT = 250,  // Config/signature present, mode not decoded
    WMBUS_SEC_MODE_TUNNEL = 251        // ELL/AFL tunnel (LMN/TLS)
} wmbus_sec_mode_t;

typedef struct
{
    bool has_ell;    // True if CI indicates ELL and header parsed
    uint8_t cc;      // ELL Control Code
    uint8_t acc;     // ELL Access Counter
    uint8_t ext[8];  // Optional ELL extension bytes (0 for CI 0x8C)
    uint8_t ext_len; // 0 or 8
    uint16_t next_offset; // offset after the ELL header (start of next layer/tag)
    uint16_t payload_offset; // same as next_offset; first byte after ELL header
    uint16_t payload_len;    // bytes after ELL header (0 if unknown)
} wmbus_ell_meta_t;

typedef struct
{
    bool has_afl;      // True if AFL detected (top-level CI 0x90 or within ELL)
    uint8_t tag;       // AFL tag (typically 0x90)
    uint8_t afll;      // AFL header length (AFLL field) if present
    uint16_t offset;   // Offset in logical frame where AFL starts
    uint8_t mcl;       // AFL.MCL byte when available
    uint16_t header_end_offset; // Offset right after the AFL header (start of next CI)
    uint16_t payload_offset; // Offset where encrypted payload starts (after AFL header)
    uint16_t payload_len;    // Remaining bytes after AFL header
} wmbus_afl_meta_t;

// Derive coarse security meta from tpl/ell/afl (best-effort; mode 0 = unencrypted).
// Returns true if the fields were filled with a meaningful guess.
bool wmbus_derive_security_meta(const wmbus_tpl_meta_t *tpl, const wmbus_ell_meta_t *ell, const wmbus_afl_meta_t *afl, uint8_t *out_mode, uint16_t *out_len, bool *out_encrypted);

wmbus_ci_class_t wmbus_classify_ci_extended(uint8_t ci);
// Parse Transport-Layer meta information (ACC/Status/Config) for the CI located at ci_offset.
// - frame must be CRC-free logical bytes (starts at L).
// Returns true when TPL meta was parsed (has_tpl set), false if CI is unknown or length insufficient.
bool wmbus_parse_tpl_meta(uint8_t ci, uint16_t ci_offset, const uint8_t *logical, uint16_t logical_len, wmbus_tpl_meta_t *out);
// Parse Extended Link Layer meta (CI 0x8C/0x8E). Returns true if parsed.
bool wmbus_parse_ell_meta(const WmbusFrameInfo *info, const uint8_t *logical, uint16_t logical_len, wmbus_ell_meta_t *out);
// Parse AFL meta (CI 0x90 or nested after ELL). Provide start offset to AFL.
bool wmbus_parse_afl_meta(const uint8_t *logical, uint16_t logical_len, uint16_t afl_offset, wmbus_afl_meta_t *out);
