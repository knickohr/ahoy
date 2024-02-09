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
        DBGPRINT(String("sTX(" + tx + ") Re(" + retries + ") RT(" + reTransmit + ") P(" + payload + ") D(" + data + ") "));
    };

    void add_rTX(String tx, String retries, String reTransmit, String payload, String data) {
        DBGPRINTLN(F(""));
        DBGPRINT(String("rTX(" + tx + ") Re(" + retries + ") RT(" + reTransmit + ") P(" + payload + ") D(" + data + ") "));
    };

    void add_Timeout(String text){
        DBGPRINT(String("TO(" + text + "ms) "));
    };

    void add_Frame(){

    };

    void add_IRQ_ACK(String text) {
        DBGPRINT(String("ia(" + text + ") "));
    };

    void add_IRQ_NACK(String text) {
        DBGPRINT(String("in(" + text + ") "));
    };

    void add_IRQ_Data(String text) {
        DBGPRINT(String("id(" + text + ") "));
    };

    void print(){};

   private:
};
