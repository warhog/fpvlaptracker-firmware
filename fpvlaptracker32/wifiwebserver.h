#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#include "storage.h"
#include "rssi.h"
#include "frequency.h"
#include "rx5808.h"
#include "statemanager.h"
#include "lapdetector.h"
#include "batterymgr.h"

namespace comm {
    class WifiWebServer {
        public:
            WifiWebServer(util::Storage *storage, lap::Rssi *rssi, radio::Rx5808 *rx5808, lap::LapDetector *lapDetector,
                battery::BatteryMgr *batteryMgr, const char *version, statemanagement::StateManager *stateManager,
                unsigned long *loopTime) :
                _storage(storage), _rssi(rssi), _rx5808(rx5808), _lapDetector(lapDetector), _batteryMgr(batteryMgr),
                _version(version), _stateManager(stateManager), _loopTime(loopTime) {}
            void begin();
            void handle();
            bool isConnected() {
                return this->_connected;
            }
        private:
            JsonObject& prepareJson();
            void sendJson(JsonObject& root);
            WebServer _server;
            util::Storage *_storage;
            lap::Rssi *_rssi;
            radio::Rx5808 *_rx5808;
            lap::LapDetector *_lapDetector;
            battery::BatteryMgr *_batteryMgr;
            statemanagement::StateManager *_stateManager;
            DynamicJsonBuffer _jsonBuffer;
            char *_serverIndex = "<style>body { font-family: Arial; }</style><h1>fpvlaptracker32 webserver</h1>current version: %VERSION%<br /><br />";
            const char *_version;
            bool _connected;
            unsigned long *_loopTime;
    };
}