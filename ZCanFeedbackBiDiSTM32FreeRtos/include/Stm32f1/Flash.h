/*********************************************************************
 * Flash
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

#define START_ADDRESS_MEMORY 0x0800FC00

class Flash
{
public:
    static void readData(void);
    static bool writeData(void);

    static uint16_t* m_memoryDataPtr;
    static size_t m_memoryDataSize;
};