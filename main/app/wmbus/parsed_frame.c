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

    // TPL
    wmbus_tpl_meta_t tpl = {0};
    if (wmbus_parse_tpl_meta(&f->info, f->raw.bytes, f->raw.len, &tpl))
    {
        f->tpl.has_tpl = tpl.has_tpl;
        f->tpl.tpl = tpl;
    }

    // ELL
    wmbus_ell_meta_t ell = {0};
    if (wmbus_parse_ell_meta(&f->info, f->raw.bytes, f->raw.len, &ell))
    {
        f->ell.has_ell = ell.has_ell;
        f->ell.ell = ell;
        // AFL nested after ELL
        wmbus_afl_meta_t afl = {0};
        if (wmbus_parse_afl_meta(f->raw.bytes, f->raw.len, ell.next_offset, &afl))
        {
            f->afl.has_afl = afl.has_afl;
            f->afl.afl = afl;
        }
    }
    else if (f->dll.ci == 0x90)
    {
        wmbus_afl_meta_t afl = {0};
        if (wmbus_parse_afl_meta(f->raw.bytes, f->raw.len, 11, &afl))
        {
            f->afl.has_afl = afl.has_afl;
            f->afl.afl = afl;
        }
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
