/*********************************************************************
 * Railcom Packet
 *
 * Copyright (C) 2023 Marcel Maage
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

#pragma once

#include <array>
#include <cstdint>

typedef struct RailcomPacket12Bit
{
    uint8_t id;
    std::array<uint8_t, 1> data;
    RailcomPacket12Bit() : id(0),
                           data{{0}} {

                           };
    // index 0 is first received byte
    RailcomPacket12Bit(std::array<uint8_t, 2> input) : id((input[0] >> 2) & 0xF),
                                                       data{{static_cast<uint8_t>(((input[0] & 0x03) << 6u) | (input[1] & 0x3F))}} {

                                                       };
} RailcomPacket12Bit;

typedef struct RailcomPacket18Bit
{
    uint8_t id;
    std::array<uint8_t, 2> data;
    RailcomPacket18Bit() : id(0),
                           data{{0, 0}} {

                           };
    // index 0 is first received byte
    RailcomPacket18Bit(std::array<uint8_t, 3> input) : id((input[0] >> 2) & 0xF),
                                                       data{{static_cast<uint8_t>(((input[1] & 0x03) << 6) | (input[2] & 0x3F)), static_cast<uint8_t>(((input[0] & 0x03) << 4) | ((input[1] & 0x3B) >> 2))}} {

                                                       };
} RailcomPacket18Bit;

typedef struct RailcomPacket24Bit
{
    uint8_t id;
    std::array<uint8_t, 3> data;
    RailcomPacket24Bit() : id(0),
                           data{{0, 0, 0}} {

                           };
    // index 0 is first received byte
    RailcomPacket24Bit(std::array<uint8_t, 4> input) : id((input[0] >> 2) & 0xF),
                                                       data{{static_cast<uint8_t>(((input[2] & 0x03) << 6) | (input[3] & 0x3F)), static_cast<uint8_t>(((input[1] & 0x0F) << 4) | ((input[2] & 0x3B) >> 2)), static_cast<uint8_t>(((input[0] & 0x03) << 2) | ((input[1] & 0x30) >> 4))}} {

                                                       };
} RailcomPacket24Bit;

typedef struct RailcomPacket36Bit
{
    uint8_t id;
    std::array<uint8_t, 4> data;
    RailcomPacket36Bit() : id(0),
                           data{{0, 0, 0, 0}} {

                           };
    // index 0 is first received byte
    RailcomPacket36Bit(std::array<uint8_t, 6> input) : id((input[0] >> 2) & 0xF),
                                                       data{{static_cast<uint8_t>(((input[4] & 0x03) << 6) | (input[5] & 0x3F)), static_cast<uint8_t>(((input[3] & 0x0F) << 4) | ((input[4] & 0x3B) >> 2)), static_cast<uint8_t>(((input[2] & 0x3F) << 2) | ((input[3] & 0x30) >> 4)), static_cast<uint8_t>(((input[0] & 0x03) << 6) | (input[1] & 0x3F))}} {

                                                       };
} RailcomPacket36Bit;

union railcomPacket
{
    RailcomPacket12Bit packet12Bit;
    RailcomPacket18Bit packet18Bit;
    RailcomPacket24Bit packet24Bit;
    RailcomPacket36Bit packet36Bit;
};