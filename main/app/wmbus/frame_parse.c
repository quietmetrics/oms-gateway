#include "app/wmbus/frame_parse.h"
#include <string.h>

wmbus_ci_class_t wmbus_classify_ci(uint8_t ci)
{
    switch (ci)
    {
    case 0x72:
    case 0x73:
    case 0x6B:
    case 0x5B:
    case 0x7A:
    case 0x7B:
    case 0x6A:
    case 0x5A:
        return WMBUS_CI_CLASS_TPL;
    case 0x8C:
    case 0x8E:
        return WMBUS_CI_CLASS_ELL;
    default:
        return WMBUS_CI_CLASS_UNKNOWN;
    }
}

wmbus_ci_class_t wmbus_classify_ci_extended(uint8_t ci)
{
    // Based on DecodingGuide CI grouping (uplink focus)
    switch (ci)
    {
    // M-Bus data frames (full/compact/format)
    case 0x72: case 0x7A: case 0x78:
    case 0x73: case 0x7B: case 0x79:
    case 0x6B: case 0x6A: case 0x69:
    case 0x5A: case 0x5B: // commands (downlink but keep class)
    case 0x6E: case 0x6F: case 0x70:
    case 0x71: case 0x74: case 0x75:
        return WMBUS_CI_CLASS_DATA_MBUS;
    // DLMS / SML (other protocols)
    case 0x60: case 0x61: case 0x7C: case 0x7D:
    case 0x64: case 0x65: case 0x7E: case 0x7F:
        return WMBUS_CI_CLASS_DATA_OTHER;
    // Extended link / AFL / security mgmt
    case 0x8C: case 0x8E:
        return WMBUS_CI_CLASS_ELL;
    case 0x90:
        return WMBUS_CI_CLASS_AFL;
    case 0x5F: case 0x9E: case 0x9F:
        return WMBUS_CI_CLASS_SECURITY_MGMT;
    // Network / pure transport management
    case 0x80: case 0x82: case 0x87: case 0x88: case 0x8A: case 0x8B:
    case 0x92: case 0x93:
        return WMBUS_CI_CLASS_NET_OR_TPL;
    // Link control / baud rate
    case 0xB8: case 0xBB: case 0xBD: case 0xBE: case 0xBF:
        return WMBUS_CI_CLASS_LINK_CTRL;
    // Image / security info / MBAL
    case 0xC0: case 0xC1: case 0xC2: case 0xC3:
    case 0xC4: case 0xC5: case 0xC6: case 0xC7:
        return WMBUS_CI_CLASS_IMAGE_OR_INFO;
    case 0xCF:
        return WMBUS_CI_CLASS_MBAL;
    default:
        return WMBUS_CI_CLASS_UNKNOWN;
    }
}

// CI → TPL header classification
static wmbus_tpl_header_type_t classify_header(uint8_t ci)
{
    switch (ci)
    {
    case 0x72:
    case 0x73:
    case 0x6B:
    case 0x5B:
        return WMBUS_TPL_HDR_LONG;
    case 0x7A:
    case 0x7B:
    case 0x6A:
    case 0x5A:
        return WMBUS_TPL_HDR_SHORT;
    default:
        return WMBUS_TPL_HDR_NONE;
    }
}

bool wmbus_parse_tpl_meta(const WmbusFrameInfo *info, const uint8_t *logical, uint16_t logical_len, wmbus_tpl_meta_t *out)
{
    if (!info || !logical || !out || logical_len < 12)
    {
        return false;
    }
    wmbus_tpl_meta_t m = {0};
    if (wmbus_classify_ci(info->header.ci_field) != WMBUS_CI_CLASS_TPL)
    {
        *out = m;
        return false;
    }
    const wmbus_tpl_header_type_t hdr = classify_header(info->header.ci_field);
    if (hdr == WMBUS_TPL_HDR_NONE)
    {
        *out = m;
        return false;
    }

    // Fixed offset: L + C + M(2) + ID(4) + Ver + Dev + CI = 11 bytes
    const uint16_t idx_app = 11;
    uint16_t need = idx_app + ((hdr == WMBUS_TPL_HDR_SHORT) ? 4 : 12);
    if (logical_len < need)
    {
        *out = m;
        return false;
    }

    if (hdr == WMBUS_TPL_HDR_SHORT)
    {
        m.acc = logical[idx_app + 0];
        m.status = logical[idx_app + 1];
        m.cfg = (uint16_t)logical[idx_app + 2] | ((uint16_t)logical[idx_app + 3] << 8);
    }
    else
    {
        m.acc = logical[idx_app + 8];
        m.status = logical[idx_app + 9];
        m.cfg = (uint16_t)logical[idx_app + 10] | ((uint16_t)logical[idx_app + 11] << 8);
    }
    m.header_type = hdr;
    m.has_tpl = true;
    *out = m;
    return true;
}

bool wmbus_parse_ell_meta(const WmbusFrameInfo *info, const uint8_t *logical, uint16_t logical_len, wmbus_ell_meta_t *out)
{
    if (!info || !logical || !out)
    {
        return false;
    }
    wmbus_ell_meta_t m = {0};
    if (wmbus_classify_ci(info->header.ci_field) != WMBUS_CI_CLASS_ELL)
    {
        *out = m;
        return false;
    }
    const uint16_t idx_app = 11; // after CI
    if (logical_len < idx_app + 2)
    {
        *out = m;
        return false;
    }

    m.cc = logical[idx_app + 0];
    m.acc = logical[idx_app + 1];
    if (info->header.ci_field == 0x8E)
    {
        if (logical_len < idx_app + 10)
        {
            *out = m;
            return false;
        }
        m.ext_len = 8;
        for (int i = 0; i < 8; i++)
        {
            m.ext[i] = logical[idx_app + 2 + i];
        }
    }
    m.has_ell = true;
    *out = m;
    return true;
}

bool wmbus_derive_security_meta(const wmbus_tpl_meta_t *tpl, const wmbus_ell_meta_t *ell, uint8_t *out_mode, uint16_t *out_len, bool *out_encrypted)
{
    if (!tpl || !out_mode || !out_len || !out_encrypted)
    {
        return false;
    }
    // Best-effort heuristics:
    // - If ELL present, treat as encrypted tunnel.
    // - If TPL parsed and cfg != 0, assume encrypted and mode unknown (0).
    // - Otherwise mode=0, encrypted=false.
    uint8_t mode = 0;
    uint16_t enc_len = 0;
    bool enc = false;

    if (ell && ell->has_ell)
    {
        enc = true;
    }
    else if (tpl && tpl->has_tpl && tpl->cfg != 0)
    {
        enc = true;
    }

    *out_mode = mode;
    *out_len = enc_len;
    *out_encrypted = enc;
    return true;
}

bool wmbus_parse_afl_meta(const uint8_t *logical, uint16_t logical_len, uint16_t afl_offset, wmbus_afl_meta_t *out)
{
    if (!logical || !out || afl_offset >= logical_len)
    {
        return false;
    }
    if (logical[afl_offset] != 0x90)
    {
        return false;
    }
    if (afl_offset + 2 > logical_len)
    {
        return false;
    }
    wmbus_afl_meta_t m = {0};
    m.has_afl = true;
    m.tag = logical[afl_offset];
    m.afll = logical[afl_offset + 1];
    m.offset = afl_offset;
    uint16_t payload_off = afl_offset + 2 + m.afll;
    if (payload_off <= logical_len)
    {
        m.payload_offset = payload_off;
        m.payload_len = logical_len - payload_off;
    }
    *out = m;
    return true;
}
