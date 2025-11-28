#include "wmbus/packet.h"

#include "wmbus/3of6.h"
#include "wmbus/crc16.h"
#include <string.h>

#define HI_UINT16(a) ((uint8_t)(((a) >> 8) & 0xFF))
#define LO_UINT16(a) ((uint8_t)((a) & 0xFF))

uint16_t wmbus_packet_size(uint8_t l_field)
{
    uint16_t nr_bytes;
    uint8_t nr_blocks;

    if (l_field < 26)
    {
        nr_blocks = 2;
    }
    else
    {
        nr_blocks = (((l_field - 26) / 16) + 3);
    }

    nr_bytes = l_field + 1;
    nr_bytes += (2 * nr_blocks);

    return nr_bytes;
}

uint16_t wmbus_byte_size_tmode(bool transmit, uint16_t packet_size)
{
    uint16_t tmode_var = (3 * packet_size) / 2;

    if (transmit)
    {
        return (tmode_var + 1);
    }

    if (packet_size % 2)
    {
        return (tmode_var + 1);
    }
    return tmode_var;
}

void wmbus_build_default_header(WmbusFrameHeaderRaw *header, uint8_t payload_len)
{
    if (!header)
    {
        return;
    }

    memset(header, 0, sizeof(WmbusFrameHeaderRaw));
    header->length = payload_len + WMBUS_L_FIELD_FIXED_BYTES;
    header->control = WMBUS_PACKET_C_FIELD;
    header->manufacturer_le = WMBUS_MAN_CODE;
    header->id[0] = (uint8_t)(WMBUS_MAN_NUMBER & 0xFF);
    header->id[1] = (uint8_t)((WMBUS_MAN_NUMBER >> 8) & 0xFF);
    header->id[2] = (uint8_t)((WMBUS_MAN_NUMBER >> 16) & 0xFF);
    header->id[3] = (uint8_t)((WMBUS_MAN_NUMBER >> 24) & 0xFF);
    // Naming follows legacy constants: MAN_ID stores Version, MAN_VER stores Device Type.
    header->version = WMBUS_MAN_ID;
    header->device_type = WMBUS_MAN_VER;
    header->ci_field = WMBUS_PACKET_CI_FIELD;
}

void wmbus_encode_tx_packet(uint8_t *packet, const uint8_t *data, uint8_t data_size)
{
    WmbusFrameHeaderRaw header;
    wmbus_build_default_header(&header, data_size);
    wmbus_encode_tx_packet_with_header(packet, &header, data, data_size);
}

void wmbus_encode_tx_packet_with_header(uint8_t *packet, const WmbusFrameHeaderRaw *header, const uint8_t *data, uint8_t data_size)
{
    if (!packet || !header)
    {
        return;
    }

    uint8_t loop_cnt;
    uint8_t data_remaining = data_size;
    uint8_t data_encoded = 0;
    uint16_t crc = 0;

    const uint8_t expected_l_field = (uint8_t)(data_size + WMBUS_L_FIELD_FIXED_BYTES);
    uint8_t l_field = header->length ? header->length : expected_l_field;
    if (l_field != expected_l_field)
    {
        l_field = expected_l_field; // keep L-field consistent with payload size
    }

    *packet = l_field;
    crc = wmbus_crc16_step(crc, *packet);
    packet++;

    *packet = header->control;
    crc = wmbus_crc16_step(crc, *packet);
    packet++;

    *packet = (uint8_t)header->manufacturer_le;
    crc = wmbus_crc16_step(crc, *packet);
    packet++;
    *packet = (uint8_t)(header->manufacturer_le >> 8);
    crc = wmbus_crc16_step(crc, *packet);
    packet++;

    *packet = header->id[0];
    crc = wmbus_crc16_step(crc, *packet);
    packet++;
    *packet = header->id[1];
    crc = wmbus_crc16_step(crc, *packet);
    packet++;
    *packet = header->id[2];
    crc = wmbus_crc16_step(crc, *packet);
    packet++;
    *packet = header->id[3];
    crc = wmbus_crc16_step(crc, *packet);
    packet++;
    *packet = header->version;
    crc = wmbus_crc16_step(crc, *packet);
    packet++;
    *packet = header->device_type;
    crc = wmbus_crc16_step(crc, *packet);
    packet++;

    *packet = HI_UINT16(~crc);
    packet++;
    *packet = LO_UINT16(~crc);
    packet++;
    crc = 0;

    *packet = header->ci_field;
    crc = wmbus_crc16_step(crc, *packet);
    packet++;

    if (data_remaining < 16)
    {
        for (loop_cnt = 0; loop_cnt < data_remaining; loop_cnt++)
        {
            *packet = data[data_encoded];
            crc = wmbus_crc16_step(crc, *packet);
            packet++;
            data_encoded++;
        }

        *packet = HI_UINT16(~crc);
        packet++;
        *packet = LO_UINT16(~crc);
        packet++;
        crc = 0;
        data_remaining = 0;
    }
    else
    {
        for (loop_cnt = 0; loop_cnt < 15; loop_cnt++)
        {
            *packet = data[data_encoded];
            crc = wmbus_crc16_step(crc, *packet);
            packet++;
            data_encoded++;
        }

        *packet = HI_UINT16(~crc);
        packet++;
        *packet = LO_UINT16(~crc);
        packet++;
        crc = 0;
        data_remaining -= 15;
    }

    while (data_remaining)
    {
        if (data_remaining < 17)
        {
            for (loop_cnt = 0; loop_cnt < data_remaining; loop_cnt++)
            {
                *packet = data[data_encoded];
                crc = wmbus_crc16_step(crc, *packet);
                packet++;
                data_encoded++;
            }

            *packet = HI_UINT16(~crc);
            packet++;
            *packet = LO_UINT16(~crc);
            packet++;
            crc = 0;
            data_remaining = 0;
        }
        else
        {
            for (loop_cnt = 0; loop_cnt < 16; loop_cnt++)
            {
                *packet = data[data_encoded];
                crc = wmbus_crc16_step(crc, *packet);
                packet++;
                data_encoded++;
            }

            *packet = HI_UINT16(~crc);
            packet++;
            *packet = LO_UINT16(~crc);
            packet++;
            crc = 0;
            data_remaining -= 16;
        }
    }
}

void wmbus_encode_tx_bytes_tmode(uint8_t *encoded, const uint8_t *packet, uint16_t packet_size)
{
    uint16_t bytes_remaining = packet_size;

    while (bytes_remaining)
    {
        if (bytes_remaining == 1)
        {
            wmbus_encode_3of6(packet, encoded, 1);
            bytes_remaining -= 1;
        }
        else if (bytes_remaining == 2)
        {
            wmbus_encode_3of6(packet, encoded, 0);
            encoded += 3;
            *encoded = 0x55;
            bytes_remaining -= 2;
        }
        else
        {
            wmbus_encode_3of6(packet, encoded, 0);
            encoded += 3;
            packet += 2;
            bytes_remaining -= 2;
        }
    }
}

uint16_t wmbus_decode_rx_bytes_tmode(const uint8_t *encoded, uint8_t *packet, uint16_t packet_size)
{
    uint16_t bytes_remaining = packet_size;
    uint16_t bytes_encoded = 0;
    uint16_t crc = 0;
    uint16_t crc_field = 0;

    while (bytes_remaining)
    {
        if (bytes_remaining == 1)
        {
            uint16_t status = wmbus_decode_3of6(encoded, packet, 1);
            if (status != WMBUS_3OF6_OK)
            {
                return WMBUS_PKT_CODING_ERROR;
            }

            bytes_remaining -= 1;
            bytes_encoded += 1;

            if (LO_UINT16(~crc) != *(packet))
            {
                return WMBUS_PKT_CRC_ERROR;
            }
        }
        else
        {
            uint16_t status = wmbus_decode_3of6(encoded, packet, 0);
            if (status != WMBUS_3OF6_OK)
            {
                return WMBUS_PKT_CODING_ERROR;
            }

            bytes_remaining -= 2;
            bytes_encoded += 2;

            if (bytes_remaining == 0)
            {
                crc_field = 1;
            }
            else if (bytes_encoded > 10)
            {
                crc_field = !((bytes_encoded - 12) % 18);
            }

            if (crc_field)
            {
                if (LO_UINT16(~crc) != *(packet + 1))
                {
                    return WMBUS_PKT_CRC_ERROR;
                }
                if (HI_UINT16(~crc) != *packet)
                {
                    return WMBUS_PKT_CRC_ERROR;
                }

                crc_field = 0;
                crc = 0;
            }
            else if (bytes_remaining == 1)
            {
                crc = wmbus_crc16_step(crc, *(packet));
                if (HI_UINT16(~crc) != *(packet + 1))
                {
                    return WMBUS_PKT_CRC_ERROR;
                }
            }
            else
            {
                crc = wmbus_crc16_step(crc, *(packet));
                crc = wmbus_crc16_step(crc, *(packet + 1));
            }

            encoded += 3;
            packet += 2;
        }
    }

    return WMBUS_PKT_OK;
}

uint16_t wmbus_strip_crc_blocks(const uint8_t *packet_with_crc, uint16_t packet_with_crc_len, uint8_t *packet_no_crc, uint16_t packet_no_crc_capacity)
{
    if (!packet_with_crc || !packet_no_crc)
    {
        return 0;
    }

    if (packet_with_crc_len < (WMBUS_FIXED_HEADER_BYTES + 4)) // needs at least header + two CRC fields
    {
        return 0;
    }

    const uint8_t l_field = packet_with_crc[0];
    const uint16_t expected_with_crc = wmbus_packet_size(l_field);
    if (expected_with_crc == 0 || expected_with_crc > packet_with_crc_len)
    {
        return 0;
    }

    if (l_field < WMBUS_L_FIELD_FIXED_BYTES)
    {
        return 0;
    }

    const uint16_t logical_size = (uint16_t)(l_field + 1);
    if (packet_no_crc_capacity < logical_size)
    {
        return 0;
    }

    const uint16_t payload_len = (uint16_t)(l_field - WMBUS_L_FIELD_FIXED_BYTES);

    // Copy L + (C..device_type); CI and payload follow after the first CRC.
    memcpy(packet_no_crc, packet_with_crc, WMBUS_FIXED_HEADER_BYTES - 1);

    uint16_t dst_idx = WMBUS_FIXED_HEADER_BYTES - 1; // points to CI in the CRC-free buffer
    uint16_t src_idx = (WMBUS_FIXED_HEADER_BYTES - 1) + 2; // skip first CRC pair

    packet_no_crc[dst_idx++] = packet_with_crc[src_idx++]; // CI-field

    uint16_t remaining_payload = payload_len;
    uint16_t block_idx = 0;

    while (remaining_payload)
    {
        uint16_t block_len;
        if (block_idx == 0)
        {
            block_len = (remaining_payload < 16) ? remaining_payload : 15;
        }
        else
        {
            block_len = (remaining_payload < 17) ? remaining_payload : 16;
        }

        if ((src_idx + block_len + 2) > packet_with_crc_len)
        {
            return 0;
        }

        memcpy(packet_no_crc + dst_idx, packet_with_crc + src_idx, block_len);

        src_idx += block_len;
        dst_idx += block_len;
        remaining_payload -= block_len;
        block_idx++;

        // Skip CRC that follows each block (even after the last one)
        src_idx += 2;
    }

    if (dst_idx != logical_size)
    {
        return 0;
    }

    return logical_size;
}

bool wmbus_parse_frame_header(const uint8_t *packet_no_crc, uint16_t packet_len, WmbusFrameHeaderRaw *out_header, const uint8_t **payload, uint16_t *payload_len)
{
    if (!packet_no_crc || !out_header)
    {
        return false;
    }

    if (packet_len < WMBUS_FIXED_HEADER_BYTES)
    {
        return false;
    }

    const uint8_t l_field = packet_no_crc[0];
    if ((uint16_t)(l_field + 1) > packet_len || l_field < WMBUS_L_FIELD_FIXED_BYTES)
    {
        return false;
    }

    out_header->length = l_field;
    out_header->control = packet_no_crc[1];
    out_header->manufacturer_le = (uint16_t)packet_no_crc[2] | ((uint16_t)packet_no_crc[3] << 8);
    memcpy(out_header->id, &packet_no_crc[4], sizeof(out_header->id));
    out_header->version = packet_no_crc[8];
    out_header->device_type = packet_no_crc[9];
    out_header->ci_field = packet_no_crc[10];

    if (payload)
    {
        *payload = packet_no_crc + WMBUS_FIXED_HEADER_BYTES;
    }
    if (payload_len)
    {
        *payload_len = (uint16_t)(l_field - WMBUS_L_FIELD_FIXED_BYTES);
    }

    return true;
}
