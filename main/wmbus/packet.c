#include "packet.h"

uint16_t wmbus_packet_size_from_l(uint8_t l_field)
{
    uint16_t nrBytes;
    uint8_t nrBlocks;

    if (l_field < 26)
        nrBlocks = 2;
    else
        nrBlocks = (((l_field - 26) / 16) + 3);

    nrBytes = l_field + 1;
    nrBytes += (2 * nrBlocks);
    return nrBytes;
}

uint16_t wmbus_encoded_size_from_packet(uint16_t packetSize)
{
    uint16_t tmodeVar = (3 * packetSize) / 2;
    if (packetSize % 2)
        return (tmodeVar + 1);
    else
        return (tmodeVar);
}

uint16_t wmbus_crc_step(uint16_t crcReg, uint8_t crcData)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (((crcReg & 0x8000) >> 8) ^ (crcData & 0x80)) {
            crcReg = (crcReg << 1) ^ 0x3D65;
        } else {
            crcReg = (crcReg << 1);
        }
        crcData <<= 1;
    }
    return crcReg;
}

wmbus_packet_status_t wmbus_decode_tmode(uint8_t *pByte, uint8_t *pPacket, uint16_t packetSize)
{
    uint16_t bytesRemaining = packetSize;
    uint16_t bytesEncoded = 0;
    uint16_t decodingStatus;
    uint16_t crc = 0;
    uint16_t crcField = 0;

    while (bytesRemaining) {
        if (bytesRemaining == 1) {
            decodingStatus = wmbus_decode_3of6(pByte, pPacket, 1);
            if (decodingStatus != WMBUS_DEC_OK) {
                return WMBUS_PKT_CODING_ERROR;
            }
            bytesRemaining -= 1;
            bytesEncoded += 1;
            if (((~crc) & 0xFF) != *pPacket) {
                return WMBUS_PKT_CRC_ERROR;
            }
        } else {
            decodingStatus = wmbus_decode_3of6(pByte, pPacket, 0);
            if (decodingStatus != WMBUS_DEC_OK) {
                return WMBUS_PKT_CODING_ERROR;
            }

            bytesRemaining -= 2;
            bytesEncoded += 2;

            if (bytesRemaining == 0)
                crcField = 1;
            else if (bytesEncoded > 10)
                crcField = !((bytesEncoded - 12) % 18);

            if (crcField) {
                if (((~crc) & 0xFF) != *(pPacket + 1))
                    return WMBUS_PKT_CRC_ERROR;

                if ((((~crc) >> 8) & 0xFF) != *pPacket)
                    return WMBUS_PKT_CRC_ERROR;

                crcField = 0;
                crc = 0;
            } else if (bytesRemaining == 1) {
                crc = wmbus_crc_step(crc, *pPacket);
                if ((((~crc) >> 8) & 0xFF) != *(pPacket + 1))
                    return WMBUS_PKT_CRC_ERROR;
            } else {
                crc = wmbus_crc_step(crc, *pPacket);
                crc = wmbus_crc_step(crc, *(pPacket + 1));
            }

            pByte += 3;
            pPacket += 2;
        }
    }

    return WMBUS_PKT_OK;
}
