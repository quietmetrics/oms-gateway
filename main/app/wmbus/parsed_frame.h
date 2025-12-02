// Parsed frame representation with hooks for decryption/APL parsing.
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "wmbus/packet.h"
#include "app/wmbus/frame_parse.h"
#include "app/config.h"
#include <string.h>

// Raw view (CRC-free logical frame)
typedef struct
{
    const uint8_t *bytes;   // points to CRC-free logical packet (starts at L)
    uint16_t len;           // length of logical packet (L+1)
} wmbus_raw_frame_t;

typedef struct
{
    bool encrypted;       // best-effort: true if security_mode != 0 or cfg/signature indicates encryption
    uint8_t security_mode; // coarse mode hint: NONE / CFG_PRESENT / TUNNEL (see wmbus_sec_mode_t)
    uint16_t enc_len;     // number of encrypted bytes/blocks if known
} wmbus_sec_meta_t;

// DLL + Address layer (always present after CRC strip)
typedef struct
{
    uint8_t l;
    uint8_t c;
    uint8_t ci;
    uint16_t manuf;
    uint8_t id[4];
    char id_str[9];
    uint8_t version;
    uint8_t dev_type;
    char gateway[APP_HOSTNAME_MAX];
} wmbus_layer_dll_t;

typedef struct
{
    bool has_tpl;
    wmbus_tpl_meta_t tpl; // header_type, acc/status/cfg
    wmbus_sec_meta_t sec; // mode/len/encrypted (derived)
} wmbus_layer_tpl_t;

typedef struct
{
    bool has_ell;
    wmbus_ell_meta_t ell; // CC/ACC/EXT + next_offset
} wmbus_layer_ell_t;

typedef struct
{
    bool has_afl;
    wmbus_afl_meta_t afl; // tag, afll, offsets
} wmbus_layer_afl_t;

typedef struct
{
    bool has_apl;
    // Placeholder for DIF/VIF or other app-layer parsing.
} wmbus_layer_apl_t;

typedef struct
{
    wmbus_raw_frame_t raw;
    WmbusFrameInfo info;
    wmbus_ci_class_t ci_class;
    wmbus_layer_dll_t dll;
    wmbus_layer_tpl_t tpl;
    wmbus_layer_ell_t ell;
    wmbus_layer_afl_t afl;
    wmbus_layer_apl_t apl;
    bool encrypted;
} wmbus_parsed_frame_t;

// Decrypt callback; returns true if decryption was applied into-place on raw.bytes or a scratch buffer.
typedef bool (*wmbus_decrypt_fn)(const wmbus_parsed_frame_t *frame, uint8_t *mutable_buf, uint16_t buf_len, void *user);

// Initialize parsed frame with raw + info
void wmbus_parsed_frame_init(wmbus_parsed_frame_t *f, const wmbus_raw_frame_t *raw, const WmbusFrameInfo *info);
// Parse TPL/ELL/AFL/Security meta into the parsed frame
void wmbus_parsed_frame_parse_meta(wmbus_parsed_frame_t *f);
