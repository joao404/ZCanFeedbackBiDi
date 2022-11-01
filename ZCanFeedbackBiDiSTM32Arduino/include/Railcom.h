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

#pragma once

#include "Arduino.h"

class Railcom
{
public:
    Railcom(){};
    virtual ~Railcom(){};

    static bool getStartAndStopByteOfUart(bool* bitStreamIN1samplePer1us, size_t startIndex, size_t endIndex, size_t* findStartIndex, size_t* findEndIndex);
    static uint8_t handleBitStream(bool bitStreamIN1samplePer1us[], size_t length, std::array<uint8_t, 8> &railcomAddr);

static uint8_t encode4to8[];
static uint8_t encode8to4[];
};

