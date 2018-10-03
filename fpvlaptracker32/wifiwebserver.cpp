#include "wifiwebserver.h"

#define DEBUG

using namespace comm;

JsonObject& WifiWebServer::prepareJson() {
    this->_jsonBuffer.clear();
    JsonObject& root = this->_jsonBuffer.createObject();
    return root;
}

void WifiWebServer::sendJson(JsonObject& root) {
    String result("");
    root.printTo(result);
    this->_server.send(200, "application/json", result);
}

void WifiWebServer::begin() {

	this->_server.onNotFound([&]() {
        String message = "File Not Found\n\n";
        message += "URI: ";
        message += this->_server.uri();
        message += "\nMethod: ";
        message += (this->_server.method() == HTTP_GET) ? "GET" : "POST";
        message += "\nArguments: ";
        message += this->_server.args();
        message += "\n";

        for (uint8_t i = 0; i < this->_server.args(); i++) {
            message += " " + this->_server.argName(i) + ": " + this->_server.arg(i) + "\n";
        }

        this->_server.send(404, "text/plain", message);
    });

    this->_server.on("/", HTTP_GET, [&]() {
        this->_server.sendHeader("Connection", "close");
        String temp(this->_serverIndex);
        temp.replace("%VERSION%", this->_version);
        this->_server.send(200, "text/html", temp);
    });
    
    this->_server.on("/rssi", HTTP_GET, [&]() {
        this->_server.sendHeader("Connection", "close");
        JsonObject& root = this->prepareJson();
        root["rssi"] = this->_rssi->getRssi();
        this->sendJson(root);
    });

    this->_server.on("/reboot", HTTP_GET, [&]() {
        this->_server.sendHeader("Connection", "close");
        this->_server.send(200, "text/html", "OK");
        ESP.restart();
    });

    this->_server.on("/devicedata", HTTP_GET, [&]() {
        this->_server.sendHeader("Connection", "close");
        JsonObject& root = this->prepareJson();
        root["frequency"] = freq::Frequency::getFrequencyForChannelIndex(this->_storage->getChannelIndex());
        root["minimumLapTime"] = this->_storage->getMinLapTime();
        root["triggerThreshold"] = this->_storage->getTriggerThreshold();
        root["triggerThresholdCalibration"] = this->_storage->getTriggerThresholdCalibration();
        root["calibrationOffset"] = this->_storage->getCalibrationOffset();
        root["state"] = this->_stateManager->toString(this->_stateManager->getState());
        root["triggerValue"] = this->_lapDetector->getTriggerValue();
        root["voltage"] = this->_batteryMgr->getVoltage();
        root["uptime"] = millis() / 1000;
        root["defaultVref"] = this->_storage->getDefaultVref();
        root["rssi"] = this->_rssi->getRssi();
        root["filterRatio"] = this->_storage->getFilterRatio();
        root["filterRatioCalibration"] = this->_storage->getFilterRatioCalibration();
        root["version"] = this->_version;
        root["loopTime"] = *this->_loopTime;
        this->sendJson(root);
    });

    this->_server.on("/devicedata", HTTP_POST, [&]() {
        this->_server.sendHeader("Connection", "close");
        if (this->_server.args() > 0) {
            this->_jsonBuffer.clear();
            JsonObject& root = this->_jsonBuffer.parseObject(this->_server.arg(0));
            if (!root.success()) {
#ifdef DEBUG
                Serial.println(F("failed to parse config"));
#endif
                this->_server.send(500, "text/html", "failed to parse config");
            } else {
#ifdef DEBUG
                Serial.printf("got config: %s\n", this->_server.arg(0).c_str());
#endif
                bool reboot = false;
                this->_storage->setMinLapTime(root["minimumLapTime"]);

                if (this->_storage->getChannelIndex() != freq::Frequency::getChannelIndexForFrequency(root["frequency"])) {
                    reboot = true;
                }
                this->_storage->setChannelIndex(freq::Frequency::getChannelIndexForFrequency(root["frequency"]));

                if (this->_storage->getTriggerThreshold() != root["triggerThreshold"]) {
                    reboot = true;
                }
                this->_storage->setTriggerThreshold(root["triggerThreshold"]);

                if (this->_storage->getTriggerThresholdCalibration() != root["triggerThresholdCalibration"]) {
                    reboot = true;
                }
                this->_storage->setTriggerThresholdCalibration(root["triggerThresholdCalibration"]);

                this->_storage->setCalibrationOffset(root["calibrationOffset"]);
                
                if (this->_storage->getDefaultVref() != root["defaultVref"]) {
                    reboot = true;
                }
                this->_storage->setDefaultVref(root["defaultVref"]);

                this->_storage->setFilterRatio(root["filterRatio"]);
                this->_storage->setFilterRatioCalibration(root["filterRatioCalibration"]);
                if (this->_stateManager->isStateCalibration()) {
                    this->_rssi->setFilterRatio(root["filterRatioCalibration"]);
                } else {
                    this->_rssi->setFilterRatio(root["filterRatio"]);
                }

                // allow setting the trigger value outside of calibration mode
                if (!this->_lapDetector->isCalibrating() && root["triggerValue"] != this->_lapDetector->getTriggerValue()) {
#ifdef DEBUG
                    Serial.printf("setting new trigger value: %d\n", root["triggerValue"]);
#endif
                    this->_lapDetector->setTriggerValue(root["triggerValue"]);
                }

                this->_storage->store();
                this->_lapDetector->init();
                
                String response = F("OK");
                if (reboot) {
                    response += " reboot";
                }
                this->_server.send(200, "text/html", response);
            }
        } else {
            this->_server.sendHeader("Connection", "close");
            this->_server.send(500, "text/html", "no arguments");
        }
    });

    this->_server.begin();
    this->_connected = true;
}

void WifiWebServer::handle() {
    this->_server.handleClient();
}