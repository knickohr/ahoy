#pragma once

#include <Arduino.h>

#define LOG_MAX_BUFFER 250;

class LogQueue {
   public:
    LogQueue(){};

    ~LogQueue(){};

    void setup(uint32_t *timestamp, uint16_t *tsMillis) {
//        mTimestamp = timestamp;
    };

    void loop(void){};

    void add_sTX(String ivId = "", String txCh = "", String txFreq = "", String txHwRetries = "", String txSwRetransmit = "", String txPayload = "", String txData = "") {
        // Timestamp
        mMicrosStart = millis();
        // NewLine
        DBGPRINTLN(F(""));
        // Debug
        DPRINT_IVID(DBG_INFO, ivId);
        DBGPRINT(F("sTX("));
        DBGPRINT(F("t=0,"));
        if (txCh != "") {
            DBGPRINT(F("c="));
            DBGPRINT(String(txCh));
            DBGPRINT(F(","));
        }
        if (txFreq != "") {
            DBGPRINT(F("f="));
            DBGPRINT(String(txFreq));
            DBGPRINT(F(","));
        }
        DBGPRINT(F("Re="));
        DBGPRINT(String(txHwRetries));
        DBGPRINT(F(",RT="));
        DBGPRINT(String(txSwRetransmit));
        DBGPRINT(F(",P="));
        DBGPRINT(String(txPayload));
        DBGPRINT(F(",D="));
        DBGPRINT(String(txData));
        DBGPRINT(F(") "));
    };

    void add_rTX(String ivId = "", String txCh = "", String txFreq = "", String txHwRetries = "", String txSwRetransmit = "", String txPayload = "", String txData = "") {
        // Timestamp
        unsigned long t = millis() - mMicrosStart;
        // NewLine
        DBGPRINTLN(F(""));
        // Debug
        DPRINT_IVID(DBG_INFO, ivId);
        DBGPRINT(F("rTX("));
        DBGPRINT(F("t="));
        DBGPRINT(String(t));
        DBGPRINT(F(","));
        if (txCh != "") {
            DBGPRINT(F("c="));
            DBGPRINT(String(txCh));
            DBGPRINT(F(","));
        }
        if (txFreq != "") {
            DBGPRINT(F("f="));
            DBGPRINT(String(txFreq));
            DBGPRINT(F(","));
        }
        DBGPRINT(F("Re="));
        DBGPRINT(String(txHwRetries));
        DBGPRINT(F(",RT="));
        DBGPRINT(String(txSwRetransmit));
        DBGPRINT(F(",P="));
        DBGPRINT(String(txPayload));
        DBGPRINT(F(",D="));
        DBGPRINT(String(txData));
        DBGPRINT(F(") "));
    };

    void add_Timeout(String ivId = "", String txTimeout = "") {
        // Timestamp
        unsigned long t = millis() - mMicrosStart;
        // NewLine
        DBGPRINTLN(F(""));
        // Debug
        DPRINT_IVID(DBG_INFO, ivId);
        DBGPRINT(F("TO("));
        DBGPRINT(F("t="));
        DBGPRINT(String(t));
        DBGPRINT(F(","));
        DBGPRINT(F("TO="));
        DBGPRINT(String(txTimeout));
        DBGPRINT(F(") "));
    };

    void add_RX(String ivId = "", String rxCh = "", String rxFreq = "", String rxTime = "", String rxPayload = "", String rxData = "") {
        // Timestamp
        unsigned long t = millis() - mMicrosStart;
        // NewLine
        DBGPRINTLN(F(""));
        // Debug
        DPRINT_IVID(DBG_INFO, ivId);
        DBGPRINT(F("RX("));
        DBGPRINT(F("t="));
        DBGPRINT(String(t));
        DBGPRINT(F(","));
        if (rxCh != "") {
            DBGPRINT(F("c="));
            DBGPRINT(String(rxCh));
            DBGPRINT(F(","));
        }
        if (rxFreq != "") {
            DBGPRINT(F("f="));
            DBGPRINT(String(rxFreq));
            DBGPRINT(F(","));
        }
        DBGPRINT(F(",T="));
        DBGPRINT(String(rxTime));
        DBGPRINT(F(",P="));
        DBGPRINT(String(rxPayload));
        DBGPRINT(F(",D="));
        DBGPRINT(String(rxData));
        DBGPRINT(F(") "));


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
            if(*mPrintWholeTrace) {
                DBGPRINT(F("| "));
                if(*mPrivacyMode)
                    ah::dumpBuf(p->packet, p->len, 1, 8);
                else
                    ah::dumpBuf(p->packet, p->len);
            } else {
                DBGPRINT(F("| "));
                DHEX(p->packet[0]);
                DBGPRINT(F(" "));
                DBGHEXLN(p->packet[9]);
            }
*/


    }

    void add_missingFrame(String ivId, String nr, String attemp, String attemptsMax) {
        // Timestamp
        unsigned long t = millis() - mMicrosStart;
        // NewLine
        DBGPRINTLN(F(""));
        // Debug
        DPRINT_IVID(DBG_INFO, ivId);
        DBGPRINT(F("Fm("));
        DBGPRINT(String(nr));
        DBGPRINT(F(" RT("));
        DBGPRINT(String(attemp));
        DBGPRINT(F(","));
        DBGPRINT(String(attemptsMax));
        DBGPRINT(F("))"));
    }

    void add_missingFrames(String ivId, String text) {
        // Timestamp
        unsigned long t = millis() - mMicrosStart;
        // NewLine
        DBGPRINTLN(F(""));
        // Debug
        DPRINT_IVID(DBG_INFO, ivId);
        DBGPRINT(F("mF(t="));
        DBGPRINT(String(t));
        DBGPRINT(F(",c="));
        DBGPRINT(String(text));
        DBGPRINT(F(") "));
    }

    void add_Reset(String ivId) {
        // Timestamp
        unsigned long t = millis() - mMicrosStart;
        // NewLine
        DBGPRINTLN(F(""));
        // Debug
        DPRINT_IVID(DBG_INFO, ivId);
        DBGPRINT(F("RST(t="));
        DBGPRINT(String(t));
        DBGPRINT(F(") "));
    }

    void add_Frame(){

    };

    void add_IRQ_ACK(String ivId = "", String index = "") {
        // Timestamp
        unsigned long t = millis() - mMicrosStart;
        // NewLine
        DBGPRINTLN(F(""));
        // Debug
        DPRINT_IVID(DBG_INFO, ivId);
        DBGPRINT(F("ia(t="));
        DBGPRINT(String(t));
        DBGPRINT(F(",i="));
        DBGPRINT(String(index));
        DBGPRINT(F(") "));
    };

    void add_IRQ_NACK(String ivId = "", String index = "") {
        // Timestamp
        unsigned long t = millis() - mMicrosStart;
        // NewLine
        DBGPRINTLN(F(""));
        // Debug
        DPRINT_IVID(DBG_INFO, ivId);
        DBGPRINT(F("in(t="));
        DBGPRINT(String(t));
        DBGPRINT(F(",i="));
        DBGPRINT(String(index));
        DBGPRINT(F(") "));
    };

    void add_IRQ_Data(String ivId = "", String index = "") {
        // Timestamp
        unsigned long t = millis() - mMicrosStart;
        // NewLine
        DBGPRINTLN(F(""));
        // Debug
        DPRINT_IVID(DBG_INFO, ivId);
        DBGPRINT(F("id(t="));
//        DBGPRINT(String(index));
//        DBGPRINT(F(",t="));
        DBGPRINT(String(t));
        DBGPRINT(F(") "));
    };

    void print(){};

   private:
//    uint32_t *mTimestamp = nullptr;
//    uint32_t mTimestampStart = 0;
//    uint32_t mMillis = 0;
    unsigned long mMicrosStart = 0;
};
