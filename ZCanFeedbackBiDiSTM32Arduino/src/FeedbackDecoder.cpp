/*********************************************************************
 * Feedback Decoder
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

#define BUILDTM_YEAR (        \
    __DATE__[7] == '?' ? 1900 \
                       : (((__DATE__[7] - '0') * 1000) + (__DATE__[8] - '0') * 100 + (__DATE__[9] - '0') * 10 + __DATE__[10] - '0'))

#define BUILDTM_MONTH (                                 \
    __DATE__[2] == '?'   ? 1                            \
    : __DATE__[2] == 'n' ? (__DATE__[1] == 'a' ? 1 : 6) \
    : __DATE__[2] == 'b' ? 2                            \
    : __DATE__[2] == 'r' ? (__DATE__[0] == 'M' ? 3 : 4) \
    : __DATE__[2] == 'y' ? 5                            \
    : __DATE__[2] == 'l' ? 7                            \
    : __DATE__[2] == 'g' ? 8                            \
    : __DATE__[2] == 'p' ? 9                            \
    : __DATE__[2] == 't' ? 10                           \
    : __DATE__[2] == 'v' ? 11                           \
                         : 12)

#define BUILDTM_DAY (      \
    __DATE__[4] == '?' ? 1 \
                       : ((__DATE__[4] == ' ' ? 0 : ((__DATE__[4] - '0') * 10)) + __DATE__[5] - '0'))

#include "FeedbackDecoder.h"
#include <algorithm>
#include <cstring>
#include "Arduino.h"

FeedbackDecoder::FeedbackDecoder(ModulConfig &modulConfig, bool (*saveDataFkt)(void), std::array<int, 8> &trackPin, bool hasRailcom,
                                 int configRailcomPin, int configIdPin, bool debug, bool zcanDebug, void (*printFunc)(const char *, ...))
    : ZCanInterfaceObserver(zcanDebug, printFunc),
      m_debug(debug),
      m_saveDataFkt(saveDataFkt),
      m_hasRailcom(hasRailcom),
      m_configRailcomPin(configRailcomPin),
      m_configIdPin(configIdPin),
      m_modulId(0x0),
      m_idPrgStartTimeINms(0),
      m_idPrgRunning(false),
      m_idPrgIntervalINms(60000), // 1 min
      m_lastCanCmdSendINms(0),
      m_pingJitterINms(0),
      m_pingIntervalINms(0),
      m_masterId(0x0),
      m_sessionId(0x0),
      m_currentRailcomPort(0),
      m_currentMeasurementPerPort(0),
      m_maxNumberOfConsecutiveMeasurements(4),
      m_processingRailcomData(false),
      m_modulConfig(modulConfig),
      m_firmwareVersion(0x05010014), // 5.1.20
      m_buildDate(0x07E60917),       // 23.09.2022
      m_hardwareVersion(0x05010001)  // 5.1.1
{
    auto sizeTrackData = m_trackData.size();
    auto sizeTrackPin = trackPin.size();
    for (uint8_t i = 0; (i < sizeTrackData) && (i < sizeTrackPin); ++i)
    {
        m_trackData[i].pin = trackPin[i];
        memset(m_trackData[i].adress.begin(), 0, sizeof(TrackData::adress));
        m_trackData[i].changeReported = true; // first report is already done
    }
}

FeedbackDecoder::~FeedbackDecoder()
{
}

void FeedbackDecoder::begin()
{
	pinMode(m_configIdPin, INPUT_PULLUP);
	
    if (0xFFFF == m_modulConfig.networkId)
    {
        // memory not set before
        // write default values
        uint32_t timeINus = micros();
        if (timeINus > (uint32_t)(modulNidMax - modulNidMin))
        {
            m_modulConfig.networkId = modulNidMin + std::max((uint32_t)1, (timeINus / 8));
        }
        else
        {
            m_modulConfig.networkId = modulNidMin + std::max((uint32_t)1, timeINus);
        }
        m_modulConfig.modulAdress = 0x00;
        // for (auto finding = m_modulConfig.trackConfig.begin(); finding != m_modulConfig.trackConfig.end(); ++finding)
        m_modulConfig.trackConfig.trackSetCurrentINmA = 10;
        m_modulConfig.trackConfig.trackFreeToSetTimeINms = 10;
        m_modulConfig.trackConfig.trackSetToFreeTimeINms = 1000;
        m_modulConfig.sendChannel2Data = 0;
        m_saveDataFkt();
    }

    m_networkId = m_modulConfig.networkId;
    m_modulId = m_modulConfig.modulAdress;
    uint32_t day = BUILDTM_DAY;
    uint32_t month = BUILDTM_MONTH;
    uint32_t year = BUILDTM_YEAR;
    m_buildDate = (year << 16) | (month << 8) | day;
    // 3300mV per 4096 bits is 0.8mVperCount
    // I have a 22 Ohm resistor so 0.8mV per Count * 22 * current is offset down below
    // so I take 8*22 = 17,6 is round about 18
    m_trackSetVoltage = 18 * m_modulConfig.trackConfig.trackSetCurrentINmA;
    m_printFunc("SW Version: 0x%08X, build date: 0x%08X\n", m_firmwareVersion, m_buildDate);
    m_printFunc("NetworkId %x MA %x CH2 %x\n", m_networkId, m_modulId, m_modulConfig.sendChannel2Data);
    m_printFunc("trackSetCurrentINmA: %d\n", m_modulConfig.trackConfig.trackSetCurrentINmA);
    m_printFunc("trackFreeToSetTimeINms: %d\n", m_modulConfig.trackConfig.trackFreeToSetTimeINms);
    m_printFunc("trackSetToFreeTimeINms: %d\n", m_modulConfig.trackConfig.trackSetToFreeTimeINms);

    m_pingJitterINms = std::max((uint32_t)0, std::min((micros() / 10), (uint32_t)100));
    m_pingIntervalINms = (9990 - m_pingJitterINms);

    ZCanInterfaceObserver::begin();

    if (m_hasRailcom)
    {
		pinMode(m_configRailcomPin, INPUT_PULLUP);
        //  this is done outside of FeedbackDecoder
        if (!digitalRead(m_configRailcomPin))
        {
            // ADC_ChannelConfTypeDef sConfig = {0};
            // hadc1.Instance = ADC1;
            // hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
            // hadc1.Init.ContinuousConvMode = DISABLE;
            // hadc1.Init.DiscontinuousConvMode = DISABLE;
            // hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
            // hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
            // hadc1.Init.NbrOfConversion = 1;
            // if (HAL_ADC_Init(&hadc1) != HAL_OK)
            // {
            //     Error_Handler();
            // }
            // // read current values of adcs as default value
            // for (uint8_t port = 0; port < m_trackData.size(); ++port)
            // {
            //     sConfig.Channel = m_trackData[port].pin.adcChannel;
            //     sConfig.Rank = ADC_REGULAR_RANK_1;
            //     sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
            //     if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
            //     {
            //         Error_Handler();
            //     }
            //     // Start ADC Conversion
            //     HAL_ADC_Start(&hadc1);
            //     // Poll ADC1 Perihperal & TimeOut = 1mSec
            //     HAL_ADC_PollForConversion(&hadc1, 1);
            //     // Read The ADC Conversion Result & Map It To PWM DutyCycle
            //     m_trackData[port].voltageOffset = HAL_ADC_GetValue(&hadc1);
            //     m_modulConfig.voltageOffset[port] = m_trackData[port].voltageOffset;
            // }
            // m_saveDataFkt();
            // // use default ADC function again
            // MX_ADC1_Init();
        }

        // ADC_ChannelConfTypeDef sConfig = {0};
        // sConfig.Channel = m_trackData[m_currentRailcomPort].pin.adcChannel;
        // sConfig.Rank = ADC_REGULAR_RANK_1;
        // sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
        // if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
        // {
        //     m_printFunc("Channel config failed\n");
        //     Error_Handler();
        // }

        // TODO remove when functioning
        m_trackData[0].adress[0] = 0x8022;
        m_trackData[2].adress[0] = 0x0023;
        m_trackData[4].adress[0] = 0xC024;
        m_trackData[7].adress[0] = 0x8025;

        m_trackData[0].state = true;
        m_trackData[2].state = true;
        m_trackData[4].state = true;
        m_trackData[6].state = true;
        m_trackData[7].state = true;
    }
    else
    {
        for (uint8_t port = 0; port < m_trackData.size(); ++port)
        {
            pinMode(m_trackData[port].pin, INPUT_PULLUP);
            m_trackData[port].state = digitalRead(m_trackData[port].pin);
            notifyBlockOccupied(port, 0x01, m_trackData[port].state);
            m_trackData[port].lastChangeTimeINms = millis();
            delay(10);
            if (m_debug)
                m_printFunc("port: %d state:%d\n", port, m_trackData[port].state);
        }
    }

    // this is done outside of FeedbackDecoder
    // pinMode(m_configIdPin, INPUT_PULLUP);

    // Wait random time before starting logging to Z21
    delay(millis());

    // Send first ping
    sendPing(m_masterId, m_modulType, m_sessionId);

    m_printFunc("%d finished config\n", m_networkId);
}

void FeedbackDecoder::cyclic()
{
    unsigned long currentTimeINms{millis()};
    if ((m_lastCanCmdSendINms + m_pingIntervalINms) < currentTimeINms)
    {
        sendPing(m_masterId, m_modulType, m_sessionId);
        m_lastCanCmdSendINms = currentTimeINms;
    }
    if (m_idPrgRunning)
    {
        if ((m_idPrgStartTimeINms + m_idPrgIntervalINms) < currentTimeINms)
        {
            m_idPrgRunning = false;
        }
    }
    if (!digitalRead(m_configIdPin))
    {
        // button pressed
        m_idPrgRunning = true;
        m_idPrgStartTimeINms = currentTimeINms;
    }

    for (uint8_t port = 0; port < m_trackData.size(); ++port)
    {
        bool state = digitalRead(m_trackData[port].pin);
        if (state != m_trackData[port].state)
        {
            m_trackData[port].changeReported = false;
            m_trackData[port].state = state;
            m_trackData[port].lastChangeTimeINms = currentTimeINms;
        }
        if (!m_trackData[port].changeReported)
        {
            if (state)
            {
                if ((m_trackData[port].lastChangeTimeINms + m_modulConfig.trackConfig.trackFreeToSetTimeINms) < currentTimeINms)
                {
                    m_trackData[port].changeReported = true;
                    notifyBlockOccupied(port, 0x01, state);
                    if (m_debug)
                        m_printFunc("port: %d state:%d\n", port, state);
                }
            }
            else
            {
                if ((m_trackData[port].lastChangeTimeINms + m_modulConfig.trackConfig.trackSetToFreeTimeINms) < currentTimeINms)
                {
                    m_trackData[port].changeReported = true;
                    notifyBlockOccupied(port, 0x01, state);
                    if (m_debug)
                        m_printFunc("port: %d state:%d\n", port, state);
                }
            }
        }
    }

    if (m_processingRailcomData)
    {
        // process Railcom data from ADC
        // memory has size of 4096
        // adc has 12mhz
        // with 64mhz it would take 472us to fill half and 944 us to fill all
        // with low frequency it would take 5ms
        // in old sequence, one conversion is 230ns, no it takes around 1,22us
        // with 14 mhz we would have sampling of 1,05us which would be better
        // In that case we would need to run chip at 56mhz to have prescaler 4
        // dies wäre möglich mit multiplikator 7
        // da der buffer mit 4096 für 4ms reichen würde, das Fenster jedoch nur 450 us lang. Also reicht ein Buffer von
        // 512 werten

        // m_bitStreamDataBuffer;
        auto iteratorBit = m_bitStreamDataBuffer.begin();
        for (auto iteratorDma = m_adcDmaBuffer.begin(); iteratorDma != m_adcDmaBuffer.end(); iteratorDma++, iteratorBit++)
        {
            *iteratorBit = (abs(*iteratorDma - m_trackData[m_currentRailcomPort].voltageOffset) > m_trackSetVoltage);
            m_printFunc("%d\n", *iteratorBit);
        }

        /*
        byte data = RailComDecodeInData();   //read Railcom Data and decode Railcom Data
      if (data < 0x40) {    //gültige Railcom Message empfangen
            if (RailComReadFirst) {
              RailComReadData = data;   //Save first Byte of Data
              if ((data >> 2) < 3)  //app:pom, app:adr_low, app:adr_high -> read only 12 Bit!
                RailComReadFirst = false;
            }
            else {
              RailComReadFirst = true;
              byte RailComID = RailComReadData >> 2; //Save ID
              RailComReadData = (RailComReadData << 6) | data;    //first 2 Bit and add next 6 Bit
              if (RailComID == 0) { //app:pom
                  RailComCVTime = 25; //Reset Timer!
                  RailComReadLastCV = RailComReadData;
                  RailComCVRead = true;
              }
              else if (RailComID == 1) { //app:adr_high
                RailComReadAdr = (RailComReadData << 8) | (RailComReadAdr & 0xFF);
              }
              else if (RailComID == 2) {  //app:adr_low
                RailComReadAdr = (RailComReadAdr & 0xFF00) | RailComReadData;
              }
            }
      }
      else RailComReadFirst = true;
      */

        // m_processingRailcomData = false;
    }
}

void FeedbackDecoder::callbackAccAddrReceived(uint16_t addr)
{
    if (m_idPrgRunning)
    {
        m_modulId = addr;
        m_modulConfig.modulAdress = m_modulId;
        m_idPrgRunning = false;
        m_saveDataFkt();
    }
}

void FeedbackDecoder::callbackLocoAddrReceived(uint16_t addr)
{
    // start detection of Railcom
    if (m_hasRailcom)
    {
        if (!m_processingRailcomData)
        {
            // HAL_ADC_Start_DMA(&hadc1, (uint32_t *)m_adcDmaBuffer.begin(), sizeof(m_adcDmaBuffer));
        }
    }
}

// void FeedbackDecoder::callbackAdcReadFinished(ADC_HandleTypeDef *hadc)
// {
//     m_processingRailcomData = true;
//     m_currentMeasurementPerPort++;
//     if (m_maxNumberOfConsecutiveMeasurements <= m_currentMeasurementPerPort)
//     {
//         m_currentMeasurementPerPort = 0;
//         m_currentRailcomPort++;
//     }
//     if (sizeof(m_trackData) <= m_currentRailcomPort)
//     {
//         m_currentRailcomPort = 0;
//     }
//     // after DMA was executed, configure next channel already to save time
//     ADC_ChannelConfTypeDef sConfig = {0};
//     sConfig.Channel = m_trackData[m_currentRailcomPort].pin.adcChannel;
//     sConfig.Rank = ADC_REGULAR_RANK_1;
//     sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
//     if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
//     {
//         Error_Handler();
//     }
// }

bool FeedbackDecoder::notifyLocoInBlock(uint8_t port, uint16_t adress1, uint16_t adress2, uint16_t adress3, uint16_t adress4)
{
    bool result = sendAccessoryDataEvt(m_modulId, port, 0x11, adress1, adress2);
    result &= sendAccessoryDataEvt(m_modulId, port, 0x12, adress3, adress4);
    return result;
}

bool FeedbackDecoder::notifyBlockOccupied(uint8_t port, uint8_t type, bool occupied)
{
    uint16_t value = occupied ? 0x1100 : 0x0100;
    return sendAccessoryPort6Evt(m_modulId, port, type, value);
}

void FeedbackDecoder::onIdenticalNetworkId()
{
    // received own network id. Generate new random network id
    m_modulConfig.networkId = modulNidMin + std::max((uint16_t)1, (uint16_t)(HAL_GetTick() % (modulNidMax - modulNidMin)));
    m_saveDataFkt();
    sendPing(m_masterId, m_modulType, m_sessionId);
}

bool FeedbackDecoder::onAccessoryData(uint16_t accessoryId, uint8_t port, uint8_t type)
{
    bool result{false};
    if ((accessoryId == m_modulId) || ((accessoryId & 0xF000) == modulNidMin))
    {
        if (port < m_trackData.size())
        {
            if (0x11 == type)
            {
                if (m_debug)
                    m_printFunc("onAccessoryData\n");
                result = sendAccessoryDataAck(m_modulId, port, type, m_trackData[port].adress[0], m_trackData[port].adress[1]);
            }
            else if (0x12 == type)
            {
                if (m_debug)
                    m_printFunc("onAccessoryData\n");
                result = sendAccessoryDataAck(m_modulId, port, type, m_trackData[port].adress[2], m_trackData[port].adress[3]);
            }
        }
    }
    return result;
}

bool FeedbackDecoder::onAccessoryPort6(uint16_t accessoryId, uint8_t port, uint8_t type)
{
    bool result{false};
    if ((accessoryId == m_modulId) || ((accessoryId & 0xF000) == modulNidMin))
    {
        if (0x1 == type)
        {
            if (port < m_trackData.size())
            {
                result = sendAccessoryDataAck(m_modulId, port, type, m_trackData[port].state ? 0x1100 : 0x0100, 0);
                if (m_debug)
                    m_printFunc("onAccessoryPort6\n");
            }
        }
    }
    return result;
}

bool FeedbackDecoder::onRequestModulInfo(uint16_t id, uint16_t type)
{
    bool result{false};
    if (id == m_networkId)
    {
        if (m_debug)
            m_printFunc("onRequestModulInfo\n");
        result = true;
        switch (type)
        {
        case 1: // HwVersion
            sendModuleInfoAck(m_modulId, type, m_hardwareVersion);
            break;
        case 2: // SwVersion
            sendModuleInfoAck(m_modulId, type, m_firmwareVersion);
            break;
        case 3: // Build Date
            sendModuleInfoAck(m_modulId, type, m_buildDate);
            break;
        case 4:
            sendModuleInfoAck(m_modulId, type, 0x00010200);
            break;
        case 20: // modul Id
            sendModuleInfoAck(m_modulId, type, m_modulId);
            break;
        case 100:
            sendModuleInfoAck(m_modulId, type, m_modulType);
            break;

        default:
            result = false;
            break;
        }
    }
    return result;
}

bool FeedbackDecoder::onModulPowerInfoEvt(uint16_t nid, uint8_t port, uint16_t status, uint16_t voltageINmV, uint16_t currentINmA)
{
    if (m_debug)
    {
        m_printFunc("onModulPowerInfoEvt\n");
    }
    return true;
}

bool FeedbackDecoder::onModulPowerInfoAck(uint16_t nid, uint8_t port, uint16_t status, uint16_t voltageINmV, uint16_t currentINmA)
{
    if (m_debug)
    {
        m_printFunc("onModulPowerInfoAck\n");
    }
    return true;
}

bool FeedbackDecoder::onCmdModulInfo(uint16_t id, uint16_t type, uint32_t info)
{
    bool result{false};
    if (id == m_networkId)
    {
        if (m_debug)
            m_printFunc("onCmdModulInfo\n");
        result = true;
        switch (type)
        {
        case 20:
            m_modulConfig.modulAdress = (((info >> 8) & 0xFF) << 8) | (info & 0xFF);
            if (m_saveDataFkt())
            {
                m_modulId = m_modulConfig.modulAdress;
            }
            sendModuleInfoAck(m_modulId, type, m_modulId);
            break;

        default:
            result = false;
            break;
        }
    }
    return result;
}

bool FeedbackDecoder::onRequestModulObjectConfig(uint16_t id, uint32_t tag)
{
    // send requested configuration
    bool result{false};
    if (id == m_networkId)
    {
        if (m_debug)
            m_printFunc("onRequestModulObjectConfig\n");
        uint16_t value{0};
        switch (tag)
        {
        case 0x00221001:
        case 0x00221002:
        case 0x00221003:
        case 0x00221004:
        case 0x00221005:
        case 0x00221006:
        case 0x00221007:
        case 0x00221008:
            value = (m_modulConfig.sendChannel2Data ? 0x0010 : 0x0000) | 0x0001;
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00401001:
        case 0x00401002:
        case 0x00401003:
        case 0x00401004:
        case 0x00401005:
        case 0x00401006:
        case 0x00401007:
        case 0x00401008:
            value = m_modulConfig.trackConfig.trackSetCurrentINmA;
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00501001:
        case 0x00501002:
        case 0x00501003:
        case 0x00501004:
        case 0x00501005:
        case 0x00501006:
        case 0x00501007:
        case 0x00501008:
            value = m_modulConfig.trackConfig.trackFreeToSetTimeINms;
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00511001:
        case 0x00511002:
        case 0x00511003:
        case 0x00511004:
        case 0x00511005:
        case 0x00511006:
        case 0x00511007:
        case 0x00511008:
            value = m_modulConfig.trackConfig.trackSetToFreeTimeINms;
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        default:
            break;
        }
    }
    return result;
}

bool FeedbackDecoder::onCmdModulObjectConfig(uint16_t id, uint32_t tag, uint16_t value)
{
    // write requested configuration
    // send requested configuration
    bool result{false};
    if (id == m_networkId)
    {
        switch (tag)
        {
        case 0x00221001:
            m_modulConfig.sendChannel2Data = ((value & 0x0010) == 0x0010) ? 1 : 0;
            if (m_debug)
                m_printFunc("Write Send Channel 2 %u\n", m_modulConfig.sendChannel2Data);
            m_saveDataFkt();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00401001:
            m_modulConfig.trackConfig.trackSetCurrentINmA = value;
            m_trackSetVoltage = 18 * value;
            if (m_debug)
            {
                m_printFunc("Write SetCurrent %u\n", m_modulConfig.trackConfig.trackSetCurrentINmA);
                m_printFunc("Write track set voltage %u\n", m_trackSetVoltage);
            }
            m_saveDataFkt();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00501001:
            m_modulConfig.trackConfig.trackFreeToSetTimeINms = value;
            if (m_debug)
                m_printFunc("Write FreeToSetTime %u\n", m_modulConfig.trackConfig.trackFreeToSetTimeINms);
            m_saveDataFkt();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        case 0x00511001:
            m_modulConfig.trackConfig.trackSetToFreeTimeINms = value;
            if (m_debug)
                m_printFunc("Write SetToFreeTime %u\n", m_modulConfig.trackConfig.trackSetToFreeTimeINms);
            m_saveDataFkt();
            result = sendModuleObjectConfigAck(m_modulId, tag, value);
            break;

        default:
            // all other values are handled
            m_printFunc("Handle tag %x\n", tag);
            result = true;
            break;
        }
    }
    return result;
}

bool FeedbackDecoder::onRequestPing(uint16_t id)
{
    bool result{false};
    if ((id == m_modulId) || ((id & 0xF000) == modulNidMin))
    {
        result = sendPing(m_masterId, m_modulType, m_sessionId);
    }
    return result;
}

bool FeedbackDecoder::onPing(uint16_t id, uint32_t masterUid, uint16_t type, uint16_t sessionId)
{
    if ((masterUid != m_masterId) && (0x2000 == (type & 0xFF00)))
    {
        if (m_debug)
        {
            m_printFunc("New master %x\n", masterUid);
        }
        m_masterId = masterUid;
        m_sessionId = sessionId;
        sendPing(m_masterId, m_modulType, m_sessionId);
    }
    return true;
}

bool FeedbackDecoder::sendMessage(ZCanMessage &message)
{
    m_lastCanCmdSendINms = HAL_GetTick();
    return ZCanInterfaceObserver::sendMessage(message);
}