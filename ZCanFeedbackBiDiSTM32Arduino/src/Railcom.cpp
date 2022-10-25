/*********************************************************************
 * Railcom
 *
 * Copyright (C) 2022 Marcel Maage
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * LICENSE file for more details.
 */

#include "Railcom.h"

bool Railcom::getStartAndStopByteOfUart(bool *startOfSearch, bool *endOfSearch, bool **startBit, bool **endBit)
{
    bool *iterator = startOfSearch;

    while (!(iterator[0] && !iterator[1]) && (&iterator[1] != endOfSearch))
    {
        // make sure to have first high level
        iterator++;
    }
    if (&iterator[1] == endOfSearch)
    {
        return false;
    }
    *startBit = &iterator[1];

    *startBit += 5;

    *endBit = *startBit + 28;

    if (*endBit > endOfSearch)
    {
        return false;
    }

    // std::cout<<std::dec<<*startBit-startOfSearch<<" "<<*endBit-startOfSearch<<" "<<*endBit-*startBit<<std::endl;

    return true;
}

uint8_t Railcom::handleBitStream(bool bitStreamIN1samplePer1us[], size_t length, std::array<uint8_t, 8> &railcomData)
{
    // search for starting zero of first byte by ignoring first 15us
    bool *startBit = bitStreamIN1samplePer1us;
    bool *endBit = bitStreamIN1samplePer1us;
    uint8_t numberOfBytes{0};

    for (auto dataIterator = railcomData.begin(); dataIterator != railcomData.end(); dataIterator++)
    {
        *dataIterator = 0xFF;
        if (Railcom::getStartAndStopByteOfUart(endBit, &bitStreamIN1samplePer1us[length - 1], &startBit, &endBit))
        {
            // found
            uint8_t dataByte{0};
            uint8_t bit{0};
            while (endBit >= startBit)
            {
                dataByte |= (((*startBit) ? 1 : 0) << bit++);
                startBit += 4;
            }
            if (8 == bit)
            {
                // from 4 to 8 code
                dataByte = encode8to4[dataByte];
                switch (dataByte)
                {
                case 0xFF:
                    // not used => error
                    break;
                default:
                    *dataIterator = dataByte;
                    numberOfBytes++;
                    break;
                }
            }
        }
        else
        {
            break;
        }
    }
    return numberOfBytes;
}

// analyze data

//     if (getStartAndStopByteOfUart(&bitStreamIN1samplePer1us[startChannel1], &bitStreamIN1samplePer1us[endChannel1], &startBit, &endBit))
//     {
//         // found
//         uint8_t dataByte{0};
//         uint8_t bit{0};
//         while (endBit >= startBit)
//         {
//             dataByte |= (((*startBit) ? 1 : 0) << bit++);
//             startBit += 4;
//         }
//         // std::cout<<std::hex<<(int)dataByte<<std::endl;
//         if (8 == bit)
//         {
//             // from 4 to 8 code
//             dataByte = encode8to4[dataByte];
//             switch (dataByte)
//             {
//             case 0xFF:
//                 // not used
//                 break;
//             case 0x40: // NACK
//                 break;
//             case 0x41: // ACK
//                 break;
//             case 0x42: // BUSY
//                 break;
//             default:
//                 // a value
//                 break;
//             }
//         }
//         else
//         {
//             // detection failed
//         }
//     }
//     // std::cout<<"-------------"<<std::endl;
//     // get second byte
//     bool *startOfSearch = endBit;
//     bool *endOfSearch = endBit + 50;
//     if (getStartAndStopByteOfUart(startOfSearch, endOfSearch, &startBit, &endBit))
//     {
//         // found
//         uint8_t dataByte{0};
//         uint8_t bit{0};
//         while (endBit >= startBit)
//         {
//             dataByte |= (((*startBit) ? 1 : 0) << bit++);
//             startBit += 4;
//         }
//         // std::cout<<std::hex<<(int)dataByte<<std::endl;

//         if (8 == bit)
//         {
//             // from 4 to 8 code
//             dataByte = encode8to4[dataByte];
//             switch (dataByte)
//             {
//             case 0xFF:
//                 // not used
//                 break;
//             case 0x40: // NACK
//                 break;
//             case 0x41: // ACK
//                 break;
//             case 0x42: // BUSY
//                 break;
//             default:
//                 // a value
//                 break;
//             }
//         }
//         else
//         {
//             // detection failed
//         }
//     }
//     return true;
// }


uint8_t Railcom::encode4to8[] = {
    0b10101100,
    0b10101010,
    0b10101001,
    0b10100101,
    0b10100011,
    0b10100110,
    0b10011100,
    0b10011010,
    0b10011001,
    0b10010101,
    0b10010011,
    0b10010110,
    0b10001110,
    0b10001101,
    0b10001011,
    0b10110001,
    0b10110010,
    0b10110100,
    0b10111000,
    0b01110100,
    0b01110010,
    0b01101100,
    0b01110010,
    0b01101100,
    0b01101010,
    0b01101001,
    0b01100101,
    0b01100011,
    0b01100110,
    0b01011100,
    0b01011010,
    0b01011001,
    0b01010101,
    0b01010011,
    0b01010110,
    0b01001110,
    0b01001101,
    0b01001011,
    0b01000111,
    0b01110001,
    0b11101000,
    0b11100100,
    0b11100010,
    0b11010001,
    0b11001001,
    0b11000101,
    0b11011000,
    0b11010100,
    0b11010010,
    0b11001010,
    0b11000110,
    0b11001100,
    0b01111000,
    0b00010111,
    0b00011011,
    0b00011101,
    0b00011110,
    0b00101110,
    0b00110110,
    0b00111010,
    0b00100111,
    0b00101011,
    0b00101101,
    0b00110101,
    0b00111001,
    0b00110011, // 0x3F

    0b00001111, // NACK
    0b11110000, // ACK
    0b11100001, // BUSY
    0b11000011, // not used
    0b10000111, // not used
    0b00111100  // not used
};

uint8_t Railcom::encode8to4[] = {
    0xFF, // invalid 0b00000000
    0xFF, // invalid 0b00000001
    0xFF, // invalid 0b00000010
    0xFF, // invalid 0b00000011
    0xFF, // invalid 0b00000100
    0xFF, // invalid 0b00000101
    0xFF, // invalid 0b00000110
    0xFF, // invalid 0b00000111
    0xFF, // invalid 0b00001000
    0xFF, // invalid 0b00001001
    0xFF, // invalid 0b00001010
    0xFF, // invalid 0b00001011
    0xFF, // invalid 0b00001100
    0xFF, // invalid 0b00001101
    0xFF, // invalid 0b00001110
    0x40, // NACK 15 0b00001111
    0xFF, // invalid 0b00010000
    0xFF, // invalid 0b00010001
    0xFF, // invalid 0b00010010
    0xFF, // invalid 0b00010011
    0xFF, // invalid 0b00010100
    0xFF, // invalid 0b00010101
    0xFF, // invalid 0b00010110
    0x33, // 0x17    0b00010111
    0xFF, // invalid 0b00011000
    0xFF, // invalid 0b00011001
    0xFF, // invalid 0b00011010
    0x34, // 0x1B    0b00011011
    0xFF, // invalid 0b00011100
    0x35, // 0x1D    0b00011101
    0x36, // 0x1E    0b00011110
    0xFF, // invalid 0b00011111
    0xFF, // invalid 0b00100000
    0xFF, // invalid 0b00100001
    0xFF, // invalid 0b00100010
    0xFF, // invalid 0b00100011
    0xFF, // invalid 0b00100100
    0xFF, // invalid 0b00100101
    0xFF, // invalid 0b00100110
    0x3A, // 0x27    0b00100111
    0xFF, // invalid 0b00101000
    0xFF, // invalid 0b00101001
    0xFF, // invalid 0b00101010
    0x3B, // 0x2B    0b00101011
    0xFF, // invalid 0b00101100
    0x3C, // invalid 0b00101101
    0x37, // 0x2E    0b00101110
    0xFF, // invalid 0b00101111
    0xFF, // invalid 0b00110000
    0xFF, // invalid 0b00110001
    0xFF, // invalid 0b00110010
    0x3F, // 0x33    0b00110011
    0xFF, // invalid 0b00110100
    0x3D, // 0x35    0b00110101
    0x38, // 0x36    0b00110110
    0xFF, // invalid 0b00110111
    0xFF, // invalid 0b00111000
    0x3E, // 0x39    0b00111001
    0x39, // 0x3A    0b00111010
    0xFF, // invalid 0b00111011
    0xEE, // not use 0b00111100
    0xFF, // invalid 0b00111101
    0xFF, // invalid 0b00111110
    0xFF, // invalid 0b00111111
    0xFF, // invalid 0b01000000
    0xFF, // invalid 0b01000001
    0xFF, // invalid 0b01000010
    0xFF, // invalid 0b01000011
    0xFF, // invalid 0b01000100
    0xFF, // invalid 0b01000101
    0xFF, // invalid 0b01000110
    0x24, // 0x47    0b01000111
    0xFF, // invalid 0b01001000
    0xFF, // invalid 0b01001001
    0xFF, // invalid 0b01001010
    0x23, // 0x4B    0b01001011
    0xFF, // invalid 0b01001100
    0x22, // 0x4D    0b01001101
    0x21, // 0x4E    0b01001110
    0xFF, // invalid 0b01001111
    0xFF, // invalid 0b01010000
    0xFF, // invalid 0b01010001
    0xFF, // invalid 0b01010010
    0x1F, // 0x53    0b01010011
    0xFF, // invalid 0b01010100
    0x1E, // 0x55    0b01010101
    0x20, // 0x56    0b01010110
    0xFF, // invalid 0b01010111
    0xFF, // invalid 0b01011000
    0x1D, // 0x59    0b01011001
    0x1C, // 0x5A    0b01011010
    0xFF, // invalid 0b01011011
    0x1B, // 0x5C    0b01011100
    0xFF, // invalid 0b01011101
    0xFF, // invalid 0b01011110
    0xFF, // invalid 0b01011111
    0xFF, // invalid 0b01100000
    0xFF, // invalid 0b01100001
    0xFF, // invalid 0b01100010
    0x19, // 0x63    0b01100011
    0xFF, // invalid 0b01100100
    0x18, // 0x65    0b01100101
    0x1A, // 0x66    0b01100110
    0xFF, // invalid 0b01100111
    0xFF, // invalid 0b01101000
    0x17, // 0x69    0b01101001
    0x16, // 0x6A    0b01101010
    0xFF, // invalid 0b01101011
    0x15, // 0x6C    0b01101100
    0xFF, // invalid 0b01101101
    0xFF, // invalid 0b01101110
    0xFF, // invalid 0b01101111
    0xFF, // invalid 0b01110000
    0x25, // 0x71    0b01110001
    0x14, // 0x72    0b01110010
    0xFF, // invalid 0b01110011
    0x13, // 0x74    0b01110100
    0xFF, // invalid 0b01110101
    0xFF, // invalid 0b01110110
    0xFF, // invalid 0b01110111
    0x32, // 0x78    0b01111000
    0xFF, // invalid 0b01111001
    0xFF, // invalid 0b01111010
    0xFF, // invalid 0b01111011
    0xFF, // invalid 0b01111100
    0xFF, // invalid 0b01111101
    0xFF, // invalid 0b01111110
    0xFF, // invalid 0b01111111
    0xFF, // invalid 0b10000000
    0xFF, // invalid 0b10000001
    0xFF, // invalid 0b10000010
    0xFF, // invalid 0b10000011
    0xFF, // invalid 0b10000100
    0xFF, // invalid 0b10000101
    0xFF, // invalid 0b10000110
    0xEE, // not use 0b10000111
    0xFF, // invalid 0b10001000
    0xFF, // invalid 0b10001001
    0xFF, // invalid 0b10001010
    0x0E, // 0x8B    0b10001011
    0xFF, // invalid 0b10001100
    0x0D, // 0x8D    0b10001101
    0x0C, // 0x8E    0b10001110
    0xFF, // invalid 0b10001111
    0xFF, // invalid 0b10010000
    0xFF, // invalid 0b10010001
    0xFF, // invalid 0b10010010
    0x0A, // 0x93    0b10010011
    0xFF, // invalid 0b10010100
    0x09, // 0x95    0b10010101
    0x0B, // 0x96    0b10010110
    0xFF, // invalid 0b10010111
    0xFF, // invalid 0b10011000
    0x08, // 0x99    0b10011001
    0x07, // 0x9A    0b10011010
    0xFF, // invalid 0b10011011
    0x06, // 0x9C    0b10011100
    0xFF, // invalid 0b10011101
    0xFF, // invalid 0b10011110
    0xFF, // invalid 0b10011111
    0xFF, // invalid 0b10100000
    0xFF, // invalid 0b10100001
    0xFF, // invalid 0b10100010
    0x04, // 0xA3    0b10100011
    0xFF, // invalid 0b10100100
    0x03, // 0xA5    0b10100101
    0x05, // 0xA6    0b10100110
    0xFF, // invalid 0b10100111
    0xFF, // invalid 0b10101000
    0x02, // 0xA9    0b10101001
    0x01, // 0xAA    0b10101010
    0xFF, // invalid 0b10101011
    0x00, // 0xAC    0b10101100
    0xFF, // invalid 0b10101101
    0xFF, // invalid 0b10101110
    0xFF, // invalid 0b10101111
    0xFF, // invalid 0b10110000
    0x0F, // 0xB1    0b10110001
    0x10, // 0xB2    0b10110010
    0xFF, // invalid 0b10110011
    0x11, // 0xB4    0b10110100
    0xFF, // invalid 0b10110101
    0xFF, // invalid 0b10110110
    0xFF, // invalid 0b10110111
    0x12, // 0xB8    0b10111000
    0xFF, // invalid 0b10111001
    0xFF, // invalid 0b10111010
    0xFF, // invalid 0b10111011
    0xFF, // invalid 0b10111100
    0xFF, // invalid 0b10111101
    0xFF, // invalid 0b10111110
    0xFF, // invalid 0b10111111
    0xFF, // invalid 0b11000000
    0xFF, // invalid 0b11000001
    0xFF, // invalid 0b11000010
    0xEE, // not use 0b11000011
    0xFF, // invalid 0b11000100
    0x2B, // 0xC5    0b11000101
    0x30, // 0xC6    0b11000110
    0xFF, // invalid 0b11000111
    0xFF, // invalid 0b11001000
    0x2A, // 0xC9    0b11001001
    0x2F, // 0xCA    0b11001010
    0xFF, // invalid 0b11001011
    0x31, // 0xCC    0b11001100
    0xFF, // invalid 0b11001101
    0xFF, // invalid 0b11001110
    0xFF, // invalid 0b11001111
    0xFF, // invalid 0b11010000
    0x29, // 0xD1    0b11010001
    0x2E, // 0xD2    0b11010010
    0xFF, // invalid 0b11010011
    0x2D, // 0xD4    0b11010100
    0xFF, // invalid 0b11010101
    0xFF, // invalid 0b11010110
    0xFF, // invalid 0b11010111
    0x2C, // 0xD8    0b11011000
    0xFF, // invalid 0b11011001
    0xFF, // invalid 0b11011010
    0xFF, // invalid 0b11011011
    0xFF, // invalid 0b11011100
    0xFF, // invalid 0b11011101
    0xFF, // invalid 0b11011110
    0xFF, // invalid 0b11011111
    0xFF, // invalid 0b11100000
    0x42, // BUSY    0b11100001
    0x28, // 0xE2    0b11100010
    0xFF, // invalid 0b11100011
    0x27, // 0xE4    0b11100100
    0xFF, // invalid 0b11100101
    0xFF, // invalid 0b11100110
    0xFF, // invalid 0b11100111
    0x26, // 0xE8    0b11101000
    0xFF, // invalid 0b11101001
    0xFF, // invalid 0b11101010
    0xFF, // invalid 0b11101011
    0xFF, // invalid 0b11101100
    0xFF, // invalid 0b11101101
    0xFF, // invalid 0b11101110
    0x41, // ACK     0b11110000
    0xFF, // invalid 0b11110001
    0xFF, // invalid 0b11110010
    0xFF, // invalid 0b11110011
    0xFF, // invalid 0b11110100
    0xFF, // invalid 0b11110101
    0xFF, // invalid 0b11110110
    0xFF, // invalid 0b11110111
    0xFF, // invalid 0b11111000
    0xFF, // invalid 0b11111001
    0xFF, // invalid 0b11111010
    0xFF, // invalid 0b11111011
    0xFF, // invalid 0b11111100
    0xFF, // invalid 0b11111101
    0xFF, // invalid 0b11111110
    0xFF, // invalid 0b11111111
};