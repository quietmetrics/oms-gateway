#include "app/wmbus/parsed_frame.h"
#include <stdio.h>

void wmbus_parsed_frame_init(wmbus_parsed_frame_t *f, const wmbus_raw_frame_t *raw, const WmbusFrameInfo *info)
{
    if (!f || !raw || !info)
    {
        return;
    }
    memset(f, 0, sizeof(*f));
    f->raw = *raw;
    f->info = *info;
    f->ci_class = wmbus_classify_ci_extended(info->header.ci_field);
    f->dll.l = raw->len ? raw->bytes[0] : 0;
    f->dll.c = info->header.control;
    f->dll.ci = info->header.ci_field;
    f->dll.manuf = info->header.manufacturer_le;
    memcpy(f->dll.id, info->header.id, sizeof(f->dll.id));
    snprintf(f->dll.id_str, sizeof(f->dll.id_str), "%02X%02X%02X%02X",
             info->header.id[3], info->header.id[2], info->header.id[1], info->header.id[0]);
    f->dll.version = info->header.version;
    f->dll.dev_type = info->header.device_type;
}

void wmbus_parsed_frame_parse_meta(wmbus_parsed_frame_t *f)
{
    if (!f || !f->raw.bytes || f->raw.len < 12)
    {
        return;
    }

    const uint8_t *logical = f->raw.bytes;
    const uint16_t logical_len = f->raw.len;
    uint16_t ci_offset = (WMBUS_FIXED_HEADER_BYTES > 0) ? (WMBUS_FIXED_HEADER_BYTES - 1) : 0;
    if (ci_offset >= logical_len)
    {
        return;
    }
    uint8_t current_ci = logical[ci_offset];

    while (ci_offset < logical_len)
    {
        const wmbus_ci_class_t ci_class = wmbus_classify_ci_extended(current_ci);
        if (ci_class == WMBUS_CI_CLASS_ELL)
        {
            wmbus_ell_meta_t ell = {0};
            if (wmbus_parse_ell_meta(&f->info, logical, logical_len, &ell))
            {
                f->ell.has_ell = true;
                f->ell.ell = ell;
                ci_offset = ell.next_offset;
            }
            else
            {
                break;
            }
        }
        else if (ci_class == WMBUS_CI_CLASS_AFL)
        {
            wmbus_afl_meta_t afl = {0};
            if (wmbus_parse_afl_meta(logical, logical_len, ci_offset, &afl))
            {
                f->afl.has_afl = true;
                f->afl.afl = afl;
                ci_offset = afl.header_end_offset;
            }
            else
            {
                break;
            }
        }
        else if (ci_class == WMBUS_CI_CLASS_TPL ||
                 ci_class == WMBUS_CI_CLASS_DATA_MBUS ||
                 ci_class == WMBUS_CI_CLASS_DATA_OTHER ||
                 ci_class == WMBUS_CI_CLASS_NET_OR_TPL)
        {
            wmbus_tpl_meta_t tpl = {0};
            if (wmbus_parse_tpl_meta(current_ci, ci_offset, logical, logical_len, &tpl))
            {
                f->tpl.has_tpl = true;
                f->tpl.tpl = tpl;
            }
            break;
        }
        else
        {
            break;
        }

        if (ci_offset >= logical_len)
        {
            break;
        }
        current_ci = logical[ci_offset];
    }

    // Security meta
    wmbus_derive_security_meta(f->tpl.has_tpl ? &f->tpl.tpl : NULL,
                               f->ell.has_ell ? &f->ell.ell : NULL,
                               f->afl.has_afl ? &f->afl.afl : NULL,
                               &f->tpl.sec.security_mode,
                               &f->tpl.sec.enc_len,
                               &f->tpl.sec.encrypted);
    f->encrypted = f->tpl.sec.encrypted;
}
