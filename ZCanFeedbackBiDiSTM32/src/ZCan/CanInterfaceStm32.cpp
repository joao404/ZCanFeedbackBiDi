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
    : m_canHandle{},
      m_usingInterrupt(useInterrupt)
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
    if (nullptr == CanInterfaceStm32::m_instance.get())
    {
        CanInterfaceStm32::m_instance = std::make_shared<CanInterfaceStm32>(useInterrupt, printFunc);
    }
    return CanInterfaceStm32::m_instance;
}

void CanInterfaceStm32::begin()
{
    m_printFunc("Setting up CAN GPIO...\n");

    __HAL_CAN_RESET_HANDLE_STATE(&m_canHandle);

    if (HAL_OK != HAL_CAN_RegisterCallback(&m_canHandle, HAL_CAN_CallbackIDTypeDef::HAL_CAN_MSPINIT_CB_ID, CanInterfaceStm32::CanMspInit))
    {
        m_printFunc("HAL_CAN_RegisterCallback MSP Init: %d\n", HAL_CAN_GetError(&m_canHandle));
        while (1)
        {
        }
    }
    if (HAL_OK != HAL_CAN_RegisterCallback(&m_canHandle, HAL_CAN_CallbackIDTypeDef::HAL_CAN_MSPDEINIT_CB_ID, CanInterfaceStm32::CanMspDeInit))
    {
        m_printFunc("HAL_CAN_RegisterCallback MSP DeInit: %d\n", HAL_CAN_GetError(&m_canHandle));
        while (1)
        {
        }
    }

    m_canHandle.Instance = CAN1;
    m_canHandle.Init.Prescaler = 14;
    m_canHandle.Init.Mode = CAN_MODE_NORMAL;
    m_canHandle.Init.SyncJumpWidth = CAN_SJW_1TQ;
    m_canHandle.Init.TimeSeg1 = CAN_BS1_13TQ;
    m_canHandle.Init.TimeSeg2 = CAN_BS2_2TQ;
    m_canHandle.Init.TimeTriggeredMode = DISABLE;
    m_canHandle.Init.AutoBusOff = ENABLE;
    m_canHandle.Init.AutoWakeUp = DISABLE;
    m_canHandle.Init.AutoRetransmission = DISABLE;
    m_canHandle.Init.ReceiveFifoLocked = DISABLE;
    m_canHandle.Init.TransmitFifoPriority = DISABLE;
    // if (HAL_CAN_DeInit(&m_canHandle) != HAL_OK)
    // {
    //     m_printFunc("HAL_CAN_DeInit: %d, %d\n", HAL_CAN_GetError(&m_canHandle), HAL_CAN_GetState(&m_canHandle));
    //     while (1)
    //     {
    //     }
    // }
    // HAL_CAN_ResetError(&m_canHandle);
    m_printFunc("HAL_CAN_Init\n");
    if (HAL_CAN_Init(&m_canHandle) != HAL_OK)
    {
        m_printFunc("HAL_CAN_Init: %d, %d\n", HAL_CAN_GetError(&m_canHandle), HAL_CAN_GetState(&m_canHandle));
        while (1)
        {
        }
    }

    CAN_FilterTypeDef sFilterConfig;
    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = 0x0000;
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x0000;
    sFilterConfig.FilterMaskIdLow = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;

    if (HAL_CAN_ConfigFilter(&m_canHandle, &sFilterConfig) != HAL_OK)
    {
        /* Filter configuration Error */
        m_printFunc("HAL_CAN_ConfigFilter: %d\n", HAL_CAN_GetError(&m_canHandle));
        while (1)
        {
        }
    }

    m_printFunc("Starting CAN...\n");

    if (HAL_CAN_Start(&m_canHandle) != HAL_OK)
    {
        /* Start Error */
        m_printFunc("HAL_CAN_Start: %d\n", HAL_CAN_GetError(&m_canHandle));
        while (1)
        {
        }
    }

    if (m_usingInterrupt)
    {
        if (HAL_OK != HAL_CAN_RegisterCallback(&m_canHandle, HAL_CAN_CallbackIDTypeDef::HAL_CAN_RX_FIFO0_MSG_PENDING_CB_ID, CanInterfaceStm32::rxFifo0MsgPendingCallback))
        {
            m_printFunc("HAL_CAN_RegisterCallback RxFiFo0: %d\n", HAL_CAN_GetError(&m_canHandle));
            while (1)
            {
            }
        }
        if (HAL_OK != HAL_CAN_RegisterCallback(&m_canHandle, HAL_CAN_CallbackIDTypeDef::HAL_CAN_RX_FIFO1_MSG_PENDING_CB_ID, CanInterfaceStm32::rxFifo1MsgPendingCallback))
        {
            m_printFunc("HAL_CAN_RegisterCallback RxFiFo1: %d\n", HAL_CAN_GetError(&m_canHandle));
            while (1)
            {
            }
        }

        // if (HAL_CAN_ActivateNotification(&m_canHandle, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING | CAN_IT_TX_MAILBOX_EMPTY) != HAL_OK)
        if (HAL_CAN_ActivateNotification(&m_canHandle, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING) != HAL_OK)
        {
            /* Notification Error */
            m_printFunc("HAL_CAN_ActivateNotification: %d\n", HAL_CAN_GetError(&m_canHandle));
            while (1)
            {
            }
        }
    }
}

void CanInterfaceStm32::cyclic()
{
    if (!m_usingInterrupt)
    {
        CanMessage frame{};
        if (receive(frame, 0))
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
    if (HAL_CAN_GetTxMailboxesFreeLevel(&m_canHandle) != 0)
    {
        if (HAL_CAN_AddTxMessage(&m_canHandle, &canFrame, &frame.data[0], &m_txMailbox) != HAL_OK)
        {
            m_printFunc("Error TX: %d\n", HAL_CAN_GetError(&m_canHandle));
        }
        else
        {
            // m_printFunc("%d,%d\n", m_txMailbox, HAL_CAN_IsTxMessagePending(&m_canHandle, m_txMailbox));
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
    uint32_t currentTimeINms = HAL_GetTick();
    do
    {
        if (HAL_CAN_GetRxFifoFillLevel(&m_canHandle, CAN_RX_FIFO0) != 0)
        {
            if (HAL_CAN_GetRxMessage(&m_canHandle, CAN_RX_FIFO0, &rxHeader, data) == HAL_OK)
            {
                frame.extd = CAN_ID_EXT == rxHeader.IDE ? 1 : 0;
                frame.identifier = CAN_ID_EXT == rxHeader.IDE ? rxHeader.ExtId : rxHeader.StdId;
                frame.data_length_code = rxHeader.DLC;
                memcpy(&frame.data[0], data, 8);
                result = true;
                break;
            }
        }
        else if (HAL_CAN_GetRxFifoFillLevel(&m_canHandle, CAN_RX_FIFO1) != 0)
        {
            if (HAL_CAN_GetRxMessage(&m_canHandle, CAN_RX_FIFO1, &rxHeader, data) == HAL_OK)
            {
                frame.extd = CAN_ID_EXT == rxHeader.IDE ? 1 : 0;
                frame.identifier = CAN_ID_EXT == rxHeader.IDE ? rxHeader.ExtId : rxHeader.StdId;
                frame.data_length_code = rxHeader.DLC;
                memcpy(&frame.data[0], data, 8);
                result = true;
                break;
            }
        }
    }
    while((currentTimeINms + (uint32_t)timeoutINms) > HAL_GetTick());
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
    // if (HAL_CAN_GetError(&m_canHandle))
    // {
    //     m_printFunc("%d\n", HAL_CAN_GetError(&m_canHandle));
    //     HAL_CAN_ResetError(&m_canHandle);
    // }
}

// void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
// {
// }

// void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
// {
// }

// void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan)
// {
// }

// void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan)
// {
// }

void CanInterfaceStm32::CanMspInit(CAN_HandleTypeDef *hcan)
{

    HAL_CAN_MspInit(hcan);
    /* Exit from sleep mode */
    CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_SLEEP);
}

void CanInterfaceStm32::CanMspDeInit(CAN_HandleTypeDef *hcan)
{
    HAL_CAN_MspDeInit(hcan);
}

void CanInterfaceStm32::rxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
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

void CanInterfaceStm32::rxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
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