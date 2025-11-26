#include "encoding_3of6.h"
#include <string.h>

static uint8_t encode_tab[16] = {
    0x16, 0x0D, 0x0E, 0x0B,
    0x1C, 0x19, 0x1A, 0x13,
    0x2C, 0x25, 0x26, 0x23,
    0x34, 0x31, 0x32, 0x29};

static uint8_t decode_tab[64] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x03, 0xFF, 0x01, 0x02, 0xFF,
    0xFF, 0xFF, 0xFF, 0x07, 0xFF, 0xFF, 0x00, 0xFF,
    0xFF, 0x05, 0x06, 0xFF, 0x04, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x0B, 0xFF, 0x09, 0x0A, 0xFF,
    0xFF, 0x0F, 0xFF, 0xFF, 0x08, 0xFF, 0xFF, 0xFF,
    0xFF, 0x0D, 0x0E, 0xFF, 0x0C, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void wmbus_encode_3of6(uint8_t *uncodedData, uint8_t *encodedData, uint8_t lastByte)
{
    uint8_t data[4];
    if (lastByte) {
        data[1] = 0x14;
    } else {
        data[0] = encode_tab[uncodedData[1] & 0x0F];
        data[1] = encode_tab[(uncodedData[1] >> 4) & 0x0F];
    }

    data[2] = encode_tab[uncodedData[0] & 0x0F];
    data[3] = encode_tab[(uncodedData[0] >> 4) & 0x0F];

    encodedData[0] = (data[3] << 2) | (data[2] >> 4);
    encodedData[1] = (data[2] << 4) | (data[1] >> 2);
    if (!lastByte) {
        encodedData[2] = (data[1] << 6) | data[0];
    }
}

wmbus_dec_status_t wmbus_decode_3of6(uint8_t *encodedData, uint8_t *decodedData, uint8_t lastByte)
{
    uint8_t data[4];

    if (!lastByte) {
        data[0] = decode_tab[encodedData[2] & 0x3F];
        data[1] = decode_tab[((encodedData[2] & 0xC0) >> 6) | ((encodedData[1] & 0x0F) << 2)];
    } else {
        data[0] = 0x00;
        data[1] = 0x00;
    }

    data[2] = decode_tab[((encodedData[1] & 0xF0) >> 4) | ((encodedData[0] & 0x03) << 4)];
    data[3] = decode_tab[((encodedData[0] & 0xFC) >> 2)];

    if ((data[0] == 0xFF) || (data[1] == 0xFF) || (data[2] == 0xFF) || (data[3] == 0xFF)) {
        return WMBUS_DEC_ERROR;
    }

    decodedData[0] = (data[3] << 4) | data[2];
    if (!lastByte) {
        decodedData[1] = (data[1] << 4) | data[0];
    }
    return WMBUS_DEC_OK;
}
