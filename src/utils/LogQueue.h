#pragma once

#include <Arduino.h>

#define LOG_MAX_BUFFER 250;

class LogQueue {
   public:
    LogQueue(){};

    ~LogQueue(){};

    void setup(void){};

    void loop(void){};

    void add_sTX(String tx, String retries, String reTransmit, String payload, String data) {
        DBGPRINTLN(F(""));
        DBGPRINT(F("sTX("));
        DBGPRINT(String(tx));
        DBGPRINT(F(") Re("));
        DBGPRINT(String(retries));
        DBGPRINT(F(") RT("));
        DBGPRINT(String(reTransmit));
        DBGPRINT(F(") P("));
        DBGPRINT(String(payload));
        DBGPRINT(F(") D("));
        DBGPRINT(String(data));
        DBGPRINT(F(") "));
    };

    void add_rTX(String tx, String retries, String reTransmit, String payload, String data) {
        DBGPRINTLN(F(""));
        DBGPRINT(F("rTX("));
        DBGPRINT(String(tx));
        DBGPRINT(F(") Re("));
        DBGPRINT(String(retries));
        DBGPRINT(F(") RT("));
        DBGPRINT(String(reTransmit));
        DBGPRINT(F(") P("));
        DBGPRINT(String(payload));
        DBGPRINT(F(") D("));
        DBGPRINT(String(data));
        DBGPRINT(F(") "));
    };

    void add_Timeout(String text){
        DBGPRINT(F("TO("));
        DBGPRINT(String(text));
        DBGPRINT(F("ms) "));
    };

    void add_Frame(){

    };

    void add_IRQ_ACK(String text) {
        DBGPRINT(F("ia("));
        DBGPRINT(String(text));
        DBGPRINT(F(") "));
    };

    void add_IRQ_NACK(String text) {
        DBGPRINT(F("in("));
        DBGPRINT(String(text));
        DBGPRINT(F(") "));
    };

    void add_IRQ_Data(String text) {
        DBGPRINT(F("id("));
        DBGPRINT(String(text));
        DBGPRINT(F(") "));
    };

    void print(){};

   private:
};
