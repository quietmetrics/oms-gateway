#include "wmbus/crc16.h"

uint16_t wmbus_crc16_step(uint16_t crc, uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (((crc & 0x8000) >> 8) ^ (data & 0x80))
        {
            crc = (crc << 1) ^ WMBUS_CRC_POLY;
        }
        else
        {
            crc = (crc << 1);
        }
        data <<= 1;
    }
    return crc;
}
