#include "wmbus/packet.h"

#include "wmbus/encoding_3of6.h"
#include "wmbus/crc16.h"

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

void wmbus_encode_tx_packet(uint8_t *packet, const uint8_t *data, uint8_t data_size)
{
    uint8_t loop_cnt;
    uint8_t data_remaining = data_size;
    uint8_t data_encoded = 0;
    uint16_t crc = 0;

    *packet = data_size + 10;
    crc = wmbus_crc16_step(crc, *packet);
    packet++;

    *packet = WMBUS_PACKET_C_FIELD;
    crc = wmbus_crc16_step(crc, *packet);
    packet++;

    *packet = (uint8_t)WMBUS_MAN_CODE;
    crc = wmbus_crc16_step(crc, *packet);
    packet++;
    *packet = (uint8_t)(WMBUS_MAN_CODE >> 8);
    crc = wmbus_crc16_step(crc, *packet);
    packet++;

    *packet = (uint8_t)WMBUS_MAN_NUMBER;
    crc = wmbus_crc16_step(crc, *packet);
    packet++;
    *packet = (uint8_t)(WMBUS_MAN_NUMBER >> 8);
    crc = wmbus_crc16_step(crc, *packet);
    packet++;
    *packet = (uint8_t)(WMBUS_MAN_NUMBER >> 16);
    crc = wmbus_crc16_step(crc, *packet);
    packet++;
    *packet = (uint8_t)(WMBUS_MAN_NUMBER >> 24);
    crc = wmbus_crc16_step(crc, *packet);
    packet++;
    *packet = (uint8_t)(WMBUS_MAN_ID);
    crc = wmbus_crc16_step(crc, *packet);
    packet++;
    *packet = (uint8_t)(WMBUS_MAN_VER);
    crc = wmbus_crc16_step(crc, *packet);
    packet++;

    *packet = HI_UINT16(~crc);
    packet++;
    *packet = LO_UINT16(~crc);
    packet++;
    crc = 0;

    *packet = WMBUS_PACKET_CI_FIELD;
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
