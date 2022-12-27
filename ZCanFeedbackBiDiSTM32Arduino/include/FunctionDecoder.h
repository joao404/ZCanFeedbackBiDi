/*********************************************************************
 * FunctionDecoder
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

#include "Arduino.h"
#include <Servo.h>

template <std::size_t N>
class FunctionDecoder
{
public:
    enum class Type : uint8_t
    {
        Switch = 0,
        Blink = 1,
        Servo = 2,
        DblLedBlink = 3,
        Pulsed = 4,
        Fade = 5
    };

    typedef struct
    {
        Type type;
        uint8_t rate;
        uint8_t startPosition;
        uint8_t endPosition;
    } FunctionConfig;

    typedef struct
    {
        uint16_t address;
        std::array<FunctionConfig, N> functionConfig;
    } Config;

public:
    FunctionDecoder(Config &config, std::function<bool(void)> saveDataFkt, std::array<int, N> &functionPin, int configIdPin,
                    void (*printFunc)(const char *, ...) = nullptr, bool debug = false);
    virtual ~FunctionDecoder();

    void begin(bool decoderReset = false);

    void cyclic();

    void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t funcState);

    uint8_t notifyCVValid(uint16_t cv, uint8_t writable);
    uint8_t notifyCVRead(uint16_t cv);
    uint8_t notifyCVWrite(uint16_t cv, uint8_t value);

protected:
    bool m_debug;

    void (*m_printFunc)(const char *, ...);

    std::function<bool(void)> m_saveDataFkt;

    const uint16_t m_resetCv{20u};
    const uint16_t m_baseCV{30u};

    const uint16_t m_functionConfigSize{4u};

    Config &m_config;

    int m_configIdPin;

    uint32_t m_idPrgStartTimeINms{0};

    uint32_t m_idPrgIntervalINms{60000}; // 1 min

    bool m_idPrgRunning{false};

    struct FunctionState
    {
        int inUse;
        int currentPosition;
        int increment;
        int stopValue;
        int startValue;
        int firstCmdReceived;
    };

    std::array<FunctionState, N> m_functionState;

    std::array<int, N> &m_functionPin;

    Servo servo[N];

    const int m_servoStartDelay{50};
    const int m_servoInitDelay{7};
    const int m_servoSlowdown{12}; // servo loop counter limit
    int m_servoSlowCounter{0};     // servo loop counter to slowdown servo transit
};

template <std::size_t N>
FunctionDecoder<N>::FunctionDecoder(Config &config, std::function<bool(void)> saveDataFkt, std::array<int, N> &functionPin,
                                    int configIdPin, void (*printFunc)(const char *, ...), bool debug)
    : m_debug(debug),
      m_printFunc(printFunc),
      m_saveDataFkt(saveDataFkt),
      m_config(config),
      m_configIdPin(configIdPin),
      m_functionPin(functionPin)
{
}

template <std::size_t N>
FunctionDecoder<N>::~FunctionDecoder()
{
}

template <std::size_t N>
void FunctionDecoder<N>::begin(bool decoderReset)
{
    pinMode(m_configIdPin, INPUT_PULLUP);

    const int timDelay = 100;
    // initialize the digital pins as outputs
    for (auto &pin : m_functionPin)
    {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, 0);
    }
    for (auto &pin : m_functionPin)
    {
        digitalWrite(pin, 1);
        delay(timDelay / 10);
    }
    delay(timDelay);
    for (auto &pin : m_functionPin)
    {
        digitalWrite(pin, 0);
        delay(timDelay / 10);
    }
    delay(timDelay);

    if (decoderReset)
    {
        m_config.address = 0x01;

        for (auto &functionConfig : m_config.functionConfig)
        {
            functionConfig.type = Type::Servo;
            functionConfig.rate = 8;
            functionConfig.startPosition = 28;
            functionConfig.endPosition = 140;
        }
        m_saveDataFkt();
    }
    for (int i = 0; i < m_functionState.size(); i++)
    {
        switch (m_config.functionConfig[i].type)
        {
        case Type::Switch: // LED on/off
            m_functionState[i].inUse = 0;
            break;
        case Type::Blink: // LED Blink
        {
            m_functionState[i].inUse = 0;
            m_functionState[i].currentPosition = 0;
            m_functionState[i].startValue = 0;
            m_functionState[i].increment = int(char(m_config.functionConfig[i].rate));
            digitalWrite(m_functionPin[i], 0);
            m_functionState[i].stopValue = int(m_config.functionConfig[i].endPosition);
        }
        break;
        case Type::Servo: // servo
        {
            m_functionState[i].currentPosition = int(m_config.functionConfig[i].startPosition); // TODO
            m_functionState[i].stopValue = int(m_config.functionConfig[i].endPosition);
            m_functionState[i].startValue = int(m_config.functionConfig[i].startPosition);
            m_functionState[i].increment = -int(char(m_config.functionConfig[i].rate));
            // attaches servo on pin to the servo object
            servo[i].attach(m_functionPin[i]);
            servo[i].write(m_functionState[i].startValue);
            delay(m_servoInitDelay * m_servoStartDelay);
            m_functionState[i].inUse = 0;
            servo[i].detach();
        }
        break;
        case Type::DblLedBlink: // DOUBLE ALTERNATING LED Blink
        {
            m_functionState[i].inUse = 0;
            m_functionState[i].currentPosition = 0;
            m_functionState[i].startValue = 0;
            m_functionState[i].increment = m_config.functionConfig[i].rate;
            digitalWrite(m_functionPin[i], 0);
            digitalWrite(m_functionPin[i + 1], 0);
            m_functionState[i].stopValue = int(m_config.functionConfig[i].endPosition);
        }
        break;
        case Type::Pulsed: // Simple Pulsed Output based on saved Rate =10*Rate in Milliseconds
        {
            m_functionState[i].inUse = 0;
            m_functionState[i].currentPosition = 0;
            m_functionState[i].increment = 10 * int(char(m_config.functionConfig[i].rate));
            digitalWrite(m_functionPin[i], 0);
        }
        break;
        case Type::Fade: // Fade On
        {
            m_functionState[i].inUse = 0;
            m_functionState[i].startValue = 0;
            m_functionState[i].increment = int(char(m_config.functionConfig[i].rate));
            digitalWrite(m_functionPin[i], 0);
            m_functionState[i].stopValue = int(m_config.functionConfig[i].endPosition) * 10.;
        }
        break;
        default:
            break;
        }
    }
}

template <std::size_t N>
void FunctionDecoder<N>::cyclic()
{
    unsigned long currentTimeINms{millis()};
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

    for (int i = 0; i < m_functionState.size(); i++)
    {
        if (m_functionState[i].inUse == 1)
        {
            switch (m_config.functionConfig[i].type)
            {
            case Type::Switch:
                break;
            case Type::Blink:
                m_functionState[i].currentPosition = m_functionState[i].currentPosition + m_functionState[i].increment;
                if (m_functionState[i].currentPosition > m_functionState[i].stopValue)
                {
                    m_functionState[i].startValue = ~m_functionState[i].startValue;
                    digitalWrite(m_functionPin[i], m_functionState[i].startValue);
                    m_functionState[i].currentPosition = 0;
                    m_functionState[i].stopValue = int(m_config.functionConfig[i].endPosition);
                }
                break;
            case Type::Servo:
            {
                if (m_servoSlowCounter++ > m_servoSlowdown)
                {
                    m_functionState[i].currentPosition = m_functionState[i].currentPosition + m_functionState[i].increment;
                    if (m_functionState[i].increment > 0)
                    {
                        if (m_functionState[i].currentPosition > m_functionState[i].stopValue)
                        {
                            m_functionState[i].currentPosition = m_functionState[i].stopValue;
                            m_functionState[i].inUse = 0;
                            servo[i].detach();
                        }
                    }
                    if (m_functionState[i].increment < 0)
                    {
                        if (m_functionState[i].currentPosition < m_functionState[i].startValue)
                        {
                            m_functionState[i].currentPosition = m_functionState[i].startValue;
                            m_functionState[i].inUse = 0;
                            servo[i].detach();
                        }
                    }
                    servo[i].write(m_functionState[i].currentPosition);
                    m_servoSlowCounter = 0;
                }
            }
            break;
            case Type::DblLedBlink:
                m_functionState[i].currentPosition = m_functionState[i].currentPosition + m_functionState[i].increment;
                if (m_functionState[i].currentPosition > m_functionState[i].stopValue)
                {
                    m_functionState[i].startValue = ~m_functionState[i].startValue;
                    digitalWrite(m_functionPin[i], m_functionState[i].startValue);
                    digitalWrite(m_functionPin[i] + 1, ~m_functionState[i].startValue);
                    m_functionState[i].currentPosition = 0;
                    m_functionState[i].stopValue = int(m_config.functionConfig[i].endPosition);
                }
                i++;
                break;
            case Type::Pulsed: // Simple Pulsed Output based on saved Rate =10*Rate in Milliseconds
            {
                m_functionState[i].inUse = 0;
                m_functionState[i].currentPosition = 0;
                m_functionState[i].increment = 10 * int(char(m_config.functionConfig[i].rate));
                digitalWrite(m_functionPin[i], 0);
            }
            break;
            case Type::Fade: // Fade On

                break;
            default:
                break;
            }
        }
    }
}

template <std::size_t N>
void FunctionDecoder<N>::notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t funcState)
{
    if (m_idPrgRunning)
    {
        m_config.address = Addr;
        m_idPrgRunning = false;
        m_saveDataFkt();
        if (m_debug)
        {
            m_printFunc("Base address of turnouts changed to 0x%x\n", Addr);
        }
    }
    else if (Addr >= m_config.address && Addr < m_config.address + m_functionPin.size())
    {
#ifdef DEBUG
        Serial.print("Addr = ");
        Serial.println(Addr);
        Serial.print("Direction = ");
        Serial.println(Direction);
#endif
        uint16_t function{static_cast<uint16_t>(Addr - m_config.address)};
        int pin{m_functionPin[function]};
        switch (m_config.functionConfig[function].type)
        {                  // Config 0=On/Off,1=Blink,2=Servo,3=DBL LED Blink,4=Pulsed,5=fade
        case Type::Switch: // On - Off LED
            digitalWrite(pin, funcState);
            m_functionState[function].inUse = 0;
            break;
        case Type::Blink: // Blinking LED
            if ((m_functionState[function].inUse == 0) && (funcState == 1))
            {
                m_functionState[function].inUse = 1;
                m_functionState[function].startValue = 0;
                digitalWrite(pin, 0);
                m_functionState[function].stopValue = int(m_config.functionConfig[function].endPosition);
            }
            else
            {
                if ((m_functionState[function].inUse == 1) && (funcState == 0))
                {
                    m_functionState[function].inUse = 0;
                    digitalWrite(pin, 0);
                }
            }
            break;
        case Type::Servo: // Servo
            if (m_functionState[function].inUse == 0)
            {
                m_functionState[function].inUse = 1;
                servo[function].attach(pin);
            }
            if (funcState == 1)
                m_functionState[function].increment = char(m_config.functionConfig[function].rate);
            else
                m_functionState[function].increment = -char(m_config.functionConfig[function].rate);
            if (funcState == 1)
                m_functionState[function].stopValue = m_config.functionConfig[function].endPosition;
            else
                m_functionState[function].stopValue = m_config.functionConfig[function].startPosition;
            break;
        case Type::DblLedBlink: // Blinking LED PAIR
            if ((m_functionState[function].inUse == 0) && (funcState == 1))
            {
                m_functionState[function].inUse = 1;
                m_functionState[function].startValue = 0;
                digitalWrite(m_functionPin[function], 0);
                digitalWrite(m_functionPin[function + 1], 1);
                m_functionState[function].stopValue = int(m_config.functionConfig[function].endPosition);
            }
            else
            {
                if (funcState == 0)
                {
                    m_functionState[function].inUse = 0;
                    digitalWrite(m_functionPin[function], 0);
                    digitalWrite(m_functionPin[function + 1], 0);
                }
            }
            break;
        case Type::Pulsed: // Pulse Output based on Rate*10 Milliseconds
            if ((m_functionState[function].inUse == 0) && (funcState == 1))
            { // First Turn On Detected
                digitalWrite(m_functionPin[function], 1);
                delay(10 * m_functionState[function].increment);
                digitalWrite(m_functionPin[function], 0);
                m_functionState[function].inUse = 1; // inUse set to 1 says we already pulsed
            }
            else if (funcState == 0)
                m_functionState[function].inUse = 0;
            break;
        case Type::Fade: // Fade On
#define fadedelay 24
            if ((m_functionState[function].inUse == 0) && (funcState == 1))
            {
                m_functionState[function].inUse = 1;
                for (uint8_t t = 0; t < m_functionState[function].stopValue; t += m_functionState[function].increment)
                {
                    digitalWrite(m_functionPin[function], 1);
                    delay(fadedelay * (t / (1. * m_functionState[function].stopValue)));
                    digitalWrite(m_functionPin[function], 0);
                    delay(fadedelay - (fadedelay * (t / (1. * m_functionState[function].stopValue))));
                }
                digitalWrite(m_functionPin[function], 1);
            }
            else
            {
                if ((m_functionState[function].inUse == 1) && (funcState == 0))
                {
                    m_functionState[function].inUse = 0;
                    digitalWrite(m_functionPin[function], 0);
                }
            }
            break;
        default:
            m_functionState[function].inUse = 0;
            break;
        }
    }
}

template <std::size_t N>
uint8_t FunctionDecoder<N>::notifyCVValid(uint16_t cv, uint8_t writable)
{
    uint8_t returnValue{0};
    if (cv == 1)
    {
        returnValue = 1;
    }
    else if (cv == m_resetCv)
    {
        returnValue = 1;
    }
    else if ((cv >= m_baseCV) && (cv < (m_baseCV + m_functionConfigSize * N)))
    {
        returnValue = 1;
    }
    return returnValue;
}

template <std::size_t N>
uint8_t FunctionDecoder<N>::notifyCVRead(uint16_t cv)
{
    uint8_t returnValue{0};
    if (cv == 1)
    {
        returnValue = m_config.address;
    }
    else if ((cv >= m_baseCV) && (cv < (m_baseCV + m_functionConfigSize * N)))
    {
        uint16_t cvNumber{static_cast<uint16_t>(cv - m_baseCV)};
        uint16_t parameter{static_cast<uint16_t>(cvNumber % m_functionConfigSize)};
        uint16_t function{static_cast<uint16_t>(cvNumber / m_functionConfigSize)};
        switch (parameter)
        {
        case 0:
            returnValue = static_cast<uint16_t>(m_config.functionConfig[function].type);
            break;
        case 1:
            returnValue = m_config.functionConfig[function].rate;
            break;
        case 2:
            returnValue = m_config.functionConfig[function].startPosition;
            break;
        case 3:
            returnValue = m_config.functionConfig[function].endPosition;
            break;
        default:
            break;
        }
    }
    return returnValue;
}

template <std::size_t N>
uint8_t FunctionDecoder<N>::notifyCVWrite(uint16_t cv, uint8_t value)
{
    uint8_t returnValue{static_cast<uint8_t>(value + 1u)};
    if (cv == 1)
    {
        if (0 != value)
        {
            m_config.address = value;
            m_saveDataFkt();
            returnValue = value;
        }
    }
    else if (cv == m_resetCv)
    {
        if (0 != value)
        {
            m_config.address = 0x01;

            for (auto &functionConfig : m_config.functionConfig)
            {
                functionConfig.type = Type::Servo;
                functionConfig.rate = 8;
                functionConfig.startPosition = 28;
                functionConfig.endPosition = 140;
            }
            m_saveDataFkt();
            returnValue = value;
        }
    }
    else if ((cv >= m_baseCV) && (cv < (m_baseCV + m_functionConfigSize * N)))
    {
        uint16_t cvNumber{static_cast<uint16_t>(cv - m_baseCV)};
        uint16_t parameter{static_cast<uint16_t>(cvNumber % m_functionConfigSize)};
        uint16_t function{static_cast<uint16_t>(cvNumber / m_functionConfigSize)};
        switch (parameter)
        {
        case 0:
            if (value <= static_cast<uint8_t>(Type::Fade))
            {
                m_config.functionConfig[function].type = static_cast<Type>(value);
                m_saveDataFkt();
            }
            returnValue = value;
            break;
        case 1:
            m_config.functionConfig[function].rate = value;
            returnValue = value;
            m_saveDataFkt();
            break;
        case 2:
            m_config.functionConfig[function].startPosition = value;
            returnValue = value;
            m_saveDataFkt();
            break;
        case 3:
            m_config.functionConfig[function].endPosition = value;
            returnValue = value;
            m_saveDataFkt();
            break;
        default:
            break;
        }
    }
    return returnValue;
}