#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

#include "storage.h"
#include "batterymgr.h"

namespace comm {
    class WifiWebServer {
        public:
            WifiWebServer(util::Storage *storage, battery::BatteryMgr *batteryMgr, const char *version, unsigned long *loopTime) :
                _storage(storage), _batteryMgr(batteryMgr), _version(version), _loopTime(loopTime), _server(80) {}
            void begin();
            void handle();
            bool isConnected() {
                return this->_connected;
            }
            void disconnect() {
                this->_connected = false;
                this->_server.client().flush();
                this->_server.client().stop();
                this->_server.stop();
            }
        private:
            JsonObject& prepareJson();
            void sendJson(JsonObject& root);
            String concat(String text);

            ESP8266WebServer _server;
            util::Storage *_storage;
            battery::BatteryMgr *_batteryMgr;
            DynamicJsonBuffer _jsonBuffer;
            const char *_version;
            bool _connected;
            unsigned long *_loopTime;
            char const *_header = "<html><head><style>body { font-family: Arial; background: #E6E6E6; } a { color: #0000ff; } #content { margin: auto; border-radius: 15px; background: #ffffff; padding: 20px; width: 600px;}</style></head><body><div id='content'><h1>fpvlaptracker-led</h1>";
            char const *_footer = "</div></body></html>";
            char const *_serverIndex = "chip id: %CHIPID%<br />current version: %VERSION%<br />build date: " __DATE__ "  " __TIME__ "<br /><br /><form method='POST' action='/setup'>ssid: <input type='text' name='ssid' value='%SSID%' /><br />password: <input type='text' name='password' value='%PASSWORD%' /><br /><input type='submit' value='Save' /></form><br /><hr size='1'/><h2>maintenance</h2>select .bin file to flash.<br /><br /><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='update'></form><br /><a href='/factorydefaults'>load factory defaults</a><br /><br /><a href='/vref'>output VREF</a>";
    };
}