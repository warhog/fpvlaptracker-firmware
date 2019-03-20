#pragma once
#include <Arduino.h>

namespace ledio {

    class AnimationEntry {
        public:
            AnimationEntry() : _mode(0), _brightness(0), _speed(0), _color(0), _time(0), _stop(false) { }
            AnimationEntry(uint8_t mode, uint8_t brightness, uint16_t speed, uint32_t color, uint16_t time, bool stop) :
                _mode(mode), _brightness(brightness), _speed(speed), _color(color), _time(time), _stop(stop) { }
                
            void setMode(uint8_t mode) {
                this->_mode = mode;
            }
            void setSpeed(uint16_t speed) {
                this->_speed = speed;
            }
            void setColor(uint32_t color) {
                this->_color = color;
            }
            void setTime(uint16_t time) {
                this->_time = time;
            }
            void setBrightness(uint8_t brightness) {
                this->_brightness = brightness;
            }
            void setStop(bool stop) {
                this->_stop = stop;
            }
            uint8_t getMode() {
                return this->_mode;
            }
            uint16_t getSpeed() {
                return this->_speed;
            }
            uint32_t getColor() {
                return this->_color;
            }
            uint16_t getTime() {
                return this->_time;
            }
            uint8_t getBrightness() {
                return this->_brightness;
            }
            bool getStop() {
                return this->_stop;
            }
        private:
            uint8_t _mode;
            uint16_t _speed;
            uint32_t _color;
            uint16_t _time;
            uint8_t _brightness;
            bool _stop;
    };

}