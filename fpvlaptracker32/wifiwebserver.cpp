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
        this->sendJson(root);
    });

    this->_server.on("/devicedata", HTTP_POST, [&]() {
        this->_server.sendHeader("Connection", "close");
        // this->_server.send(200, "text/html", temp);
    }, [&]() {
        
    });

//       server.on("/edit", HTTP_POST, []() {
//     server.send(200, "text/plain", "");
//   }, handleFileUpload);

// void handleFileUpload() {
//   if (server.uri() != "/edit") {
//     return;
//   }
//   HTTPUpload& upload = server.upload();
//   if (upload.status == UPLOAD_FILE_START) {
//     String filename = upload.filename;
//     if (!filename.startsWith("/")) {
//       filename = "/" + filename;
//     }
//     DBG_OUTPUT_PORT.print("handleFileUpload Name: "); DBG_OUTPUT_PORT.println(filename);
//     fsUploadFile = SPIFFS.open(filename, "w");
//     filename = String();
//   } else if (upload.status == UPLOAD_FILE_WRITE) {
//     //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
//     if (fsUploadFile) {
//       fsUploadFile.write(upload.buf, upload.currentSize);
//     }
//   } else if (upload.status == UPLOAD_FILE_END) {
//     if (fsUploadFile) {
//       fsUploadFile.close();
//     }
//     DBG_OUTPUT_PORT.print("handleFileUpload Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
//   }
// }

    this->_server.begin();
    this->_connected = true;
}

void WifiWebServer::handle() {
    this->_server.handleClient();
}