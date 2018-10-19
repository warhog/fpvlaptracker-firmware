#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>

#include "storage.h"

class WebUpdate {
public:
    WebUpdate(String version, util::Storage *storage) : _server(80), _version(version), _storage(storage) {};
    WebUpdate(util::Storage *storage) : _server(80), _version("0") {};
    void begin();
    void run();
    void setVersion(String version) {
        this->_version = version;
    }
private:
    char *_serverIndex = "<style>body { font-family: Arial; }</style><h1>fpvlaptracker webupdate</h1>chip id: %CHIPID%<br />current version: %VERSION%<br />build date: " __DATE__ "  " __TIME__ "<br /><br />select .bin file to flash.<br /><br /><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='update'></form><br /><br /><a href='/factorydefaults'>load factory defaults</a>";
    char *_successResponse = "update successful! rebooting...";
    char *_failedResponse = "update failed!\n";
    WebServer _server;
    String _version;
    util::Storage *_storage;

};