#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#include "comm.h"
#include "storage.h"
#include "rssi.h"
#include "frequency.h"
#include "rx5808.h"
#include "statemanager.h"
#include "lapdetector.h"
#include "batterymgr.h"

namespace comm {

    class WifiComm : public Comm {
    public:
        WifiComm(util::Storage *storage, lap::Rssi *rssi, radio::Rx5808 *rx5808, lap::LapDetector *lapDetector, battery::BatteryMgr *batteryMgr, const char *version, statemanagement::StateManager *stateManager, unsigned long *loopTime);
        void reg();
        void lap(unsigned long lapTime, unsigned int rssi);
        int connect();
        void processIncomingMessage();
        void sendUdpMessage(String msg);
        void sendCalibrationDone();
        void sendData();
        void disconnect();
        void sendVoltageAlarm();

    private:
        String getBroadcastIP();
        WiFiUDP _udp;
        bool _wifiSsidFound;
    };

}