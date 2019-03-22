#pragma once
#include <Arduino.h>
#include <NeoPixelBus.h>

namespace ledio {

    class AnimationEntry {
        public:
            AnimationEntry() : _time(0), _stop(false) { }
            AnimationEntry(uint16_t time, bool stop) : _time(time), _stop(stop) { }
                
            virtual uint8_t getType() = 0;
            virtual RgbColor getColor() = 0;
            virtual RgbColor getPixel(uint8_t pixel) = 0;

            void setTime(uint16_t time) {
                this->_time = time;
            }
            void setStop(bool stop) {
                this->_stop = stop;
            }
            uint16_t getTime() {
                return this->_time;
            }
            bool getStop() {
                return this->_stop;
            }

        private:
            uint16_t _time;
            bool _stop;
    };

}