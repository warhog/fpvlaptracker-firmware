#include "wificomm.h"

using namespace comm;

//#define DEBUG
const unsigned int UDP_PORT = 31337;

WifiComm::WifiComm(util::Storage *storage, battery::BatteryMgr *batteryMgr, const char *version,
    unsigned long *loopTime, ledio::Ws2812 *ws2812) : _version(version), _wifiSsidFound(false), _loopTime(loopTime),
    _connected(false), _ws2812(ws2812), _storage(storage) {
}

int WifiComm::connect() {
    this->_wifiSsidFound = false;
	this->_connected = false;
	int nrOfWifis = WiFi.scanNetworks(false, false);
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
            while (WiFi.status() != WL_CONNECTED && wait < 10000) {
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
        } else if (strncmp(incomingPacket, "led", 3) == 0) {
#ifdef DEBUG
            Serial.println(F("got led packet"));
#endif
            String request = incomingPacket;
            request = request.substring(4);
            processRequest(request);
        } else if (strncmp(incomingPacket, "requestData", 11) == 0) {
#ifdef DEBUG
            Serial.println(F("got request data packet"));
#endif
            this->sendData();
        }
    }
}

void WifiComm::processRequest(String request) {
    String mode = "";
    String color = "";
    uint16_t interval = 0;

    mode = this->split(request, ' ', 0);
    color = this->split(request, ' ', 1);

    if (mode == "blink" || mode == "countdown" || mode == "right" || mode == "left" || mode == "expand") {
        interval = this->split(request, ' ', 2).toInt();
        if (interval == 0) {
            interval = 500;
        }
    }

    RgbColor colorRgb = this->convertColor(color);

    if (mode == "blink") {
        this->_ws2812->blinkColor(colorRgb, interval);
    } else if (mode == "static") {
        this->_ws2812->staticColor(colorRgb);
    } else if (mode == "countdown") {
        this->_ws2812->clearAnimation();
        this->_ws2812->addAnimationEntry(new ledio::ColorEntry(colorRgb, interval, false));
        RgbColor black(0, 0, 0);
        this->_ws2812->addAnimationEntry(new ledio::ColorEntry(black, 1, true));
    } else if (mode == "right") {
        this->_ws2812->right(colorRgb, interval);
    } else if (mode == "left") {
        this->_ws2812->left(colorRgb, interval);
    } else if (mode == "expand") {
        this->_ws2812->expand(colorRgb, interval);
    }

    Serial.printf("mode %s, color %s, interval %d\n", mode.c_str(), color.c_str(), interval);

}

RgbColor WifiComm::convertColor(String color) {
    RgbColor colorRgb(0, 0, 0);
    if (color == "red") {
        colorRgb.R = 255;
    } else if (color == "green") {
        colorRgb.G = 255;
    } else if (color == "blue") {
        colorRgb.B = 255;
    } else if (color == "white") {
        colorRgb.R = 255;
        colorRgb.G = 255;
        colorRgb.B = 255;
    } else if (color == "yellow") {
        colorRgb.R = 255;
        colorRgb.G = 255;
    } else if (color == "cyan") {
        colorRgb.G = 255;
        colorRgb.B = 255;
    } else if (color == "magenta") {
        colorRgb.R = 255;
        colorRgb.B = 255;
    } else if (color == "purple") {
        colorRgb.R = 64;
        colorRgb.B = 128;
    } else if (color == "orange") {
        colorRgb.R = 255;
        colorRgb.G = 48;
    } else if (color == "pink") {
        colorRgb.R = 255;
        colorRgb.G = 20;
        colorRgb.B = 147;
    }
    return colorRgb;
}

String WifiComm::split(String s, char delimiter, int index) {
	int count = 0;
	int fromIndex = 0;
	int toIndex = -1;
	while (true) {
		fromIndex = toIndex + 1;
		toIndex = s.indexOf(delimiter, fromIndex);
		if (count == index) {
			if (toIndex == -1) {
				// have reached our index and not found a next delimiter
				// this means the end of the string is reached
				toIndex = s.length();
			}
			return s.substring(fromIndex, toIndex);
		}
		if (toIndex == -1) {
			// char not found
			break;
		} else {
			count++;
		}
	}
	return "";
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

void WifiComm::sendVoltageAlarm() {
#ifdef DEBUG 
    Serial.println(F("sending voltage alarm message"));
#endif
    String msg = "{\"type\":\"battery_low\",\"chipid\":";
    msg += static_cast<unsigned long>(ESP.getChipId());
    msg += "}";
    this->sendUdpMessage(msg);    
}

void WifiComm::reg() {
#ifdef DEBUG 
    Serial.println(F("sending register message"));
#endif
    String msg = "{\"type\":\"registerled\",\"chipid\":";
    msg += static_cast<unsigned long>(ESP.getChipId());
    msg += ",\"ip\":";
    msg += WiFi.localIP();
    msg += "}";
    this->sendUdpMessage(msg);    
}

void WifiComm::sendData() {
#ifdef DEBUG 
    Serial.println(F("sending data message"));
#endif
    unsigned long chipId = static_cast<unsigned long>(ESP.getChipId());
    DynamicJsonBuffer _jsonBuffer(400);
    JsonObject& root = _jsonBuffer.createObject();
    root["type"] = "leddevicedata";
    root["chipid"] = chipId;
    root["voltage"] = this->_batteryMgr->getVoltage();
    root["uptime"] = millis() / 1000;
    root["defaultVref"] = this->_storage->getDefaultVref();
    root["loopTime"] = *this->_loopTime;
    String msg("");
    root.printTo(msg);
    _jsonBuffer.clear();
    this->sendUdpMessage(msg);
}

void WifiComm::disconnect() {
    this->_connected = false;
    WiFi.disconnect();
}