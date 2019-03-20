#pragma once

#include <Arduino.h>
#include <EEPROM.h>

namespace util {

    class Storage {
    public:
        Storage();
        void load();
        void store();
        void loadFactoryDefaults();
        
        void setDefaultVref(unsigned int defaultVref) {
            this->_defaultVref = defaultVref;
        }
        unsigned int getDefaultVref() {
            return this->_defaultVref;
        }

        void setSsid(String ssid) {
            if (ssid.length() > 64) {
                return;
            }
            this->_ssid = ssid;
        }
        String getSsid() {
            return this->_ssid;
        }

        void setWifiPassword(String password) {
            if (password.length() > 64) {
                return;
            }
            this->_wifiPassword = password;
        }
        String getWifiPassword() {
            return this->_wifiPassword;
        }

    private:
        String _ssid;
        String _wifiPassword;
        unsigned int _defaultVref;

        struct StorageEepromStruct {
            char version[4];
            char ssid[64];
            char wifiPassword[64];
            unsigned int defaultVref;
        };

    };

}