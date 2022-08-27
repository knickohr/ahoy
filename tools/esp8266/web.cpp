//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif

#include "web.h"

#include "html/h/index_html.h"
#include "html/h/style_css.h"
#include "html/h/favicon_ico_gz.h"
#include "html/h/setup_html.h"
#include "html/h/visualization_html.h"
#include "html/h/update_html.h"


const uint16_t pwrLimitOptionValues[] {
    NoPowerLimit,
    AbsolutNonPersistent,
    AbsolutPersistent,
    RelativNonPersistent,
    RelativPersistent
};

const char* const pwrLimitOptions[] {
    "no power limit",
    "absolute in Watt non persistent",
    "absolute in Watt persistent",
    "relativ in percent non persistent",
    "relativ in percent persistent"
};

//-----------------------------------------------------------------------------
web::web(app *main, sysConfig_t *sysCfg, config_t *config, char version[]) {
    mMain    = main;
    mSysCfg  = sysCfg;
    mConfig  = config;
    mVersion = version;
    mWeb     = new AsyncWebServer(80);
    //mEvts    = new AsyncEventSource("/events");

    mApi     = new api(mWeb, main, sysCfg, config, version);
}


//-----------------------------------------------------------------------------
void web::setup(void) {
    DPRINTLN(DBG_VERBOSE, F("app::setup-begin"));
    mWeb->begin();
    DPRINTLN(DBG_VERBOSE, F("app::setup-on"));
    mWeb->on("/",               HTTP_ANY,  std::bind(&web::showIndex,         this, std::placeholders::_1));
    mWeb->on("/style.css",      HTTP_ANY,  std::bind(&web::showCss,           this, std::placeholders::_1));
    mWeb->on("/favicon.ico",    HTTP_ANY,  std::bind(&web::showFavicon,       this, std::placeholders::_1));
    mWeb->onNotFound (                     std::bind(&web::showNotFound,      this, std::placeholders::_1));
    mWeb->on("/uptime",         HTTP_ANY,  std::bind(&web::showUptime,        this, std::placeholders::_1));
    mWeb->on("/reboot",         HTTP_ANY,  std::bind(&web::showReboot,        this, std::placeholders::_1));
    mWeb->on("/erase",          HTTP_ANY,  std::bind(&web::showErase,         this, std::placeholders::_1));
    mWeb->on("/factory",        HTTP_ANY,  std::bind(&web::showFactoryRst,    this, std::placeholders::_1));

    mWeb->on("/setup",          HTTP_ANY,  std::bind(&web::showSetup,         this, std::placeholders::_1));
    mWeb->on("/save",           HTTP_ANY,  std::bind(&web::showSave,          this, std::placeholders::_1));

    mWeb->on("/cmdstat",        HTTP_ANY,  std::bind(&web::showStatistics,    this, std::placeholders::_1));
    mWeb->on("/visualization",  HTTP_ANY,  std::bind(&web::showVisualization, this, std::placeholders::_1));
    mWeb->on("/livedata",       HTTP_ANY,  std::bind(&web::showLiveData,      this, std::placeholders::_1));
    mWeb->on("/json",           HTTP_ANY,  std::bind(&web::showJson,          this, std::placeholders::_1));
    mWeb->on("/api",            HTTP_POST, std::bind(&web::showWebApi,        this, std::placeholders::_1));


    mWeb->on("/update",         HTTP_GET,  std::bind(&web::showUpdateForm,    this, std::placeholders::_1));
    mWeb->on("/update",         HTTP_POST, std::bind(&web::showUpdate,        this, std::placeholders::_1),
                                           std::bind(&web::showUpdate2,       this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));

    //mEvts->onConnect(std::bind(&web::onConnect, this, std::placeholders::_1));
    //mWeb->addHandler(mEvts);

    mApi->setup();
}


//-----------------------------------------------------------------------------
void web::loop(void) {
    mApi->loop();
}


//-----------------------------------------------------------------------------
/*void web::onConnect(AsyncEventSourceClient *client) {
    DPRINTLN(DBG_INFO, "onConnect");

    if(client->lastId())
        DPRINTLN(DBG_INFO, "Client reconnected! Last message ID that it got is: " + String(client->lastId()));

    client->send("hello!", NULL, millis(), 1000);
}*/


//-----------------------------------------------------------------------------
void web::showIndex(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("showIndex"));
    String html = FPSTR(index_html);
    html.replace(F("{DEVICE}"), mSysCfg->deviceName);
    html.replace(F("{VERSION}"), mVersion);
    html.replace(F("{TS}"), String(mConfig->sendInterval) + " ");
    html.replace(F("{JS_TS}"), String(mConfig->sendInterval * 1000));
    html.replace(F("{BUILD}"), String(AUTO_GIT_HASH));
    request->send(200, "text/html", html);
}


//-----------------------------------------------------------------------------
void web::showCss(AsyncWebServerRequest *request) {
    request->send(200, "text/css", FPSTR(style_css));
}


//-----------------------------------------------------------------------------
void web::showFavicon(AsyncWebServerRequest *request) {
    static const char favicon_type[] PROGMEM = "image/x-icon";
    AsyncWebServerResponse *response = request->beginResponse_P(200, favicon_type, favicon_ico_gz, favicon_ico_gz_len);
    response->addHeader(F("Content-Encoding"), "gzip");
    request->send(response);
}


//-----------------------------------------------------------------------------
void web::showNotFound(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("showNotFound - ") + request->url());
    String msg = F("File Not Found\n\nURL: ");
    msg += request->url();
    msg += F("\nMethod: ");
    msg += ( request->method() == HTTP_GET ) ? "GET" : "POST";
    msg += F("\nArguments: ");
    msg += request->args();
    msg += "\n";

    for(uint8_t i = 0; i < request->args(); i++ ) {
        msg += " " + request->argName(i) + ": " + request->arg(i) + "\n";
    }

    request->send(404, F("text/plain"), msg);
}


//-----------------------------------------------------------------------------
void web::showUptime(AsyncWebServerRequest *request) {
    char time[21] = {0};
    uint32_t uptime = mMain->getUptime();

    uint32_t upTimeSc = uint32_t((uptime) % 60);
    uint32_t upTimeMn = uint32_t((uptime / (60)) % 60);
    uint32_t upTimeHr = uint32_t((uptime / (60 * 60)) % 24);
    uint32_t upTimeDy = uint32_t((uptime / (60 * 60 * 24)) % 365);

    snprintf(time, 20, "%d Days, %02d:%02d:%02d", upTimeDy, upTimeHr, upTimeMn, upTimeSc);

    request->send(200, "text/plain", String(time) + "; now: " + mMain->getDateTimeStr(mMain->getTimestamp()));
}


//-----------------------------------------------------------------------------
void web::showReboot(AsyncWebServerRequest *request) {
    request->send(200, F("text/html"), F("<!doctype html><html><head><title>Rebooting ...</title><meta http-equiv=\"refresh\" content=\"10; URL=/\"></head><body>rebooting ... auto reload after 10s</body></html>"));
    delay(1000);
    ESP.restart();
}


//-----------------------------------------------------------------------------
void web::showErase(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("showErase"));
    mMain->eraseSettings();
    showReboot(request);
}


//-----------------------------------------------------------------------------
void web::showFactoryRst(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("showFactoryRst"));
    String content = "";
    int refresh = 3;
    if(request->args() > 0) {
        if(request->arg("reset").toInt() == 1) {
            mMain->eraseSettings(true);
            content = F("factory reset: success\n\nrebooting ... ");
            refresh = 10;
        }
        else {
            content = F("factory reset: aborted");
            refresh = 3;
        }
    }
    else {
        content = F("<h1>Factory Reset</h1>"
            "<p><a href=\"/factory?reset=1\">RESET</a><br/><br/><a href=\"/factory?reset=0\">CANCEL</a><br/></p>");
        refresh = 120;
    }
    request->send(200, F("text/html"), F("<!doctype html><html><head><title>Factory Reset</title><meta http-equiv=\"refresh\" content=\"") + String(refresh) + F("; URL=/\"></head><body>") + content + F("</body></html>"));
    if(refresh == 10) {
        delay(1000);
        ESP.restart();
    }
}


//-----------------------------------------------------------------------------
void web::showSetup(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("app::showSetup"));

    //tmplProc *proc = new tmplProc(request, 11000);
    //proc->process(setup_html, setup_html_len, std::bind(&web::showSetupCb, this, std::placeholders::_1));
    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), setup_html, setup_html_len);
    response->addHeader(F("Content-Encoding"), "gzip");
    request->send(response);
}


//-----------------------------------------------------------------------------
void web::showSave(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("showSave"));

    if(request->args() > 0) {
        char buf[20] = {0};

        // general
        if(request->arg("ssid") != "")
            request->arg("ssid").toCharArray(mSysCfg->stationSsid, SSID_LEN);
        if(request->arg("pwd") != "{PWD}")
            request->arg("pwd").toCharArray(mSysCfg->stationPwd, PWD_LEN);
        if(request->arg("device") != "")
            request->arg("device").toCharArray(mSysCfg->deviceName, DEVNAME_LEN);

        // inverter
        Inverter<> *iv;
        for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
            iv = mMain->mSys->getInverterByPos(i, false);
            // address
            request->arg("inv" + String(i) + "Addr").toCharArray(buf, 20);
            if(strlen(buf) == 0)
                memset(buf, 0, 20);
            iv->serial.u64 = mMain->Serial2u64(buf);

            // active power limit
            uint16_t actPwrLimit = request->arg("inv" + String(i) + "ActivePowerLimit").toInt();
            uint16_t actPwrLimitControl = request->arg("inv" + String(i) + "PowerLimitControl").toInt();
            if (actPwrLimit != 0xffff && actPwrLimit > 0){
                iv->powerLimit[0] = actPwrLimit;
                iv->powerLimit[1] = actPwrLimitControl;
                iv->devControlCmd = ActivePowerContr;
                iv->devControlRequest = true;
                if ((iv->powerLimit[1] & 0x0001) == 0x0001)
                    DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("%") );    
                else {
                    DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("W") );
                    DPRINTLN(DBG_INFO, F("Power Limit Control Setting ") + String(iv->powerLimit[1]));
            }
            }
            if (actPwrLimit == 0xffff) { // set to 100%
                iv->powerLimit[0] = 100;
                iv->powerLimit[1] = RelativPersistent;
                iv->devControlCmd = ActivePowerContr;
                iv->devControlRequest = true;
                DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to unlimted"));
            }
                
            // name
            request->arg("inv" + String(i) + "Name").toCharArray(iv->name, MAX_NAME_LENGTH);

            // max channel power / name
            for(uint8_t j = 0; j < 4; j++) {
                iv->chMaxPwr[j] = request->arg("inv" + String(i) + "ModPwr" + String(j)).toInt() & 0xffff;
                request->arg("inv" + String(i) + "ModName" + String(j)).toCharArray(iv->chName[j], MAX_NAME_LENGTH);
            }
            iv->initialized = true;
        }
        if(request->arg("invInterval") != "")
            mConfig->sendInterval = request->arg("invInterval").toInt();
        if(request->arg("invRetry") != "")
            mConfig->maxRetransPerPyld = request->arg("invRetry").toInt();

        // pinout
        uint8_t pin;
        for(uint8_t i = 0; i < 3; i ++) {
            pin = request->arg(String(pinArgNames[i])).toInt();
            switch(i) {
                default: mConfig->pinCs  = pin; break;
                case 1:  mConfig->pinCe  = pin; break;
                case 2:  mConfig->pinIrq = pin; break;
            }
        }

        // nrf24 amplifier power
        mConfig->amplifierPower = request->arg("rf24Power").toInt() & 0x03;

        // ntp
        if(request->arg("ntpAddr") != "") {
            request->arg("ntpAddr").toCharArray(mConfig->ntpAddr, NTP_ADDR_LEN);
            mConfig->ntpPort = request->arg("ntpPort").toInt() & 0xffff;
        }

        // mqtt
        if(request->arg("mqttAddr") != "") {
            request->arg("mqttAddr").toCharArray(mConfig->mqtt.broker, MQTT_ADDR_LEN);
            request->arg("mqttUser").toCharArray(mConfig->mqtt.user, MQTT_USER_LEN);
            request->arg("mqttPwd").toCharArray(mConfig->mqtt.pwd, MQTT_PWD_LEN);
            request->arg("mqttTopic").toCharArray(mConfig->mqtt.topic, MQTT_TOPIC_LEN);
            mConfig->mqtt.port = request->arg("mqttPort").toInt();
        }

        // serial console
        if(request->arg("serIntvl") != "") {
            mConfig->serialInterval = request->arg("serIntvl").toInt() & 0xffff;

            mConfig->serialDebug  = (request->arg("serDbg") == "on");
            mConfig->serialShowIv = (request->arg("serEn") == "on");
            // Needed to log TX buffers to serial console
            mMain->mSys->Radio.mSerialDebug = mConfig->serialDebug;
        }

        mMain->saveValues();

        if(request->arg("reboot") == "on")
            showReboot(request);
        else
            request->send(200, F("text/html"), F("<!doctype html><html><head><title>Setup saved</title><meta http-equiv=\"refresh\" content=\"0; URL=/setup\"></head><body>"
                "<p>saved</p></body></html>"));
    }
}


//-----------------------------------------------------------------------------
void web::showStatistics(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("web::showStatistics"));
    request->send(200, F("text/plain"), mMain->getStatistics());
}


//-----------------------------------------------------------------------------
void web::showVisualization(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("web::showVisualization"));
    String html = FPSTR(visualization_html);
    html.replace(F("{DEVICE}"), mSysCfg->deviceName);
    html.replace(F("{VERSION}"), mVersion);
    html.replace(F("{TS}"), String(mConfig->sendInterval) + " ");
    html.replace(F("{JS_TS}"), String(mConfig->sendInterval * 1000));
    request->send(200, F("text/html"), html);
}


//-----------------------------------------------------------------------------
void web::showLiveData(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("web::showLiveData"));

    String modHtml;
    for (uint8_t id = 0; id < mMain->mSys->getNumInverters(); id++) {
        Inverter<> *iv = mMain->mSys->getInverterByPos(id);
        if (NULL != iv) {
#ifdef LIVEDATA_VISUALIZED
            uint8_t modNum, pos;
            switch (iv->type) {
                default:
                case INV_TYPE_1CH: modNum = 1; break;
                case INV_TYPE_2CH: modNum = 2; break;
                case INV_TYPE_4CH: modNum = 4; break;
            }

            modHtml += F("<div class=\"iv\">"
                         "<div class=\"ch-iv\"><span class=\"head\">")
                    + String(iv->name) + F(" Limit ")
                    + String(iv->actPowerLimit) + F("%");
            if(NoPowerLimit == iv->powerLimit[1])
                modHtml += F(" (not controlled)");
            modHtml += F(" | last Alarm: ") + iv->lastAlarmMsg + F("</span>");

            uint8_t list[] = {FLD_UAC, FLD_IAC, FLD_PAC, FLD_F, FLD_PCT, FLD_T, FLD_YT, FLD_YD, FLD_PDC, FLD_EFF, FLD_PRA, FLD_ALARM_MES_ID};

            for (uint8_t fld = 0; fld < 11; fld++) {
                pos = (iv->getPosByChFld(CH0, list[fld]));
                if (0xff != pos) {
                    modHtml += F("<div class=\"subgrp\">");
                    modHtml += F("<span class=\"value\">") + String(iv->getValue(pos));
                    modHtml += F("<span class=\"unit\">") + String(iv->getUnit(pos)) + F("</span></span>");
                    modHtml += F("<span class=\"info\">") + String(iv->getFieldName(pos)) + F("</span>");
                    modHtml += F("</div>");
                }
            }
            modHtml += "</div>";

            for (uint8_t ch = 1; ch <= modNum; ch++) {
                modHtml += F("<div class=\"ch\"><span class=\"head\">");
                if (iv->chName[ch - 1][0] == 0)
                    modHtml += F("CHANNEL ") + String(ch);
                else
                    modHtml += String(iv->chName[ch - 1]);
                modHtml += F("</span>");
                for (uint8_t j = 0; j < 6; j++) {
                    switch (j) {
                        default: pos = (iv->getPosByChFld(ch, FLD_UDC)); break;
                        case 1:  pos = (iv->getPosByChFld(ch, FLD_IDC)); break;
                        case 2:  pos = (iv->getPosByChFld(ch, FLD_PDC)); break;
                        case 3:  pos = (iv->getPosByChFld(ch, FLD_YD));  break;
                        case 4:  pos = (iv->getPosByChFld(ch, FLD_YT));  break;
                        case 5:  pos = (iv->getPosByChFld(ch, FLD_IRR)); break;
                    }
                    if (0xff != pos) {
                        modHtml += F("<span class=\"value\">") + String(iv->getValue(pos));
                        modHtml += F("<span class=\"unit\">") + String(iv->getUnit(pos)) + F("</span></span>");
                        modHtml += F("<span class=\"info\">") + String(iv->getFieldName(pos)) + F("</span>");
                    }
                }
                modHtml += "</div>";
            }
            modHtml += F("<div class=\"ts\">Last received data requested at: ") + mMain->getDateTimeStr(iv->ts) + F("</div>");
            modHtml += F("</div>");
#else
            // dump all data to web frontend
            modHtml = F("<pre>");
            char topic[30], val[10];
            for (uint8_t i = 0; i < iv->listLen; i++) {
                snprintf(topic, 30, "%s/ch%d/%s", iv->name, iv->assign[i].ch, iv->getFieldName(i));
                snprintf(val, 10, "%.3f %s", iv->getValue(i), iv->getUnit(i));
                modHtml += String(topic) + ": " + String(val) + "\n";
            }
            modHtml += F("</pre>");
#endif
        }
    }

    request->send(200, F("text/html"), modHtml);
}


//-----------------------------------------------------------------------------
void web::showJson(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("web::showJson"));
    request->send(200, F("application/json"), mMain->getJson());
}


//-----------------------------------------------------------------------------
void web::showWebApi(AsyncWebServerRequest *request)
{
    DPRINTLN(DBG_VERBOSE, F("web::showWebApi"));
    DPRINTLN(DBG_DEBUG, request->arg("plain"));
    const size_t capacity = 200; // Use arduinojson.org/assistant to compute the capacity.
    DynamicJsonDocument response(capacity);

    // Parse JSON object
    deserializeJson(response, request->arg("plain"));
    // ToDo: error handling for payload
    uint8_t iv_id = response["inverter"];
    uint8_t cmd = response["cmd"];
    Inverter<> *iv = mMain->mSys->getInverterByPos(iv_id);
    if (NULL != iv)
    {
        if (response["tx_request"] == (uint8_t)TX_REQ_INFO)
        {
            // if the AlarmData is requested set the Alarm Index to the requested one
            if (cmd == AlarmData || cmd == AlarmUpdate){
                // set the AlarmMesIndex for the request from user input
                iv->alarmMesIndex = response["payload"]; 
            }
            DPRINTLN(DBG_INFO, F("Will make tx-request 0x15 with subcmd ") + String(cmd) + F(" and payload ") + String((uint16_t) response["payload"]));
            // process payload from web request corresponding to the cmd
            iv->enqueCommand<InfoCommand>(cmd);
        }
        

        if (response["tx_request"] == (uint8_t)TX_REQ_DEVCONTROL)
        {
            if (response["cmd"] == (uint8_t)ActivePowerContr)
            {
                uint16_t webapiPayload = response["payload"];
                uint16_t webapiPayload2 = response["payload2"];
                if (webapiPayload > 0 && webapiPayload < 10000)
                {
                    iv->devControlCmd = ActivePowerContr;
                    iv->powerLimit[0] = webapiPayload;
                    if (webapiPayload2 > 0)
                    {
                        iv->powerLimit[1] = webapiPayload2; // dev option, no sanity check
                    }
                    else
                    {                                             // if not set, set it to 0x0000 default
                        iv->powerLimit[1] = AbsolutNonPersistent; // payload will be seted temporay in Watt absolut
                    }
                    if (iv->powerLimit[1] & 0x0001)
                    {
                        DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("% via REST API"));
                    }
                    else
                    {
                        DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("W via REST API"));
                    }
                    iv->devControlRequest = true; // queue it in the request loop
                }
            }
            if (response["cmd"] == (uint8_t)TurnOff){
                iv->devControlCmd = TurnOff;
                iv->devControlRequest = true; // queue it in the request loop
        }
            if (response["cmd"] == (uint8_t)TurnOn){
                iv->devControlCmd = TurnOn;
                iv->devControlRequest = true; // queue it in the request loop
    }
        }
    }
    request->send(200, "text/json", "{success:true}");
}


//-----------------------------------------------------------------------------
void web::showUpdateForm(AsyncWebServerRequest *request) {
    tmplProc *proc = new tmplProc(request, 850);
    proc->process(update_html, update_html_len, std::bind(&web::showUpdateFormCb, this, std::placeholders::_1));
}


//-----------------------------------------------------------------------------
void web::showUpdate(AsyncWebServerRequest *request) {
    bool reboot = !Update.hasError();

    String html = F("<!doctype html><html><head><title>Update</title><meta http-equiv=\"refresh\" content=\"20; URL=/\"></head><body>Update: ");
    if(reboot)
        html += "success";
    else
        html += "failed";
    html += F("<br/><br/>rebooting ... auto reload after 20s</body></html>");

    AsyncWebServerResponse *response = request->beginResponse(200, F("text/html"), html);
    response->addHeader("Connection", "close");
    request->send(response);
    mMain->mShouldReboot = reboot;
}


//-----------------------------------------------------------------------------
void web::showUpdate2(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if(!index) {
        Serial.printf("Update Start: %s\n", filename.c_str());
        Update.runAsync(true);
        if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
            Update.printError(Serial);
        }
    }
    if(!Update.hasError()) {
        if(Update.write(data, len) != len){
            Update.printError(Serial);
        }
    }
    if(final) {
        if(Update.end(true)) {
            Serial.printf("Update Success: %uB\n", index+len);
        } else {
            Update.printError(Serial);
        }
    }
}


//-----------------------------------------------------------------------------
String web::replaceHtmlGenericKeys(char *key) {
    if(0 == strncmp(key, "VERSION", 7)) return mVersion;
    else if(0 == strncmp(key, "DEVICE", 6))  return mSysCfg->deviceName;
    else if(0 == strncmp(key, "IP", 2)) {
        if(mMain->getWifiApActive()) return F("http://192.168.1.1");
        else          return (F("http://") + String(WiFi.localIP().toString()));
    }

    return "";
}


//-----------------------------------------------------------------------------
String web::showSetupCb(char* key) {
    // PWD will be left at the default value (for protection)
    // -> the PWD will only be changed if it does not match the placeholder "{PWD}"

    String generic = replaceHtmlGenericKeys(key);
    if(generic.length() == 0) {
        if(0 == strncmp(key, "SSID", 4)) return mSysCfg->stationSsid;
        else if(0 == strncmp(key, "PWD", 3)) return F("{PWD}");
        else if(0 == strncmp(key, "PINOUT", 6)) {
            String pinout = "";
            for(uint8_t i = 0; i < 3; i++) {
                pinout += F("<label for=\"") + String(pinArgNames[i]) + "\">" + String(pinNames[i]) + F("</label>");
                pinout += F("<select name=\"") + String(pinArgNames[i]) + "\">";
                for(uint8_t j = 0; j <= 16; j++) {
                    pinout += F("<option value=\"") + String(j) + "\"";
                    switch(i) {
                        default: if(j == mConfig->pinCs)  pinout += F(" selected"); break;
                        case 1:  if(j == mConfig->pinCe)  pinout += F(" selected"); break;
                        case 2:  if(j == mConfig->pinIrq) pinout += F(" selected"); break;
                    }
                    pinout += ">" + String(wemosPins[j]) + F("</option>");
                }
                pinout += F("</select>");
            }
            return pinout;
        }
        else if(0 == strncmp(key, "RF24", 4)) {
            String rf24 = "";
            for(uint8_t i = 0; i <= 3; i++) {
                rf24 += F("<option value=\"") + String(i) + "\"";
                if(i == mConfig->amplifierPower)
                    rf24 += F(" selected");
                rf24 += ">" + String(rf24AmpPowerNames[i]) + F("</option>");
            }
            return rf24;
        }
        else if(0 == strncmp(key, "INV_INTVL", 9)) return String(mConfig->sendInterval);
        else if(0 == strncmp(key, "INV_RETRIES", 11)) return String(mConfig->maxRetransPerPyld);
        else if(0 == strncmp(key, "SER_INTVL", 9)) return String(mConfig->serialInterval);
        else if(0 == strncmp(key, "SER_VAL_CB", 10)) return (mConfig->serialShowIv) ? "checked" : "";
        else if(0 == strncmp(key, "SER_DBG_CB", 10)) return (mConfig->serialDebug) ? "checked" : "";
        else if(0 == strncmp(key, "NTP_ADDR", 8)) return String(mConfig->ntpAddr);
        else if(0 == strncmp(key, "NTP_PORT", 8)) return String(mConfig->ntpPort);
        else if(0 == strncmp(key, "MQTT_ADDR", 9)) return String(mConfig->mqtt.broker);
        else if(0 == strncmp(key, "MQTT_PORT", 9)) return String(mConfig->mqtt.port);
        else if(0 == strncmp(key, "MQTT_USER", 9)) return String(mConfig->mqtt.user);
        else if(0 == strncmp(key, "MQTT_PWD", 8))    return String(mConfig->mqtt.pwd);
        else if(0 == strncmp(key, "MQTT_TOPIC", 10)) return String(mConfig->mqtt.topic);
    }

    return generic;
}


//-----------------------------------------------------------------------------
String web::showUpdateFormCb(char *key) {
    String generic = replaceHtmlGenericKeys(key);
    if(generic.length() == 0) {
        if(0 == strncmp(key, "CONTENT", 7))
            return F("<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
        else if(0 == strncmp(key, "HEAD", 4))
            return "";
    }

    return generic;
}
