#include "app/wmbus/frame_parse.h"
#include <string.h>

#define WMBUS_FIRST_CI_OFFSET (WMBUS_FIXED_HEADER_BYTES - 1)

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
    case 0x7A:
    case 0x7B:
    case 0x6A:
    case 0x5A:
        return WMBUS_TPL_HDR_SHORT;
    }
    switch (wmbus_classify_ci_extended(ci))
    {
    case WMBUS_CI_CLASS_TPL:
    case WMBUS_CI_CLASS_DATA_MBUS:
    case WMBUS_CI_CLASS_DATA_OTHER:
    case WMBUS_CI_CLASS_NET_OR_TPL:
        return WMBUS_TPL_HDR_LONG;
    default:
        return WMBUS_TPL_HDR_NONE;
    }
}

static uint16_t read_le16(const uint8_t *buf, uint16_t offset, uint16_t len)
{
    if (!buf || offset + 1 >= len)
    {
        return 0;
    }
    return (uint16_t)buf[offset] | ((uint16_t)buf[offset + 1] << 8);
}

static uint32_t read_le32(const uint8_t *buf, uint16_t offset, uint16_t len)
{
    if (!buf || offset + 3 >= len)
    {
        return 0;
    }
    return (uint32_t)buf[offset] |
           ((uint32_t)buf[offset + 1] << 8) |
           ((uint32_t)buf[offset + 2] << 16) |
           ((uint32_t)buf[offset + 3] << 24);
}

static void decode_tpl_cfg(uint16_t cfg, const uint8_t *logical, uint16_t logical_len, uint16_t *payload_offset, wmbus_tpl_cfg_info_t *info)
{
    if (!payload_offset || !info)
    {
        return;
    }
    memset(info, 0, sizeof(*info));
    uint16_t offset = *payload_offset;
    const uint8_t cfg_low = cfg & 0xFF;
    const uint8_t cfg_high = (cfg >> 8) & 0xFF;
    uint8_t mode = cfg_high & 0x1F;
    info->mode = mode;

    switch (mode)
    {
    case 0: // Mode 0 mirrors mode 5 layout, but without encryption
    case 5:
    {
        info->content = (cfg_low >> 2) & 0x03;
        info->has_link_ctrl = true;
        info->link_b = (cfg_high >> 7) & 0x01;
        info->link_a = (cfg_high >> 6) & 0x01;
        info->link_s = (cfg_high >> 5) & 0x01;
        info->link_r = (cfg_low >> 1) & 0x01;
        info->link_h = cfg_low & 0x01;
        info->n_field = (cfg_low >> 4) & 0x0F;
        info->encrypted_length_bytes = (info->n_field == 0x0F) ? 0 : (uint16_t)info->n_field * 16;
        if (offset + 2 <= logical_len)
        {
            info->has_decryption_verification = true;
            info->dv_offset = offset;
            info->dv_len = 2;
            offset += 2;
        }
        break;
    }
    case 7:
    {
        info->content = (cfg_high >> 6) & 0x03;
        info->content_index = cfg_low & 0x07; // only 3 LSBs defined
        info->n_field = (cfg_low >> 4) & 0x0F;
        info->encrypted_length_bytes = (info->n_field == 0x0F) ? 0 : (uint16_t)info->n_field * 16;
        if (offset + 1 <= logical_len)
        {
            info->has_cfg_ext = true;
            info->cfg_ext_offset = offset;
            info->cfg_ext_len = 1;
            offset += 1;
        }
        if (offset + 2 <= logical_len)
        {
            info->has_decryption_verification = true;
            info->dv_offset = offset;
            info->dv_len = 2;
            offset += 2;
        }
        break;
    }
    case 10:
    {
        info->content = (cfg_high >> 6) & 0x03;
        const bool has_msg_counter = ((cfg_high >> 5) & 0x01) != 0;
        info->n_field = cfg_low;
        info->encrypted_length_bytes = cfg_low;
        if (has_msg_counter && offset + 4 <= logical_len)
        {
            info->has_message_counter = true;
            info->msg_counter_offset = offset;
            info->msg_counter = read_le32(logical, offset, logical_len);
            offset += 4;
        }
        if (offset + 2 <= logical_len)
        {
            const uint8_t ext0 = logical[offset];
            const uint8_t ext1 = logical[offset + 1];
            info->has_cfg_ext = true;
            info->cfg_ext_offset = offset;
            info->cfg_ext_len = 2;
            info->content_index = (ext0 >> 2) & 0x0F;
            info->has_key_version = ((ext1 >> 6) & 0x01) != 0;
            offset += 2;
            if (info->has_key_version && offset + 1 <= logical_len)
            {
                info->key_version_offset = offset;
                info->key_version = logical[offset];
                offset += 1;
            }
        }
        break;
    }
    case 13:
    {
        info->content = (cfg_high >> 6) & 0x03;
        info->n_field = cfg_low;
        info->encrypted_length_bytes = cfg_low;
        if (offset + 1 <= logical_len)
        {
            info->has_cfg_ext = true;
            info->cfg_ext_offset = offset;
            info->cfg_ext_len = 1;
            offset += 1;
        }
        break;
    }
    default:
        break;
    }

    *payload_offset = offset;
}

bool wmbus_parse_tpl_meta(uint8_t ci, uint16_t ci_offset, const uint8_t *logical, uint16_t logical_len, wmbus_tpl_meta_t *out)
{
    if (!logical || !out || logical_len <= WMBUS_FIRST_CI_OFFSET || ci_offset + 1 >= logical_len)
    {
        return false;
    }
    wmbus_tpl_meta_t m = {0};
    const wmbus_tpl_header_type_t hdr = classify_header(ci);
    if (hdr == WMBUS_TPL_HDR_NONE)
    {
        *out = m;
        return false;
    }
    const uint16_t idx_app = ci_offset + 1;
    const uint16_t header_bytes = (hdr == WMBUS_TPL_HDR_SHORT) ? 4 : 12;
    if (logical_len < idx_app + header_bytes)
    {
        *out = m;
        return false;
    }

    if (hdr == WMBUS_TPL_HDR_SHORT)
    {
        m.acc = logical[idx_app + 0];
        m.status = logical[idx_app + 1];
        m.cfg = read_le16(logical, idx_app + 2, logical_len);
        m.payload_offset = idx_app + 4;
    }
    else
    {
        m.acc = logical[idx_app + 8];
        m.status = logical[idx_app + 9];
        m.cfg = read_le16(logical, idx_app + 10, logical_len);
        m.payload_offset = idx_app + 12;
    }
    decode_tpl_cfg(m.cfg, logical, logical_len, &m.payload_offset, &m.cfg_info);
    if (logical_len > m.payload_offset)
    {
        m.payload_len = logical_len - m.payload_offset;
    }
    m.header_type = hdr;
    m.has_tpl = true;
    m.ci = ci;
    m.ci_offset = ci_offset;
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
    if (wmbus_classify_ci_extended(info->header.ci_field) != WMBUS_CI_CLASS_ELL)
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
    m.next_offset = idx_app + 2;
    m.payload_offset = m.next_offset;
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
        m.next_offset = idx_app + 10;
        m.payload_offset = m.next_offset;
    }
    if (logical_len > m.payload_offset)
    {
        m.payload_len = logical_len - m.payload_offset;
    }
    m.has_ell = true;
    *out = m;
    return true;
}

bool wmbus_derive_security_meta(const wmbus_tpl_meta_t *tpl, const wmbus_ell_meta_t *ell, const wmbus_afl_meta_t *afl, uint8_t *out_mode, uint16_t *out_len, bool *out_encrypted)
{
    if (!tpl || !out_mode || !out_len || !out_encrypted)
    {
        return false;
    }
    uint8_t mode = WMBUS_SEC_MODE_NONE;
    uint16_t enc_len = 0;
    bool enc = false;

    if (ell && ell->has_ell)
    {
        enc = true;
        mode = WMBUS_SEC_MODE_TUNNEL;
    }
    if (afl && afl->has_afl)
    {
        enc = true;
        mode = WMBUS_SEC_MODE_TUNNEL;
    }
    if (!enc && tpl && tpl->has_tpl)
    {
        if (tpl->cfg_info.mode != 0)
        {
            mode = tpl->cfg_info.mode;
            enc = true;
        }
        else if (tpl->cfg != 0)
        {
            mode = WMBUS_SEC_MODE_CFG_PRESENT;
            enc = true;
        }
    }

    // Choose best-known encrypted length: prefer AFL, then ELL, then TPL payload.
    if (afl && afl->has_afl && afl->payload_len > 0)
    {
        enc_len = afl->payload_len;
    }
    else if (ell && ell->has_ell && ell->payload_len > 0)
    {
        enc_len = ell->payload_len;
    }
    else if (tpl && tpl->has_tpl)
    {
        if (tpl->cfg_info.encrypted_length_bytes > 0)
        {
            enc_len = tpl->cfg_info.encrypted_length_bytes;
        }
        else if (tpl->payload_len > 0)
        {
            enc_len = tpl->payload_len;
        }
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
    const uint16_t header_start = afl_offset + 2;
    if (m.afll > 0 && header_start < logical_len)
    {
        m.mcl = logical[header_start];
    }
    uint16_t payload_off = header_start + m.afll;
    m.header_end_offset = payload_off;
    if (payload_off <= logical_len)
    {
        m.payload_offset = payload_off;
        m.payload_len = logical_len - payload_off;
    }
    *out = m;
    return true;
}
