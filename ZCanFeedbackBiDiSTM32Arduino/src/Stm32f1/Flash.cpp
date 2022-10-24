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

#include "Stm32f1/Flash.h"

uint16_t *Flash::m_memoryDataPtr = nullptr;
size_t Flash::m_memoryDataSize = 0;

void Flash::readData(void)
{
    if (nullptr != m_memoryDataPtr)
    {
        uint16_t *memoryPtr = (uint16_t *)START_ADDRESS_MEMORY;
        uint16_t *dataPtr = Flash::m_memoryDataPtr;
        for (uint16_t i = 0; i < Flash::m_memoryDataSize; i += 2)
        {
            *dataPtr = *(volatile uint16_t *)memoryPtr;
            memoryPtr++;
            dataPtr++;
        }
    }
}

bool Flash::writeData(void)
{
    if (nullptr == m_memoryDataPtr)
    {
        return false;
    }
    static FLASH_EraseInitTypeDef eraseInitStruct{0};

    /* Unlock the Flash to enable the flash control register access *************/
    HAL_FLASH_Unlock();

    /* Erase the user Flash area */

    /* Fill EraseInit structure*/
    eraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInitStruct.PageAddress = START_ADDRESS_MEMORY; // Starting address of flash page (0x0800 0000 - 0x0801 FC00)
    eraseInitStruct.NbPages = 1;                        // The number of pages to be erased

    uint32_t errorStatus = 0;

    if (HAL_FLASHEx_Erase(&eraseInitStruct, &errorStatus) != HAL_OK)
    {
        Serial.printf("%d\n", HAL_FLASH_GetError());
        return false;
    }

    uint32_t memoryAdress = START_ADDRESS_MEMORY;
    uint16_t *dataPtr = Flash::m_memoryDataPtr;
    for (uint16_t i = 0; i < Flash::m_memoryDataSize; i += 2)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, memoryAdress, *dataPtr) == HAL_OK)
        {
            memoryAdress += 2; // use StartPageAddress += 2 for half word and 8 for double word
            dataPtr++;
        }
        else
        {
            /* Error occurred while writing data in Flash memory*/
            Serial.printf("%d\n", HAL_FLASH_GetError());
            return false;
        }
    }

    /* Lock the Flash to disable the flash control register access (recommended
       to protect the FLASH memory against possible unwanted operation) *********/
    HAL_FLASH_Lock();
    return true;
}