#include "wificomm.h"

using namespace comm;

//#define DEBUG
const unsigned int UDP_PORT = 31337;

WifiComm::WifiComm(util::Storage *storage, lap::Rssi *rssi, radio::Rx5808 *rx5808, lap::LapDetector *lapDetector,
    battery::BatteryMgr *batteryMgr, const char *version, statemanagement::StateManager *stateManager,
    unsigned long *loopTime) :
    Comm(storage, rssi, rx5808, lapDetector, batteryMgr, version, stateManager, loopTime), _wifiSsidFound(false) {
        this->_version = version;
}

int WifiComm::connect() {
    this->_wifiSsidFound = false;
	this->_connected = false;

	int nrOfWifis = WiFi.scanNetworks(false, false, false, 1000);
#ifdef DEBUG
	Serial.print(nrOfWifis);
	Serial.println(F(" network(s) found"));
#endif
	for (unsigned int i = 0; i < nrOfWifis; i++) {
#ifdef DEBUG
		Serial.println(WiFi.SSID(i));
#endif
		if (WiFi.SSID(i) == this->_storage->getSsid()) {
			this->_wifiSsidFound = true;
#ifdef DEBUG
			Serial.println(F("found ssid, connecting"));
#endif
		}
	}

	if (this->_wifiSsidFound) {
        unsigned int tries = 5;
        while (tries > 0) {
#ifdef DEBUG
            Serial.print(F("wifi connecting to ssid "));
            Serial.print(this->_storage->getSsid());
#endif
    		WiFi.begin(this->_storage->getSsid().c_str(), this->_storage->getWifiPassword().c_str());
            unsigned int wait = 0;
            while (WiFi.status() != WL_CONNECTED && wait < 5000) {
                delay(500);
                wait += 500;
#ifdef DEBUG
    			Serial.print(F("."));
#endif
            }
            tries--;
            if (WiFi.status() != WL_CONNECTED) {
                WiFi.disconnect();
#ifdef DEBUG
                if (tries == 0) {
        			Serial.println(F("cannot connect to ssid, switching to standalone mode"));
                } else {
                    Serial.println(F("cannot connect to ssid, retrying"));
                }
#endif
		    } else if (WiFi.status() == WL_CONNECTED) {
#ifdef DEBUG
    			Serial.println(F("connected"));
#endif
    			this->_connected = true;
                tries = 0;
            }

        }
        if (this->_connected) {
#ifdef DEBUG
            Serial.println(F("WiFi set up"));
            Serial.print(F("IP address: "));
            Serial.println(WiFi.localIP());
            Serial.print(F("broadcast IP is "));
            Serial.println(getBroadcastIP());
            Serial.println(F("starting udp"));
#endif
            this->_udp.begin(UDP_PORT);
        }
#ifdef DEBUG
	} else {
        Serial.println(F("wifi ssid not found"));
#endif
    }

    return 0;
}

void WifiComm::sendUdpMessage(String msg) {
#ifdef DEBUG
    Serial.print(F("sending udp message: "));
    Serial.println(msg);
#endif
    this->_udp.beginPacket(this->getBroadcastIP().c_str(), UDP_PORT);
    this->_udp.print(msg.c_str());
    this->_udp.endPacket();
}

void WifiComm::lap(unsigned long lapTime, unsigned int rssi) {
#ifdef DEBUG 
    Serial.println(F("sending lap message"));
#endif
    String msg = "{\"type\":\"lap\",\"chipid\":";
    msg += static_cast<unsigned long>(ESP.getEfuseMac());
    msg += ",\"duration\":";
    msg += lapTime;
    msg += ",\"rssi\":";
    msg += rssi;
    msg += "}";
    this->sendUdpMessage(msg);
}

void WifiComm::processIncomingMessage() {
    int packetSize = this->_udp.parsePacket();
    if (packetSize) {
        char incomingPacket[255];
        int len = this->_udp.read(incomingPacket, 255);
        if (len > 0) {
            incomingPacket[len] = 0;
        }
#ifdef DEBUG
        Serial.printf("incoming packet: %s\n", incomingPacket);
#endif
        if (strncmp(incomingPacket, "requestRegistration", 19) == 0) {
#ifdef DEBUG
            Serial.println(F("got request registration packet"));
#endif
            this->reg();
        } else if (strncmp(incomingPacket, "requestData", 11) == 0) {
#ifdef DEBUG
            Serial.println(F("got request data packet"));
#endif
            this->sendData();
        }
    }
}

void WifiComm::reg() {
#ifdef DEBUG 
    Serial.println(F("sending register message"));
#endif
    String msg = "{\"type\":\"register32\",\"chipid\":";
    msg += static_cast<unsigned long>(ESP.getEfuseMac());
    msg += ",\"ip\":";
    msg += WiFi.localIP();
    msg += "}";
    this->sendUdpMessage(msg);    
}

/*---------------------------------------------------
 * build broadcast ip for current localIP
 *-------------------------------------------------*/
String WifiComm::getBroadcastIP() {
    uint32_t ip = (uint32_t)WiFi.localIP();
    uint32_t netmask = (uint32_t)WiFi.subnetMask();
    uint32_t broadcast = ip | (~netmask);
    IPAddress broadcastAddress(broadcast);
    return broadcastAddress.toString();
}

void WifiComm::sendCalibrationDone() {
#ifdef DEBUG 
    Serial.println(F("sending calibration done message"));
#endif
    String msg = "{\"type\":\"calibrationdone\",\"chipid\":";
    msg += static_cast<unsigned long>(ESP.getEfuseMac());
    msg += "}";
    this->sendUdpMessage(msg);    
}

void WifiComm::sendData() {
#ifdef DEBUG 
    Serial.println(F("sending data message"));
#endif
    unsigned long chipId = static_cast<unsigned long>(ESP.getEfuseMac());
    DynamicJsonBuffer _jsonBuffer(400);
    JsonObject& root = _jsonBuffer.createObject();
    root["type"] = "devicedata";
    root["chipid"] = chipId;
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
    root["loopTime"] = *this->_loopTime;
    root["filterRatio"] = this->_storage->getFilterRatio();
    root["filterRatioCalibration"] = this->_storage->getFilterRatioCalibration();
    String msg("");
    root.printTo(msg);
    _jsonBuffer.clear();
    this->sendUdpMessage(msg);
}

void WifiComm::disconnect() {
    WiFi.disconnect();
}