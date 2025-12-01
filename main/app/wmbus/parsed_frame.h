// Parsed frame representation with hooks for decryption/APL parsing.
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "wmbus/packet.h"
#include "app/wmbus/frame_parse.h"

// Raw view (CRC-free logical frame)
typedef struct
{
    const uint8_t *bytes;   // points to CRC-free logical packet (starts at L)
    uint16_t len;           // length of logical packet (L+1)
} wmbus_raw_frame_t;

typedef struct
{
    bool encrypted;       // best-effort: true if security_mode != 0 or cfg/signature indicates encryption
    uint8_t security_mode; // optional security mode (0/5/7/10/13) if derivable, else 0
    uint16_t enc_len;     // number of encrypted bytes/blocks if known
} wmbus_sec_meta_t;

typedef struct
{
    WmbusFrameInfo info;      // parsed DLL+TPL header from pipeline
    wmbus_raw_frame_t raw;    // logical raw view
    wmbus_tpl_meta_t tpl;     // parsed TPL meta (short/long)
    wmbus_ell_meta_t ell;     // parsed ELL meta
    wmbus_sec_meta_t sec;     // coarse security meta
    // TODO: APL parsing can be added later when decrypt is available.
} wmbus_parsed_frame_t;

// Decrypt callback; returns true if decryption was applied into-place on raw.bytes or a scratch buffer.
typedef bool (*wmbus_decrypt_fn)(const wmbus_parsed_frame_t *frame, uint8_t *mutable_buf, uint16_t buf_len, void *user);

