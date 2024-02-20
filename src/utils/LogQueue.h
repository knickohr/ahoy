#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <array>

#include "config/config.h"
#include "defines.h"

/**
 * TX   Senden
 * RX   Empfangen
 * Fm   Frame missing
 * mF   Frames missing
 * RST  Reset
 * IA   Interrupt ACK
 * IN   Interrupt NACK
 * ID   Interrupt Data
 * TO   Timeout
 *
 * a =                          A =
 * b =                          B =
 * c = nRF Channel              C =
 * d =                          D = Data
 * e =                          E =
 * f = cmt Frequency            F =
 * g =                          G =
 * h = HW Retries               H = HW Retries Max
 * i = id                       I = IV Id
 * j =                          J =
 * k =                          K =
 * l =                          L =
 * m =                          M =
 * n =                          N =
 * o =                          O =
 * p =                          P = Payload
 * q =                          Q =
 * r = rssi                     R =
 * s = SW Retransmits           S = SW Retransmits Max
 * t = Time                     T = Timeout
 * u =                          U =
 * v =                          V =
 * w =                          W =
 * x =                          X =
 * y =                          Y =
 * z = Timestamp                Z =
 */

class LogQueue {
   public:
    LogQueue(){};

    ~LogQueue(){};

    void setup(bool *serialDebug, bool *privacyMode, bool *printWholeTrace) {
        _PrivacyMode = privacyMode;
        _SerialDebug = serialDebug;
        _PrintWholeTrace = printWholeTrace;
    };

    void loop(void) {}

    void setValid(void) {
        _isValid = true;
    }

    bool getValid(void) {
        return _isValid;
    }

    void add_TX(uint8_t ivId, uint8_t channel, uint8_t frequency, uint8_t hwRetries, uint8_t swRetransmit, uint8_t payload, std::array<uint8_t, MAX_RF_PAYLOAD_SIZE> txBuff, uint8_t framesExpected) {
        // Timestamp
        mMillisStart = millis();
        unsigned long t = millis() - mMillisStart;
        // JSON
        JsonObject TX = _jsonLog["TX"].to<JsonObject>();
        TX["I"] = ivId;
        TX["t"] = t;
        if (channel != 0)
            TX["c"] = channel;
        if (frequency != 0)
            TX["f"] = frequency;
        TX["h"] = hwRetries;
        TX["s"] = _TxAttemp;
        TX["P"] = payload;
        JsonArray data = TX["D"].to<JsonArray>();
        if (*_PrintWholeTrace) {
            for (uint8_t i = 0; i < payload; i++) {
                if (*_PrivacyMode) {
                    if ((i <= 1) && (i >= 8)) {
                        data.add("*");
                    } else {
                        data.add(dec2hex(txBuff[i]));
                    }
                } else {
                    data.add(dec2hex(txBuff[i]));
                }
            }
        } else {
            data.add(dec2hex(txBuff[0]));
            data.add(dec2hex(txBuff[10]));
            data.add(dec2hex(txBuff[9]));
        }
        TX["d"] = framesExpected;
    };

    void set_TxStart(uint32_t tspMillis = millis()) {
        mMillisStart = tspMillis;
    }

    void add_TxAttemp(void) {
        _TxAttemp++;
    }

    void add_Timeout(String ivId = "", String timeout = "") {
        // Timestamp
        unsigned long t = millis() - mMillisStart;
        // JSON
        JsonObject TO = _jsonLog["TO"].to<JsonObject>();
        TO["I"] = ivId;
        TO["t"] = t;
        TO["T"] = timeout;
    };

    void add_RX(uint8_t ivId = 0, uint8_t channel = 0, float frequency = 0, String rxTime = "", packet_t *p = NULL) {
        // Timestamp
        unsigned long t = millis() - mMillisStart;
        // JSON
        JsonObject RX = _jsonLog["RX"].to<JsonObject>();
        RX["I"] = ivId;
        RX["t"] = t;
        if (channel != 0)
            RX["c"] = channel;
        if (frequency != 0)
            RX["f"] = frequency;
        //        RX["T"] = rxTime;
        /*
                    DBGPRINT(String(p->len));
                    if(INV_RADIO_TYPE_NRF == q->iv->ivRadioType) {
                        DBGPRINT(F(" CH"));
                        if(3 == p->ch)
                            DBGPRINT(F("0"));
                        DBGPRINT(String(p->ch));
                        DBGPRINT(F(" "));
                    } else {
                        DBGPRINT(F(" "));
                        DBGPRINT(String(p->rssi));
                        DBGPRINT(F("dBm "));
                    }
        */
        // Data
        JsonArray data = RX["D"].to<JsonArray>();
        if (*_PrintWholeTrace) {
            for (uint8_t i = 0; i < p->len; i++) {
                data.add(dec2hex(p->packet[i]));
                if (*_PrivacyMode) {
                    if ((i <= 1) && (i >= 8)) {
                        data.add("*");
                    } else {
                        data.add(dec2hex(p->packet[i]));
                    }
                } else {
                    data.add(dec2hex(p->packet[i]));
                }
            }
        } else {
            data.add(dec2hex(p->packet[0]));
            data.add(dec2hex(p->packet[9]));
        }
    }

    void add_missingFrame(uint8_t ivId, uint8_t nr, uint8_t attemp, uint8_t attemptsMax) {
        // Timestamp
        unsigned long t = millis() - mMillisStart;
        // JSON
        JsonObject Fm = _jsonLog["Fm"].to<JsonObject>();
        Fm["I"] = ivId;
        Fm["t"] = t;
        Fm["i"] = nr;
        Fm["s"] = (attemptsMax - attemp);
        Fm["S"] = attemptsMax;
    }

    void add_missingFrames(uint8_t ivId, uint8_t text) {
        // Timestamp
        unsigned long t = millis() - mMillisStart;
        // JSON
        JsonObject mF = _jsonLog["mF"].to<JsonObject>();
        mF["I"] = ivId;
        mF["t"] = t;
        mF["c"] = text;
    }

    void add_Reset(uint8_t ivId) {
        // Timestamp
        unsigned long t = millis() - mMillisStart;
        // JSON
        JsonObject RST = _jsonLog["RST"].to<JsonObject>();
        RST["I"] = ivId;
        RST["t"] = t;
    }

    void add_IRQ_ACK(uint8_t ivId, uint8_t index) {
        // Timestamp
        unsigned long t = millis() - mMillisStart;
        // JSON
        JsonObject IrqACK = _jsonLog["IA"].to<JsonObject>();
        IrqACK["I"] = ivId;
        IrqACK["t"] = t;
        IrqACK["i"] = index;
    };

    void add_IRQ_NACK(uint8_t ivId, uint8_t index) {
        // Timestamp
        unsigned long t = millis() - mMillisStart;
        // JSON
        JsonObject IrqNACK = _jsonLog["IN"].to<JsonObject>();
        IrqNACK["I"] = ivId;
        IrqNACK["t"] = t;
        IrqNACK["i"] = index;
    };

    void add_IRQ_Data(uint8_t ivId, uint8_t rssi) {
        // Timestamp
        unsigned long t = millis() - mMillisStart;
        // JSON
        JsonObject IrqData = _jsonLog["ID"].to<JsonObject>();
        IrqData["I"] = ivId;
        IrqData["t"] = t;
        IrqData["r"] = rssi;
    };

    void print() {
        DBGPRINTLN(_jsonLog.as<String>());
        _jsonLog.clear();
    }

    String getLog(void) {
        String log = _jsonLog.as<String>();
        _jsonLog.clear();
        _isValid = false;
        _TxAttemp = 0;
        return log;
    }

   private:
    String dec2hex(uint8_t zahl) {
        String hex = String(zahl, HEX);
        if (hex.length() < 2)
            hex = String("0") + String(zahl, HEX);
        return hex;
    }

    bool *_PrivacyMode = nullptr;
    bool *_SerialDebug = nullptr;
    bool *_PrintWholeTrace = nullptr;
    bool _isValid = false;
    unsigned long mMillisStart = 0;
    StaticJsonDocument<5000> _jsonLog;
    uint8_t _TxAttemp = 0;
};
