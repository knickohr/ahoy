//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

// https://bert.emelis.net/espMqttClient/

#ifndef __PUB_MQTT_H__
#define __PUB_MQTT_H__

#if defined(ENABLE_MQTT)
#ifdef ESP8266
    #include <ESP8266WiFi.h>
    #define xSemaphoreTake(a, b) { while(a) { yield(); } a = true; }
    #define xSemaphoreGive(a) { a = false; }
#elif defined(ESP32)
    #include <WiFi.h>
#endif

#include <array>
#include "../utils/dbg.h"
#include "../config/config.h"
#include <espMqttClient.h>
#include <ArduinoJson.h>
#include "../defines.h"
#include "../hm/hmSystem.h"

#include "pubMqttDefs.h"
#include "pubMqttIvData.h"

typedef std::function<void(const char*, const uint8_t*, size_t, size_t, size_t)> subscriptionCb;
typedef std::function<void(void)> connectionCb;

typedef struct {
    bool running;
    uint8_t lastIvId;
    uint8_t sub;
    uint8_t foundIvCnt;
} discovery_t;

template<class HMSYSTEM>
class PubMqtt {
    public:
        PubMqtt() : SendIvData() {
            #if defined(ESP32)
                mutex = xSemaphoreCreateBinaryStatic(&mutexBuffer);
                xSemaphoreGive(mutex);
            #else
                mutex = false;
            #endif

            mLastIvState.fill(InverterStatus::OFF);
            mIvLastRTRpub.fill(0);

            mVal.fill(0);
            mTopic.fill(0);
            mSubTopic.fill(0);
            mClientId.fill(0);
            mLwtTopic.fill(0);
            mSendAlarm.fill(false);
        }

        ~PubMqtt() {
            #if defined(ESP32)
            vSemaphoreDelete(mutex);
            #endif
        }

        void setup(IApp *app, cfgMqtt_t *cfg_mqtt, const char *devName, const char *version, HMSYSTEM *sys, uint32_t *utcTs, uint32_t *uptime) {
            mApp             = app;
            mCfgMqtt         = cfg_mqtt;
            mDevName         = devName;
            mVersion         = version;
            mSys             = sys;
            mUtcTimestamp    = utcTs;
            mUptime          = uptime;
            mIntervalTimeout = 1;

            SendIvData.setup(app, sys, cfg_mqtt, utcTs, &mSendList);
            SendIvData.setPublishFunc([this](const char *subTopic, const char *payload, bool retained, uint8_t qos) {
                publish(subTopic, payload, retained, true, qos);
            });
            mDiscovery.running = false;

            snprintf(mLwtTopic.data(), mLwtTopic.size(), "%s/mqtt", mCfgMqtt->topic);

            if((strlen(mCfgMqtt->user) > 0) && (strlen(mCfgMqtt->pwd) > 0))
                mClient.setCredentials(mCfgMqtt->user, mCfgMqtt->pwd);

            if(strlen(mCfgMqtt->clientId) > 0)
                snprintf(mClientId.data(), mClientId.size(), "%s", mCfgMqtt->clientId);
            else {
                snprintf(mClientId.data(), mClientId.size(), "%s-", mDevName);
                uint8_t pos = strlen(mClientId.data());
                mClientId[pos++] = WiFi.macAddress().substring( 9, 10).c_str()[0];
                mClientId[pos++] = WiFi.macAddress().substring(10, 11).c_str()[0];
                mClientId[pos++] = WiFi.macAddress().substring(12, 13).c_str()[0];
                mClientId[pos++] = WiFi.macAddress().substring(13, 14).c_str()[0];
                mClientId[pos++] = WiFi.macAddress().substring(15, 16).c_str()[0];
                mClientId[pos++] = WiFi.macAddress().substring(16, 17).c_str()[0];
                mClientId[pos++] = '\0';
            }

            mClient.setClientId(mClientId.data());
            mClient.setServer(mCfgMqtt->broker, mCfgMqtt->port);
            mClient.setWill(mLwtTopic.data(), QOS_0, true, mqttStr[MQTT_STR_LWT_NOT_CONN]);
            mClient.onConnect(std::bind(&PubMqtt::onConnect, this, std::placeholders::_1));
            mClient.onDisconnect(std::bind(&PubMqtt::onDisconnect, this, std::placeholders::_1));
            mClient.onMessage(std::bind(&PubMqtt::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
        }

        void loop() {
            std::queue<message_s> queue;
            xSemaphoreTake(mutex, portMAX_DELAY);
            queue.swap(mReceiveQueue);
            xSemaphoreGive(mutex);

            while (!queue.empty())
            {
#warning "TODO: sTopic und sPayload hier fällen und dann übergeben?";
#warning "Ist es wirklich so, dass die Payload eines Topics aus mehreren Nachrichten bestehen kann und erst zusammengesetzt werden muss? Was ist dann mit der Reihenfolge?";
                message_s *entry = &queue.front();
                if(NULL != mSubscriptionCb)
                {
                    (mSubscriptionCb)(entry->topic, entry->payload, entry->len, entry->index, entry->total);
                    mRxCnt++;
                }
                queue.pop();
            }

            SendIvData.loop();

            #if defined(ESP8266)
            mClient.loop();
            yield();
            #endif

            if(mDiscovery.running)
                discoveryConfigLoop();
        }

        void tickerSecond() {
            if (mIntervalTimeout > 0)
                mIntervalTimeout--;

            if(mClient.disconnected()) {
                mClient.connect();
                return; // next try in a second
            }

            if(0 == mCfgMqtt->interval) // no fixed interval, publish once new data were received (from inverter)
                sendIvData();
            else { // send mqtt data in a fixed interval
                if(mIntervalTimeout == 0) {
                    mIntervalTimeout = mCfgMqtt->interval;
                    mSendList.push(sendListCmdIv(RealTimeRunData_Debug, NULL));
                    sendIvData();
                }
            }

            sendAlarmData();
        }

        void tickerMinute() {
            snprintf(mVal.data(), mVal.size(), "%u", (*mUptime));
            publish(subtopics[MQTT_UPTIME], mVal.data());
            publish(subtopics[MQTT_RSSI], String(WiFi.RSSI()).c_str());
            publish(subtopics[MQTT_FREE_HEAP], String(ESP.getFreeHeap()).c_str());
            #ifndef ESP32
            publish(subtopics[MQTT_HEAP_FRAG], String(ESP.getHeapFragmentation()).c_str());
            #endif
        }

        bool tickerSun(uint32_t sunrise, uint32_t sunset, int16_t offsM, int16_t offsE, bool isSunrise = false) {
            if (!mClient.connected())
                return false;

            publish(subtopics[MQTT_SUNRISE], String(sunrise).c_str(), true);
            publish(subtopics[MQTT_SUNSET], String(sunset).c_str(), true);
            publish(subtopics[MQTT_COMM_START], String(sunrise + offsM).c_str(), true);
            publish(subtopics[MQTT_COMM_STOP], String(sunset + offsE).c_str(), true);

            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                Inverter<> *iv = mSys->getInverterByPos(i);
                if(NULL == iv)
                    continue;

                snprintf(mSubTopic.data(), mSubTopic.size(), "%s/dis_night_comm", iv->config->name);
                publish(mSubTopic.data(), ((iv->commEnabled) ? dict[STR_TRUE] : dict[STR_FALSE]), true);
            }

            snprintf(mSubTopic.data(), mSubTopic.size(), "comm_disabled");
            publish(mSubTopic.data(), (((*mUtcTimestamp > (sunset + offsE)) || (*mUtcTimestamp < (sunrise + offsM))) ? dict[STR_TRUE] : dict[STR_FALSE]), true);

            return true;
        }

        void notAvailChanged(bool allNotAvail) {
            if(!allNotAvail)
                SendIvData.resetYieldDay();
        }

        bool tickerComm(bool disabled) {
             if (!mClient.connected())
                return false;

            publish(subtopics[MQTT_COMM_DISABLED], ((disabled) ? dict[STR_TRUE] : dict[STR_FALSE]), true);
            publish(subtopics[MQTT_COMM_DIS_TS], String(*mUtcTimestamp).c_str(), true);

            return true;
        }

        void tickerMidnight() {
            // set Total YieldDay to zero
            snprintf(mSubTopic.data(), mSubTopic.size(), "total/%s", fields[FLD_YD]);
            snprintf(mVal.data(), mVal.size(), "0");
            publish(mSubTopic.data(), mVal.data(), true);
        }

        void payloadEventListener(uint8_t cmd, Inverter<> *iv) {
            if(mClient.connected()) { // prevent overflow if MQTT broker is not reachable but set
                if((0 == mCfgMqtt->interval) || (RealTimeRunData_Debug != cmd)) // no interval or no live data
                    mSendList.push(sendListCmdIv(cmd, iv));
            }
        }

        void alarmEvent(Inverter<> *iv) {
            mSendAlarm[iv->id] = true;
        }

        void publish(const char *subTopic, const char *payload, bool retained = false, bool addTopic = true, uint8_t qos = QOS_0) {
            if(!mClient.connected())
                return;

            if(addTopic)
                snprintf(mTopic.data(), mTopic.size(), "%s/%s", mCfgMqtt->topic, subTopic);
            else
                snprintf(mTopic.data(), mTopic.size(), "%s", subTopic);

            if(!mCfgMqtt->enableRetain)
                retained = false;

            mClient.publish(mTopic.data(), qos, retained, payload);
            yield();
            mTxCnt++;
        }

        void subscribe(const char *subTopic, uint8_t qos = QOS_0) {
            char topic[MQTT_TOPIC_LEN + 20];
            snprintf(topic, (MQTT_TOPIC_LEN + 20), "%s/%s", mCfgMqtt->topic, subTopic);
            mClient.subscribe(topic, qos);
        }

        void subscribeExtern(const char *subTopic, uint8_t qos = QOS_0) {
            char topic[MQTT_TOPIC_LEN + 20];
            snprintf(topic, (MQTT_TOPIC_LEN + 20), "%s", subTopic);
            mClient.subscribe(topic, qos);
        }

        // new - need to unsubscribe the topics.
        void unsubscribeExtern(const char *subTopic)
        {
            mClient.unsubscribe(subTopic);  // add as many topics as you like
        }

        void setConnectionCb(connectionCb cb) {
            mConnectionCb = cb;
        }

        void setSubscriptionCb(subscriptionCb cb) {
            mSubscriptionCb = cb;
        }

        inline bool isConnected() {
            return mClient.connected();
        }

        inline uint32_t getTxCnt(void) {
            return mTxCnt;
        }

        inline uint32_t getRxCnt(void) {
            return mRxCnt;
        }

        void sendDiscoveryConfig(void) {
            DPRINTLN(DBG_VERBOSE, F("sendMqttDiscoveryConfig"));
            mDiscovery.running  = true;
            mDiscovery.lastIvId = 0;
            mDiscovery.sub = 0;
            mDiscovery.foundIvCnt = 0;
        }

        void setPowerLimitAck(Inverter<> *iv) {
            if (NULL != iv) {
                snprintf(mSubTopic.data(), mSubTopic.size(), "%s/%s", iv->config->name, subtopics[MQTT_ACK_PWR_LMT]);
                snprintf(mVal.data(), mVal.size(), "%.1f", iv->powerLimit[0]/10.0);
                publish(mSubTopic.data(), mVal.data(), true, true, QOS_2);
            }
        }

    private:
        void onConnect(bool sessionPreset) {
            DPRINTLN(DBG_INFO, F("MQTT connected"));

            publish(subtopics[MQTT_VERSION], mVersion, false);
            publish(subtopics[MQTT_DEVICE], mDevName, false);
            publish(subtopics[MQTT_IP_ADDR], mApp->getIp().c_str(), true);
            tickerMinute();
            publish(mLwtTopic.data(), mqttStr[MQTT_STR_LWT_CONN], true, false);

            snprintf(mVal.data(), mVal.size(), "ctrl/restart_ahoy");
            subscribe(mVal.data());

           for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                snprintf(mVal.data(), mVal.size(), "ctrl/limit/%d", i);
                subscribe(mVal.data(), QOS_2);
                snprintf(mVal.data(), mVal.size(), "ctrl/restart/%d", i);
                subscribe(mVal.data());
                snprintf(mVal.data(), mVal.size(), "ctrl/power/%d", i);
                subscribe(mVal.data());
            }
            snprintf(mVal.data(), mVal.size(), "ctrl/#");
            subscribe(mVal.data(), QOS_2);
            subscribe(subscr[MQTT_SUBS_SET_TIME]);

            if(NULL == mConnectionCb)
                return;
            (mConnectionCb)();
        }

        void onDisconnect(espMqttClientTypes::DisconnectReason reason) {
            DPRINT(DBG_INFO, F("MQTT disconnected, reason: "));
            switch (reason) {
                case espMqttClientTypes::DisconnectReason::TCP_DISCONNECTED:
                    DBGPRINTLN(F("TCP disconnect"));
                    break;
                case espMqttClientTypes::DisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
                    DBGPRINTLN(F("wrong protocol version"));
                    break;
                case espMqttClientTypes::DisconnectReason::MQTT_IDENTIFIER_REJECTED:
                    DBGPRINTLN(F("identifier rejected"));
                    break;
                case espMqttClientTypes::DisconnectReason::MQTT_SERVER_UNAVAILABLE:
                    DBGPRINTLN(F("broker unavailable"));
                    break;
                case espMqttClientTypes::DisconnectReason::MQTT_MALFORMED_CREDENTIALS:
                    DBGPRINTLN(F("malformed credentials"));
                    break;
                case espMqttClientTypes::DisconnectReason::MQTT_NOT_AUTHORIZED:
                    DBGPRINTLN(F("not authorized"));
                    break;
                default:
                    DBGPRINTLN(F("unknown"));
            }
        }

        void onMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
        {
#warning "TODO: if aktivieren nach Logprüfung. Was bedeutet index und total?";
//            if (total != 1) {
//                DPRINTLN(DBG_ERROR, String("pubMqtt.h: onMessage ERROR: index=") + String(index) + String(" total=") + String(total));
//                return;
//            }

            if (len == 0) {
                DPRINT(DBG_INFO, String("MQTT-topic: "));
                DPRINT(DBG_INFO, String(topic));
                DPRINTLN(DBG_INFO, String(" is empty."));
                return;
            }

            xSemaphoreTake(mutex, portMAX_DELAY);
            mReceiveQueue.push(message_s(topic, payload, len, index, total));
            xSemaphoreGive(mutex);

        }

        void discoveryConfigLoop(void) {
            DynamicJsonDocument doc(256);

            constexpr static uint8_t fldTotal[] = {FLD_PAC, FLD_YT, FLD_YD, FLD_PDC};

            String node_id = String(mDevName) + "_TOTAL";
            bool total = (mDiscovery.lastIvId == MAX_NUM_INVERTERS);

            Inverter<> *iv = mSys->getInverterByPos(mDiscovery.lastIvId);
            record_t<> *rec = NULL;
            if (NULL != iv) {
                rec = iv->getRecordStruct(RealTimeRunData_Debug);
                if(0 == mDiscovery.sub)
                mDiscovery.foundIvCnt++;
            }

            if ((NULL != iv) || total) {
                if (!total) {
                    doc[F("name")] = iv->config->name;
                    doc[F("ids")] = String(iv->config->serial.u64, HEX);
                    doc[F("mdl")] = iv->config->name;
                }
                else {
                    doc[F("name")] = node_id;
                    doc[F("ids")] = node_id;
                    doc[F("mdl")] = node_id;
                }

                doc[F("cu")] = F("http://") + String(WiFi.localIP().toString());
                doc[F("mf")] = F("Hoymiles");
                JsonObject deviceObj = doc.as<JsonObject>(); // deviceObj is only pointer!?

                std::array<char, 64> topic;
                std::array<char, 32> name;
                std::array<char, 32> uniq_id;
                std::array<char, 350> buf;
                topic.fill(0);
                name.fill(0);
                uniq_id.fill(0);
                buf.fill(0);
                const char *devCls, *stateCls;
                if (!total) {
                    if (rec->assign[mDiscovery.sub].ch == CH0)
                        snprintf(name.data(), name.size(), "%s", iv->getFieldName(mDiscovery.sub, rec));
                    else
                        snprintf(name.data(), name.size(), "CH%d_%s", rec->assign[mDiscovery.sub].ch, iv->getFieldName(mDiscovery.sub, rec));
                    snprintf(topic.data(), name.size(), "/ch%d/%s", rec->assign[mDiscovery.sub].ch, iv->getFieldName(mDiscovery.sub, rec));
                    snprintf(uniq_id.data(), uniq_id.size(), "ch%d_%s", rec->assign[mDiscovery.sub].ch, iv->getFieldName(mDiscovery.sub, rec));

                    devCls = getFieldDeviceClass(rec->assign[mDiscovery.sub].fieldId);
                    stateCls = getFieldStateClass(rec->assign[mDiscovery.sub].fieldId);
                }

                else { // total values
                    snprintf(name.data(), name.size(), "Total %s", fields[fldTotal[mDiscovery.sub]]);
                    snprintf(topic.data(), topic.size(), "/%s", fields[fldTotal[mDiscovery.sub]]);
                    snprintf(uniq_id.data(), uniq_id.size(), "total_%s", fields[fldTotal[mDiscovery.sub]]);
                    devCls = getFieldDeviceClass(fldTotal[mDiscovery.sub]);
                    stateCls = getFieldStateClass(fldTotal[mDiscovery.sub]);
                }

                DynamicJsonDocument doc2(512);
                constexpr static const char* unitTotal[] = {"W", "kWh", "Wh", "W"};
                doc2[F("name")] = String(name.data());
                doc2[F("stat_t")] = String(mCfgMqtt->topic) + "/" + ((!total) ? String(iv->config->name) : "total" ) + String(topic.data());
                doc2[F("unit_of_meas")] = ((!total) ? (iv->getUnit(mDiscovery.sub, rec)) : (unitTotal[mDiscovery.sub]));
                doc2[F("uniq_id")] = ((!total) ? (String(iv->config->serial.u64, HEX)) : (node_id)) + "_" + uniq_id.data();
                doc2[F("dev")] = deviceObj;
                if (!(String(stateCls) == String("total_increasing")))
                    doc2[F("exp_aft")] = MQTT_INTERVAL + 5;  // add 5 sec if connection is bad or ESP too slow @TODO: stimmt das wirklich als expire!?
                if (devCls != NULL)
                    doc2[F("dev_cla")] = String(devCls);
                if (stateCls != NULL)
                    doc2[F("stat_cla")] = String(stateCls);

                if (!total)
                    snprintf(topic.data(), topic.size(), "%s/sensor/%s/ch%d_%s/config", MQTT_DISCOVERY_PREFIX, iv->config->name, rec->assign[mDiscovery.sub].ch, iv->getFieldName(mDiscovery.sub, rec));
                else // total values
                    snprintf(topic.data(), topic.size(), "%s/sensor/%s/total_%s/config", MQTT_DISCOVERY_PREFIX, node_id.c_str(), fields[fldTotal[mDiscovery.sub]]);
                size_t size = measureJson(doc2) + 1;
                serializeJson(doc2, buf.data(), size);
                if(FLD_EVT != rec->assign[mDiscovery.sub].fieldId)
                    publish(topic.data(), buf.data(), true, false);

                if(++mDiscovery.sub == ((!total) ? (rec->length) : 4)) {
                    mDiscovery.sub = 0;
                    checkDiscoveryEnd();
                }
            } else {
                mDiscovery.sub = 0;
                checkDiscoveryEnd();
            }

            yield();
        }

        void checkDiscoveryEnd(void) {
            if(++mDiscovery.lastIvId == MAX_NUM_INVERTERS) {
                // check if only one inverter was found, then don't create 'total' sensor
                if(mDiscovery.foundIvCnt == 1)
                    mDiscovery.running = false;
            } else if(mDiscovery.lastIvId == (MAX_NUM_INVERTERS + 1))
                mDiscovery.running = false;
        }

        const char *getFieldDeviceClass(uint8_t fieldId) {
            uint8_t pos = 0;
            for (; pos < DEVICE_CLS_ASSIGN_LIST_LEN; pos++) {
                if (deviceFieldAssignment[pos].fieldId == fieldId)
                    break;
            }
            return (pos >= DEVICE_CLS_ASSIGN_LIST_LEN) ? NULL : deviceClasses[deviceFieldAssignment[pos].deviceClsId];
        }

        const char *getFieldStateClass(uint8_t fieldId) {
            uint8_t pos = 0;
            for (; pos < DEVICE_CLS_ASSIGN_LIST_LEN; pos++) {
                if (deviceFieldAssignment[pos].fieldId == fieldId)
                    break;
            }
            return (pos >= DEVICE_CLS_ASSIGN_LIST_LEN) ? NULL : stateClasses[deviceFieldAssignment[pos].stateClsId];
        }

        bool processIvStatus() {
            // returns true if any inverter is available
            bool allAvail = true;   // shows if all enabled inverters are available
            bool anyAvail = false;  // shows if at least one enabled inverter is available
            bool changed = false;
            Inverter<> *iv;

            for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                iv = mSys->getInverterByPos(id);
                if (NULL == iv)
                    continue; // skip to next inverter
                if (!iv->config->enabled)
                    continue; // skip to next inverter

                // inverter status
                InverterStatus status = iv->getStatus();
                if (InverterStatus::OFF < status)
                    anyAvail = true;
                else // inverter is enabled but not available
                    allAvail = false;

                if(mLastIvState[id] != status) {
                    // if status changed from producing to not producing send last data immediately
                    if (InverterStatus::WAS_PRODUCING == mLastIvState[id])
                        sendData(iv, RealTimeRunData_Debug);

                    mLastIvState[id] = status;
                    changed = true;

                    snprintf(mSubTopic.data(), mSubTopic.size(), "%s/available", iv->config->name);
                    snprintf(mVal.data(), mVal.size(), "%d", (uint8_t)status);
                    publish(mSubTopic.data(), mVal.data(), true);
                }
            }

            if(changed) {
                snprintf(mVal.data(), mVal.size(), "%d", ((allAvail) ? MQTT_STATUS_ONLINE : ((anyAvail) ? MQTT_STATUS_PARTIAL : MQTT_STATUS_OFFLINE)));
                publish("status", mVal.data(), true);
            }

            return anyAvail;
        }

        void sendAlarmData() {
            Inverter<> *iv;
            uint32_t localTime = gTimezone.toLocal(*mUtcTimestamp);
            uint32_t lastMidnight = gTimezone.toUTC(localTime - (localTime % 86400));  // last midnight local time
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                if(!mSendAlarm[i])
                    continue;

                iv = mSys->getInverterByPos(i, false);
                if (NULL == iv)
                    continue;
                if (!iv->config->enabled)
                    continue;

                mSendAlarm[i] = false;

                snprintf(mSubTopic.data(), mSubTopic.size(), "%s/alarm/cnt", iv->config->name);
                snprintf(mVal.data(), mVal.size(), "%d", iv->alarmCnt);
                publish(mSubTopic.data(), mVal.data(), false);

                for(uint8_t j = 0; j < 10; j++) {
                    if(0 != iv->lastAlarm[j].code) {
                        snprintf(mSubTopic.data(), mSubTopic.size(), "%s/alarm/%d", iv->config->name, j);
                        snprintf(mVal.data(), mVal.size(), "{\"code\":%d,\"str\":\"%s\",\"start\":%d,\"end\":%d}",
                            iv->lastAlarm[j].code,
                            iv->getAlarmStr(iv->lastAlarm[j].code).c_str(),
                            iv->lastAlarm[j].start + lastMidnight,
                            iv->lastAlarm[j].end + lastMidnight);
                        publish(mSubTopic.data(), mVal.data(), false);
                        yield();
                    }
                }
            }
        }

        void sendData(Inverter<> *iv, uint8_t curInfoCmd) {
            if (mCfgMqtt->json)
                return;

            record_t<> *rec = iv->getRecordStruct(curInfoCmd);

            uint32_t lastTs = iv->getLastTs(rec);
            bool pubData = (lastTs > 0);
            if (curInfoCmd == RealTimeRunData_Debug)
                pubData &= (lastTs != mIvLastRTRpub[iv->id]);

            if (pubData) {
                mIvLastRTRpub[iv->id] = lastTs;
                for (uint8_t i = 0; i < rec->length; i++) {
                    bool retained = false;
                    if (curInfoCmd == RealTimeRunData_Debug) {
                        switch (rec->assign[i].fieldId) {
                            case FLD_YT:
                            case FLD_YD:
                                if ((rec->assign[i].ch == CH0) && (!iv->isProducing())) // avoids returns to 0 on restart
                                    continue;
                                retained = true;
                                break;
                        }
                    }

                    snprintf(mSubTopic.data(), mSubTopic.size(), "%s/ch%d/%s", iv->config->name, rec->assign[i].ch, fields[rec->assign[i].fieldId]);
                    snprintf(mVal.data(), mVal.size(), "%g", ah::round3(iv->getValue(i, rec)));
                    publish(mSubTopic.data(), mVal.data(), retained);

                    yield();
                }
            }
        }

        void sendIvData() {
            bool anyAvail = processIvStatus();
            if (mLastAnyAvail != anyAvail)
                mSendList.push(sendListCmdIv(RealTimeRunData_Debug, NULL));  // makes sure that total values are calculated

            if(mSendList.empty())
                return;

            SendIvData.start();
            mLastAnyAvail = anyAvail;
        }

    private:
        enum {MQTT_STATUS_OFFLINE = 0, MQTT_STATUS_PARTIAL, MQTT_STATUS_ONLINE};

        struct message_s
        {
            char *topic;
            uint8_t *payload;
            size_t len;
            size_t index;
            size_t total;

            message_s()
            : topic { nullptr }
            , payload { nullptr }
            , len { 0 }
            , index { 0 }
            , total { 0 }
            {}

            message_s(const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
            {
                uint8_t topic_len = strlen(topic) + 1;
                this->topic = new char[topic_len];
                this->payload = new uint8_t[len];

                memcpy(this->topic, topic, topic_len);
                memcpy(this->payload, payload, len);
                this->len = len;
                this->index = index;
                this->total = total;
            }

            message_s(const message_s &) = delete;

            message_s(message_s && other) : message_s {}
            {
                this->swap( other );
            }

            ~message_s()
            {
                delete[] this->topic;
                delete[] this->payload;
            }

            message_s  &operator = (const message_s &) = delete;

            message_s  &operator = (message_s &&other)
            {
                this->swap(other);
                return *this;
            }

            void swap(message_s &other)
            {
                std::swap(this->topic, other.topic);
                std::swap(this->payload, other.payload);
                std::swap(this->len, other.len);
                std::swap(this->index, other.index);
                std::swap(this->total, other.total);
            }

        };

    private:
        espMqttClient mClient;
        cfgMqtt_t *mCfgMqtt = nullptr;
        IApp *mApp;
        #if defined(ESP8266)
        WiFiEventHandler mHWifiCon, mHWifiDiscon;
        volatile bool mutex;
        #else
        SemaphoreHandle_t mutex;
        StaticSemaphore_t mutexBuffer;
        #endif

        HMSYSTEM *mSys = nullptr;
        PubMqttIvData<HMSYSTEM> SendIvData;

        uint32_t *mUtcTimestamp = nullptr, *mUptime = nullptr;
        uint32_t mRxCnt = 0, mTxCnt = 0;
        std::queue<sendListCmdIv> mSendList;
        std::array<bool, MAX_NUM_INVERTERS> mSendAlarm;
        connectionCb mConnectionCb = nullptr;
        subscriptionCb mSubscriptionCb = nullptr;
        bool mLastAnyAvail = false;
        std::array<InverterStatus, MAX_NUM_INVERTERS> mLastIvState;
        std::array<uint32_t, MAX_NUM_INVERTERS> mIvLastRTRpub;
        uint16_t mIntervalTimeout = 0;

        std::queue<message_s> mReceiveQueue;

        // last will topic and payload must be available through lifetime of 'espMqttClient'
        std::array<char, (MQTT_TOPIC_LEN + 5)> mLwtTopic;
        const char *mDevName = nullptr, *mVersion = nullptr;
        std::array<char, 24> mClientId; // number of chars is limited to 23 up to v3.1 of MQTT
        // global buffer for mqtt topic. Used when publishing mqtt messages.
        std::array<char, (MQTT_TOPIC_LEN + 32 + MAX_NAME_LENGTH + 1)> mTopic;
        std::array<char, (32 + MAX_NAME_LENGTH + 1)> mSubTopic;
        std::array<char, 100> mVal;
        discovery_t mDiscovery = {true, 0, 0, 0};
};

#endif /*ENABLE_MQTT*/
#endif /*__PUB_MQTT_H__*/
