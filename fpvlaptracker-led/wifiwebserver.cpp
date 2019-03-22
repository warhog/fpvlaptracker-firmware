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
        temp.replace("%SSID%", this->_storage->getSsid());
        temp.replace("%PASSWORD%", this->_storage->getWifiPassword());
        uint64_t chipId = ESP.getChipId();
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

    this->_server.on("/setup", HTTP_POST, [&]() {
        this->_server.sendHeader("Connection", "close");
        if (this->_server.args() > 0) {
            bool reboot = false;
            if (this->_server.arg("ssid") != this->_storage->getSsid()) {
                this->_storage->setSsid(this->_server.arg("ssid"));
                reboot = true;
            }
            if (this->_server.arg("password") != this->_storage->getWifiPassword()) {
                this->_storage->setWifiPassword(this->_server.arg("password"));
                reboot = true;
            }
            this->_storage->store();
            
            String response = F("values saved! ");
            if (reboot) {
                response += "rebooting...";
            }
            response += "<a href='/'>back</a>";
            this->_server.send(200, "text/html", this->concat(response));
            if (reboot) {
                this->_server.client().flush();
                this->_server.client().stop();
                ESP.restart();
            }
        } else {
            this->_server.send(500, "text/html", "no arguments");
        }
    });

    this->_server.on("/update", HTTP_POST, [&]() {
        this->_server.sendHeader("Connection", "close");
        this->_server.send(200, "text/html", this->concat((Update.hasError()) ? "update failed!\n" : "update successful! rebooting...<a href='/'>back</a>"));
        this->_server.client().setNoDelay(true);
        delay(100);
        this->_server.client().flush();
        this->_server.client().stop();
        ESP.restart();
    }, [&]() {
        HTTPUpload& upload = this->_server.upload();
        if (upload.status == UPLOAD_FILE_START) {
#ifdef DEBUG
            Serial.setDebugOutput(true);
            Serial.printf("Update: %s\n", upload.filename.c_str());
#endif
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) { //start with max available size
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
            } else {
                Update.printError(Serial);
#endif
            }
#ifdef DEBUG
            Serial.setDebugOutput(false);
#endif
        }
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
        root["voltage"] = this->_batteryMgr->getVoltage();
        root["uptime"] = millis() / 1000;
        root["defaultVref"] = this->_storage->getDefaultVref();
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
                if (root["defaultVref"] > 999 && root["defaultVref"] < 1200) {
                    if (this->_storage->getDefaultVref() != root["defaultVref"]) {
                        reboot = true;
                    }
                    this->_storage->setDefaultVref(root["defaultVref"]);
                }

                this->_storage->store();
                
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