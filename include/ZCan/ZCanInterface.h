/*********************************************************************
 * ZCan Interface Esp32
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

#include <Arduino.h>
#include <Printable.h>
#include <String>
#include <array>

class ZCanMessage : public Printable
{

public:
    struct
    {
        // do not change structure
        uint32_t group : 4;
        uint32_t command : 6;
        uint32_t mode : 2;
        uint32_t networkId : 16; // id of message sender
    };

    /**
     * The number of data bytes in the payload.
     */
    uint8_t length;

    /**
     * The actual message data bytes.
     */
    std::array<uint8_t, 8> data;

    /**
     * Clears the message, setting all values to zero. Provides for
     * easy recycling of TrackMessage objects.
     */
    void clear();

    /**
     * Prints the message to the given Print object, which could be a
     * Serial object, for instance. The message format looks like this
     *
     * HHHH R CC L DD DD DD DD DD DD DD DD
     *
     * with all numbers being hexadecimals and the data bytes being
     * optional beyond what the message length specifies. Exactly one
     * whitespace is inserted between different fields as a separator.
     */
    virtual size_t printTo(Print &p) const;

    /**
     * Parses the message from the given String. Returns true on
     * success, false otherwise. The message must have exactly the
     * format that printTo creates. This includes each and every
     * whitespace. If the parsing fails the state of the object is
     * undefined afterwards, and a clear() is recommended.
     */
    bool parseFrom(String &s);
};

class ZCanInterface
{
public:
    enum class deviceType : uint16_t
    {
        MX10 = 0x1000,
        MX10Max = 0x1FFF,
        Z21 = 0x0200,
        Z21Max = 0x02FF,
        Roco10806 = 0x0205, // Booster
        Roco10807 = 0x0206, // Booster
        StEin = 0x9000,
        StEinMax = 0x90FF,
        Roco10808 = 0x9200, // Besetztmelder 10808
        Roco10808Max = 0x92FF
    };

    enum class Group : uint8_t
    {
        System = 0x00,      // systemkritische Aufgaben (Ein/Aus, Notstopp, ...)
        Accessory = 0x01,   // Befehle zum Steuern des Zubehörs. Damit sind sowohl Encoder/Rückmelder wie auch Decoder gemeint.
        Vehicle = 0x02,     // Befehle zum Steuern der Fahrzeuge (Mobile Decoder)
        Free = 0x03,        // Derzeit noch unbenutzt
        RCS = 0x04,         // RESERVIERT
        Config = 0x05,      // Konfiguration von Geräten, ZIMO Command Language
        TrackCfg = 0x06,    // RESERVIERT
        Data = 0x07,        // Object-Daten Transfer
        Info = 0x08,        // Statusmeldungen, meist ungefragte Meldungen
        AdditonalHw = 0x09, // Darf von Fremdsystemen je nach Bedarf verwendet werden.
        Network = 0x0A      // Network Management, Modulanmeldung, ...
    };

    enum class AccessoryCmd : uint8_t
    {
        Status = 0x00, // Jedes Steuersystem (Fahrpult, PC-Software) sollte den Zubehörstatus immer als erste Initialabfrage ausführen. Insbesondere für die MX8 und MX9 Module.
        Mode = 0x01,   // Dieses Datagramm dient der Abfrage und der Einstellung der Zubehörbetriebsart.
        Gpio = 0x02,   // Diese Datagramme dienen der effizienten Statusabfrage von simplen Ein-/Ausgängen. Es werden je Gruppe bis zu 32 Ein-/Ausgangszustände übertragen.
        Port = 0x04,   // Wenn M = 0b00, DLC = 3, dann wird der Zustand des Ein/Ausganges (Port) vom Zubehör NID abgefragt.Durch M = 0b01, DLC = 4 wird der Ausgang des Zubehörs (NID) auf den angegeben Wert eingestellt.
        Data = 0x05    // Mit diesen Datagrammen können Objektdaten abgefragt und gesetzt werden.
    };

    enum class InfoCmd : uint8_t
    {
        ModulPowerInfo = 0x00,
        ModulInfo = 0x08
    };

    enum class NetworkCmd : uint8_t
    {
        Ping = 0x00,
        PortOpen = 0x06
    };

    enum class Mode : uint8_t
    {
        Req = 0b00,
        Cmd = 0b01,
        Evt = 0b10,
        ACK = 0b11
    };

    enum class AccessoryErrorCode : uint8_t
    {
        NoError = 0x0000, // Kein Fehler
        NoData = 0x0001,
        NoTrackVoltage = 0x0002,     // Keine Gleisspannung
        NoAccessoryVoltage = 0x0003, // Keine Zubehör Versorgung
        NoDccSignal = 0x0004,        // Kein DCC Signal
        NoCanVoltage = 0x0005,       // Keine CAN Spannung
        NoHighVoltage = 0x0006,      // Keine +20V
        NoLowVoltage = 0x0007        // Keine +5V
    };

protected:
    ZCanInterface(bool debug);

    virtual ~ZCanInterface();

    bool m_debug;

    virtual void begin();

    virtual bool sendMessage(ZCanMessage &message) = 0;

    virtual bool receiveMessage(ZCanMessage &message) = 0;

    virtual void end() = 0;

    void handleReceivedMessage(ZCanMessage &message);

    bool handleAccessoryMessage(ZCanMessage &message);

    bool handleInfoMessage(ZCanMessage &message);

    bool handleNetworkMessage(ZCanMessage &message);

    virtual bool onAccessoryStatus(uint16_t accessoryId) { return false; };

    virtual bool onAccessoryMode(uint16_t accessoryId) { return false; };

    virtual bool onAccessoryGpio(uint16_t accessoryId, uint16_t type) { return false; };

    virtual bool onAccessoryPort4(uint16_t accessoryId, uint8_t port) { return false; };

    virtual bool onAccessoryPort4(uint16_t accessoryId, uint8_t port, uint8_t value) { return false; };

    virtual bool onAccessoryData(uint16_t accessoryId, uint8_t port, uint8_t type) { return false; };

    virtual bool onAccessorySetData(uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value) { return false; };

    virtual bool onRequestModulPowerInfo(uint16_t id, uint8_t port) { return false; };

    virtual bool onRequestModulInfo(uint16_t id, uint16_t type) { return false; };

    virtual bool onCmdModulInfo(uint16_t id, uint16_t type, uint32_t info) { return false; };

    virtual bool onRequestPing(uint16_t id) { return false; };

    virtual bool onPing(uint16_t nid, uint32_t masterUid, uint16_t type, uint16_t sessionId) { return false; };

    bool sendAccessoryStatus(uint16_t accessoryId, uint16_t state, uint16_t ctrlNID, uint16_t lastControlCmdINms);

    bool sendAccessoryMode(uint16_t accessoryId, uint16_t mode);

    bool sendAccessoryGpio(uint16_t accessoryId, uint16_t type, uint32_t state);

    bool sendAccessoryPort4Evt(uint16_t accessoryId, uint8_t port, uint8_t value);
    bool sendAccessoryPort4Ack(uint16_t accessoryId, uint8_t port, bool valid, uint8_t value);

    bool sendAccessoryDataEvt(uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value);
    bool sendAccessoryDataAck(uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value);

    bool requestModulePowerInfo(uint16_t sendId, uint16_t requestId);
    bool requestModuleInfo(uint16_t sendId, uint16_t nid, uint16_t type);
    bool getModuleInfo(uint16_t sendId, uint16_t nid, uint16_t type, uint32_t info);
    bool sendModuleInfoEvt(uint16_t sendId, uint16_t type, uint32_t info);

    bool requestPing(uint16_t sendId, uint16_t requestId);
    bool sendPing(uint16_t sendId, uint32_t masterUid, uint16_t type, uint16_t sessionId);

    bool requestPortOpen(uint16_t id);

private:
    void messageAccessoryStatus(ZCanMessage &message, uint16_t accessoryId, uint16_t state, uint16_t CtrlNID, uint16_t lastControlCmdINms);

    void messageAccessoryMode(ZCanMessage &message, uint16_t accessoryId, uint16_t mode);

    void messageAccessoryGpio(ZCanMessage &message, uint16_t accessoryId, uint16_t type, uint32_t state);

    void messageAccessoryPort4Evt(ZCanMessage &message, uint16_t accessoryId, uint8_t port, uint8_t value);
    void messageAccessoryPort4Ack(ZCanMessage &message, uint16_t accessoryId, uint8_t port, bool valid, uint8_t value);

    void messageAccessoryDataEvt(ZCanMessage &message, uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value);
    void messageAccessoryDataAck(ZCanMessage &message, uint16_t accessoryId, uint8_t port, uint8_t type, uint32_t value);

    void messageRequestModulePowerInfo(ZCanMessage &message, uint16_t sendId, uint16_t requestId);
    void messageRequestModuleInfo(ZCanMessage &message, uint16_t sendId, uint16_t nid, uint16_t type);
    void messageCmdModuleInfo(ZCanMessage &message, uint16_t sendId, uint16_t nid, uint16_t type, uint32_t info);
    void messageModuleInfoEvt(ZCanMessage &message, uint16_t sendId, uint16_t type, uint32_t info);

    void messageRequestPing(ZCanMessage &message, uint16_t sendId, uint16_t requestId);
    void messagePing(ZCanMessage &message, uint16_t sendId, uint32_t masterUid, uint16_t type, uint16_t sessionId);

    void messageRequestPortOpen(ZCanMessage &message, uint16_t id);
};