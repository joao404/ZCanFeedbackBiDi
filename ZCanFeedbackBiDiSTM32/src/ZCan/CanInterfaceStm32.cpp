/*********************************************************************
 * CanInterfaceStm32
 *
 * Copyright (C) 2022 Marcel Maage
 *
 * This library is free software; you twai redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * LICENSE file for more details.
 */

#include "ZCan/CanInterfaceStm32.h"
#include <cstring>
#include <functional>

std::shared_ptr<CanInterfaceStm32> CanInterfaceStm32::m_instance;

CanInterfaceStm32::CanInterfaceStm32(bool useInterrupt, void (*printFunc)(const char *, ...))
    : m_usingInterrupt(useInterrupt)
{
    if (nullptr != printFunc)
    {
        m_printFunc = printFunc;
    }
}

CanInterfaceStm32::~CanInterfaceStm32()
{
}

std::shared_ptr<CanInterfaceStm32> CanInterfaceStm32::createInstance(bool useInterrupt, void (*printFunc)(const char *, ...))
{
    if(nullptr == CanInterfaceStm32::m_instance.get())
    {
      CanInterfaceStm32::m_instance = std::make_shared<CanInterfaceStm32>(useInterrupt, printFunc);
    }
    return CanInterfaceStm32::m_instance;
}

void CanInterfaceStm32::begin()
{
    m_printFunc("Setting up CAN...\n");
    // CAN_FilterTypeDef sFilterConfig;
    MX_CAN_Init();

    if (HAL_CAN_Start(&hcan) != HAL_OK)
    {
        /* Start Error */
        m_printFunc("HAL_CAN_Start: %d\n", HAL_CAN_GetError(&hcan));
        Error_Handler();
    }

    // HAL_CAN_RegisterCallback(&hcan, HAL_CAN_TX_MAILBOX0_COMPLETE_CB_ID, HAL_CAN_TxMailbox0CompleteCallback);
    // HAL_CAN_RegisterCallback(&hcan, HAL_CAN_RX_FIFO0_MSG_PENDING_CB_ID, HAL_CAN_RxFIFO0MsgPendingCallback);
    if (m_usingInterrupt)
    {
        if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING | CAN_IT_TX_MAILBOX_EMPTY) != HAL_OK)
        {
            /* Notification Error */
            m_printFunc("HAL_CAN_ActivateNotification: %d\n", HAL_CAN_GetError(&hcan));
            Error_Handler();
        }
    }
}

void CanInterfaceStm32::cyclic()
{
    if (!m_usingInterrupt)
    {
        CanMessage frame;
        while (receive(frame, 0))
        {
            notify(&frame);
        }
    }
    errorHandling();
}

bool CanInterfaceStm32::transmit(CanMessage &frame, uint16_t timeoutINms)
{
    CAN_TxHeaderTypeDef canFrame;
    canFrame.IDE = frame.extd ? CAN_ID_EXT : 0;
    canFrame.RTR = CAN_RTR_DATA;
    canFrame.TransmitGlobalTime = DISABLE;
    canFrame.ExtId = frame.identifier;
    canFrame.DLC = frame.data_length_code;
    bool result{false};
    uint32_t m_txMailbox;
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) != 0)
    {
        if (HAL_CAN_AddTxMessage(&hcan, &canFrame, &frame.data[0], &m_txMailbox) != HAL_OK)
        {
            m_printFunc("Error TX: %d\n", HAL_CAN_GetError(&hcan));
        }
        else
        {
            m_printFunc("%d,%d\n", m_txMailbox, HAL_CAN_IsTxMessagePending(&hcan, m_txMailbox));
            result = true;
        }
    }
    else
    {
        m_printFunc("No Free Mailbox");
    }

    return result;
}

bool CanInterfaceStm32::receive(CanMessage &frame, uint16_t timeoutINms)
{
    bool result{false};
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t data[8];
    if (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) != 0)
    {
        if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &rxHeader, data) == HAL_OK)
        {
            frame.extd = CAN_ID_EXT == rxHeader.IDE ? 1 : 0;
            frame.identifier = CAN_ID_EXT == rxHeader.IDE ? rxHeader.ExtId : rxHeader.StdId;
            frame.data_length_code = rxHeader.DLC;
            memcpy(&frame.data[0], data, 8);
            result = true;
        }
    }
    else if (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO1) != 0)
    {
        if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO1, &rxHeader, data) == HAL_OK)
        {
            frame.extd = CAN_ID_EXT == rxHeader.IDE ? 1 : 0;
            frame.identifier = CAN_ID_EXT == rxHeader.IDE ? rxHeader.ExtId : rxHeader.StdId;
            frame.data_length_code = rxHeader.DLC;
            memcpy(&frame.data[0], data, 8);
            result = true;
        }
    }
    return result;
}

void CanInterfaceStm32::handleCanReceive(CAN_RxHeaderTypeDef &frameHeader, uint8_t &data)
{
    // transmit data to CanMessage and transfer data
    CanMessage frame;
    frame.extd = CAN_ID_EXT == frameHeader.IDE ? 1 : 0;
    frame.identifier = CAN_ID_EXT == frameHeader.IDE ? frameHeader.ExtId : frameHeader.StdId;
    frame.data_length_code = frameHeader.DLC;
    memcpy(&frame.data[0], &data, 8);
    notify(&frame);
}

void CanInterfaceStm32::canReceiveInterrupt(CAN_RxHeaderTypeDef &frameHeader, uint8_t &data)
{
    if (nullptr != m_instance.get())
    {
        m_instance->handleCanReceive(frameHeader, data);
    }
}

void CanInterfaceStm32::errorHandling()
{
    if (HAL_CAN_GetError(&hcan))
    {
        m_printFunc("%d\n", HAL_CAN_GetError(&hcan));
        HAL_CAN_ResetError(&hcan);
    }
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
{
}

void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan)
{
}

void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan)
{
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t data[8];
    while (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0) != 0)
    {
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, data) == HAL_OK)
        {
            CanInterfaceStm32::canReceiveInterrupt(rxHeader, *data);
        }
        else
        {
            return;
        }
    }
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t data[8];
    while (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO1) != 0)
    {
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO1, &rxHeader, data) == HAL_OK)
        {
            CanInterfaceStm32::canReceiveInterrupt(rxHeader, *data);
        }
        else
        {
            return;
        }
    }
}