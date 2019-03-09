#include "wifiwebserver.h"

//#define DEBUG

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

String WifiWebServer::concat(String text) {
    return this->_header + text + this->_footer;
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

        this->_server.send(404, "text/plain", this->concat(message));
    });

    this->_server.on("/", HTTP_GET, [&]() {
        this->_server.sendHeader("Connection", "close");
        String temp(this->_serverIndex);
        temp.replace("%VERSION%", this->_version);
        uint64_t chipId = ESP.getEfuseMac();
        char strChipId[15] = { 0 };
        sprintf(strChipId, "%u", chipId);
        String chipString = strChipId;
        temp.replace("%CHIPID%", chipString);
        this->_server.send(200, "text/html", this->concat(temp));
    });
    
    this->_server.on("/factorydefaults", HTTP_GET, [&]() {
        this->_server.sendHeader("Connection", "close");
        this->_server.send(200, "text/html", this->concat("really load factory defaults? <a href='/dofactorydefaults'>yes</a> <a href='/'>no</a>"));
    });
    this->_server.on("/dofactorydefaults", HTTP_GET, [&]() {
        this->_server.sendHeader("Connection", "close");
        this->_storage->loadFactoryDefaults();
        this->_storage->store();
        this->_server.send(200, "text/html", this->concat("factory defaults loaded"));
    });

    this->_server.on("/vref", HTTP_GET, [&]() {
        this->_server.sendHeader("Connection", "close");
        this->_server.send(200, "text/html", this->concat("really goto vref mode? <a href='/dovref'>yes</a> <a href='/'>no</a>"));
    });
    this->_server.on("/dovref", HTTP_GET, [&]() {
        this->_server.sendHeader("Connection", "close");
        this->_stateManager->update(statemanagement::state_enum::VREF_OUTPUT);
        this->_server.send(200, "text/html", this->concat("start VREF output, wifi/bluetooth disabled! reboot to exit this mode."));
    });

    this->_server.on("/bluetooth", HTTP_GET, [&]() {
        this->_server.sendHeader("Connection", "close");
        this->_server.send(200, "text/html", this->concat("switching to bluetooth mode, wifi is now disabled!"));
        this->_stateManager->update(statemanagement::state_enum::SWITCH_TO_BLUETOOTH);
    });

    this->_server.on("/update", HTTP_POST, [&]() {
        this->_server.sendHeader("Connection", "close");
        this->_server.send(200, "text/html", this->concat((Update.hasError()) ? "update failed!\n" : "update successful! rebooting...<a href='/'>back</a>"));
        this->_server.client().setNoDelay(true);
        delay(100);
        this->_server.client().stop();
        ESP.restart();
    }, [&]() {
        HTTPUpload& upload = this->_server.upload();
        if (upload.status == UPLOAD_FILE_START) {
#ifdef DEBUG
            Serial.setDebugOutput(true);
            Serial.printf("Update: %s\n", upload.filename.c_str());
#endif
            if (!Update.begin()) { //start with max available size
#ifdef DEBUG
                Update.printError(Serial);
#endif
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
#ifdef DEBUG
                Update.printError(Serial);
#endif
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) { //true to set the size to the current progress
#ifdef DEBUG
                Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
#endif
            } else {
#ifdef DEBUG
                Update.printError(Serial);
#endif
            }
#ifdef DEBUG
            Serial.setDebugOutput(false);
#endif
        }
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
        this->_server.client().flush();
        this->_server.client().stop();
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

    this->_server.on("/skipcalibration", HTTP_GET, [&]() {
        this->_server.sendHeader("Connection", "close");
        JsonObject& root = this->prepareJson();
        root["result"] = "NOK";
        if (this->_stateManager->isStateCalibration()) {
            this->_stateManager->setState(statemanagement::state_enum::CALIBRATION_DONE);
            this->_lapDetector->disableCalibrationMode();
            root["result"] = "OK";
        }
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
                if (root["minimumLapTime"] > 1000 && root["minimumLapTime"] < 60000) {
                    this->_storage->setMinLapTime(root["minimumLapTime"]);
                }

                if (root["frequency"] >= 5325 && root["frequency"] <= 5945) {
                    if (this->_storage->getChannelIndex() != freq::Frequency::getChannelIndexForFrequency(root["frequency"])) {
                        reboot = true;
                    }
                    this->_storage->setChannelIndex(freq::Frequency::getChannelIndexForFrequency(root["frequency"]));
                }

                if (root["triggerThreshold"] > 10 && root["triggerThreshold"] < 1024) {
                    if (this->_storage->getTriggerThreshold() != root["triggerThreshold"]) {
                        reboot = true;
                    }
                    this->_storage->setTriggerThreshold(root["triggerThreshold"]);
                }

                if (root["triggerThresholdCalibration"] > 10 && root["triggerThresholdCalibration"] < 1024) {
                    if (this->_storage->getTriggerThresholdCalibration() != root["triggerThresholdCalibration"]) {
                        reboot = true;
                    }
                    this->_storage->setTriggerThresholdCalibration(root["triggerThresholdCalibration"]);
                }

                if (root["calibrationOffset"] > 1 && root["calibrationOffset"] < 256) {
                    this->_storage->setCalibrationOffset(root["calibrationOffset"]);
                }
                
                if (root["defaultVref"] > 999 && root["defaultVref"] < 1200) {
                    if (this->_storage->getDefaultVref() != root["defaultVref"]) {
                        reboot = true;
                    }
                    this->_storage->setDefaultVref(root["defaultVref"]);
                }

                if (root["filterRatio"] > 0.0 && root["filterRatio"] < 1.0) {
                    this->_storage->setFilterRatio(root["filterRatio"]);
                }
                if (root["filterRatioCalibration"] > 0.0 && root["filterRatioCalibration"] < 1.0) {
                    this->_storage->setFilterRatioCalibration(root["filterRatioCalibration"]);
                }
                if (this->_stateManager->isStateCalibration()) {
                    this->_rssi->setFilterRatio(root["filterRatioCalibration"]);
                } else {
                    this->_rssi->setFilterRatio(root["filterRatio"]);
                }

                if (root["triggerValue"] > 10 && root["triggerValue"] < 1024) {
                    // allow setting the trigger value outside of calibration mode
                    if (!this->_lapDetector->isCalibrating() && root["triggerValue"] != this->_lapDetector->getTriggerValue()) {
#ifdef DEBUG
                        Serial.printf("setting new trigger value: %d\n", root["triggerValue"]);
#endif
                        this->_lapDetector->setTriggerValue(root["triggerValue"]);
                    }
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