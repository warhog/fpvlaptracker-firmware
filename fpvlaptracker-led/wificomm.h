#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NeoPixelBus.h>

#include "storage.h"
#include "batterymgr.h"
#include "ws2812.h"
#include "PixelEntry.h"
#include "ColorEntry.h"

namespace comm {

    class WifiComm {
    public:
        WifiComm(util::Storage *storage, battery::BatteryMgr *batteryMgr, const char *version, unsigned long *loopTime, ledio::Ws2812 *ws2812);
        int connect();
        void processIncomingMessage();
        void sendUdpMessage(String msg);
        void reg();
        void sendData();
        void disconnect();
        void sendVoltageAlarm();
        bool isConnected() const {
            return this->_connected;
        }

    private:
        RgbColor convertColor(String color);
        void processRequest(String request);
        String split(String s, char delimiter, int index);
        String getBroadcastIP();
        
        util::Storage *_storage;
        WiFiUDP _udp;
        bool _wifiSsidFound;
        const char *_version;
        unsigned long *_loopTime;
        bool _connected;
        battery::BatteryMgr *_batteryMgr;
        ledio::Ws2812 *_ws2812;
    };

}