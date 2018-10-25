#include "btcomm.h"

using namespace comm;

//#define DEBUG

BtComm::BtComm(BluetoothSerial *btSerial, util::Storage *storage, lap::Rssi *rssi, radio::Rx5808 *rx5808,
    lap::LapDetector *lapDetector, battery::BatteryMgr *batteryMgr, const char *version,
    statemanagement::StateManager *stateManager, comm::WifiComm *wifiComm, unsigned long *loopTime) : 
    Comm(storage, rssi, rx5808, lapDetector, batteryMgr, version, stateManager, loopTime), _serialGotLine(false),
    _serialString(false), _btSerial(btSerial), _jsonBuffer(400), _wifiComm(wifiComm) {
        this->_version = version;
}

int BtComm::connect() {
#ifdef DEBUG
    Serial.println(F("bluetooth connect()"));
#endif
    uint64_t chipId = ESP.getEfuseMac();
    char strChipId[15] = { 0 };
    sprintf(strChipId, "%u", chipId);
    String chipString = strChipId;
    String name = "FLT" + chipString;
    if (!this->_btSerial->begin(name)) {
        return btErrorCode::INIT_FAILED;
    }

#ifdef DEBUG
    Serial.print(F("bluetooth name: "));
    Serial.println(name);
#endif
    this->_connected = true;
    return btErrorCode::OK;
}

void BtComm::reg() {
    
}

void BtComm::lap(unsigned long lapTime, unsigned int rssi) {
    JsonObject& root = this->prepareJson();
    root["type"] = "lap";
    root["lapTime"] = lapTime;
    root["rssi"] = rssi;
    this->sendJson(root);
}

void BtComm::sendBtMessage(String msg) {
    sendBtMessage(msg, false);
}

void BtComm::sendBtMessageWithNewline(String msg) {
    sendBtMessage(msg, true);
}
void BtComm::sendBtMessage(String msg, boolean newLine) {
    if (newLine) {
        msg += '\n';
    }
#ifdef DEBUG
    Serial.print(F("sendBtMessage(): "));
    Serial.println(msg);
#endif
    this->_btSerial->print(msg.c_str());
}

/*---------------------------------------------------
 * event handler for incoming serial events (hc-06)
 *-------------------------------------------------*/
void BtComm::processIncomingMessage() {
    // process serial data
    while (this->_btSerial->available()) {
        char c = (char)this->_btSerial->read();
        this->_serialString += c;
        if (c == '\n') {
            this->_serialGotLine = true;
        }
    }

    // process serial line
    if (this->_serialGotLine) {
#ifdef DEBUG
        Serial.printf("processIncommingMessage(): %s\n", this->_serialString.c_str());
#endif
        if (this->_serialString.substring(0, 1) == "0") {
            // sometimes on first connect there i a leading zero sent
            this->_serialString = this->_serialString.substring(1, this->_serialString.length() - 1);
        }

        if (this->_serialString.length() >= 11 && this->_serialString.substring(0, 11) == "GET version") {
            // get the version
            JsonObject& root = this->prepareJson();
            root["type"] = "version";
            root["version"] = this->_version;
            this->sendJson(root);
        } else if (this->_serialString.length() >= 8 && this->_serialString.substring(0, 8) == "GET rssi") {
            // get the current rssi
            this->sendFastRssiData(this->_rssi->getRssi());
        } else if (this->_serialString.length() >= 6 && this->_serialString.substring(0, 6) == "REBOOT") {
            // reboot the device
#ifdef DEBUG
            Serial.println(F("reboot device"));
            Serial.flush();
#endif
            this->disconnect();
            if (this->_wifiComm->isConnected()) {
                this->_wifiComm->disconnect();
            }
            ESP.restart();
        } else if (this->_serialString.length() >= 10 && this->_serialString.substring(0, 10) == "GET device") {
            // get the current config data
            this->processGetDeviceData();
        } else if (this->_serialString.length() >= 11 && this->_serialString.substring(0, 11) == "PUT config ") {
            // store the given config data
            this->processStoreConfig();
        } else if (this->_serialString.length() >= 10 && this->_serialString.substring(0, 10) == "START scan") {
            // start channel scan
            this->_rx5808->startScan(this->_storage->getChannelIndex());
            this->_stateManager->update(statemanagement::state_enum::SCAN);
            this->sendGenericState("scan", "started");
        } else if (this->_serialString.length() >= 9 && this->_serialString.substring(0, 9) == "STOP scan") {
            // stop channel scan
            this->_rx5808->stopScan();
            this->_stateManager->update(statemanagement::state_enum::RESTORE_STATE);
            this->sendGenericState("scan", "stopped");
        } else if (this->_serialString.length() >= 10 && this->_serialString.substring(0, 10) == "START rssi") {
            // start fast rssi scan
            this->_stateManager->update(statemanagement::state_enum::RSSI);
            this->sendGenericState("rssi", "started");
        } else if (this->_serialString.length() >= 9 && this->_serialString.substring(0, 9) == "STOP rssi") {
            // stop fast rssi scan
            this->_stateManager->update(statemanagement::state_enum::RESTORE_STATE);
            this->sendGenericState("rssi", "stopped");
        } else {
            String cmd = F("UNKNOWN_COMMAND: ");
            cmd += this->_serialString;
            this->sendBtMessageWithNewline(cmd);
        }
        this->_serialGotLine = false;
        this->_serialString = "";
    }
}

/*---------------------------------------------------
 * received get device data message
 *-------------------------------------------------*/
void BtComm::processGetDeviceData() {
    JsonObject& root = this->prepareJson();
    root["type"] = "device";
    root["frequency"] = freq::Frequency::getFrequencyForChannelIndex(this->_storage->getChannelIndex());
    root["minimumLapTime"] = this->_storage->getMinLapTime();
    root["ssid"] = this->_storage->getSsid();
    root["password"] = this->_storage->getWifiPassword();
    root["triggerThreshold"] = this->_storage->getTriggerThreshold();
    root["triggerThresholdCalibration"] = this->_storage->getTriggerThresholdCalibration();
    root["calibrationOffset"] = this->_storage->getCalibrationOffset();
    root["state"] = this->_stateManager->toString(this->_stateManager->getState());
    root["triggerValue"] = this->_lapDetector->getTriggerValue();
    root["voltage"] = this->_batteryMgr->getVoltage();
    root["uptime"] = millis() / 1000;
    root["defaultVref"] = this->_storage->getDefaultVref();
    root["wifiState"] = this->_wifiComm->isConnected();
    root["rssi"] = this->_rssi->getRssi();
    root["loopTime"] = *this->_loopTime;
    root["filterRatio"] = this->_storage->getFilterRatio();
    root["filterRatioCalibration"] = this->_storage->getFilterRatioCalibration();
    this->sendJson(root);
}

/*---------------------------------------------------
 * received store config message
 *-------------------------------------------------*/
void BtComm::processStoreConfig() {
    // received config
    this->_jsonBuffer.clear();
    JsonObject& root = this->_jsonBuffer.parseObject(this->_serialString.substring(11));
    if (!root.success()) {
#ifdef DEBUG
        Serial.println(F("failed to parse config"));
#endif
        this->sendBtMessageWithNewline(F("SETCONFIG: NOK"));
    } else {
#ifdef DEBUG
        Serial.printf("got config: %s\n", this->_serialString.substring(11).c_str());
#endif
        bool reboot = false;
        this->_storage->setMinLapTime(root["minimumLapTime"]);
        
        if (this->_storage->getSsid() != root["ssid"]) {
            reboot = true;
        }
        this->_storage->setSsid(root["ssid"]);

        if (this->_storage->getWifiPassword() != root["password"]) {
            reboot = true;
        }
        this->_storage->setWifiPassword(root["password"]);

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
        
        String response = F("SETCONFIG: OK");
        if (reboot) {
            response += " reboot";
        }
        this->sendBtMessageWithNewline(response);
    }
}

void BtComm::sendScanData(unsigned int frequency, unsigned int rssi) {
    JsonObject& root = this->prepareJson();
    root["type"] = "scan";
    root["frequency"] = frequency;
    root["rssi"] = rssi;
    this->sendJson(root);
}

void BtComm::sendFastRssiData(unsigned int rssi) {
    JsonObject& root = this->prepareJson();
    root["type"] = "rssi";
    root["rssi"] = rssi;
    this->sendJson(root);
}

void BtComm::sendGenericState(const char* type, const char* state) {
    JsonObject& root = this->prepareJson();
    root["type"] = "state";
    root[type] = state;
    this->sendJson(root);
}

void BtComm::sendCalibrationDone() {
    this->sendGenericState("calibration", "done");
}

JsonObject& BtComm::prepareJson() {
    this->_jsonBuffer.clear();
    JsonObject& root = this->_jsonBuffer.createObject();
    return root;
}

void BtComm::sendVoltageAlarm() {
    JsonObject& root = this->prepareJson();
    root["type"] = "alarm";
    root["msg"] = "Battery voltage low!";
    this->sendJson(root);
}

void BtComm::sendJson(JsonObject& root) {
    String result("");
    root.printTo(result);
    this->sendBtMessageWithNewline(result);
}

bool BtComm::hasClient() {
    return this->_btSerial->hasClient();
}

void BtComm::disconnect() {
    this->_connected = false;
    this->_btSerial->end();
}