#pragma once
#include <Arduino.h>
#include <NeoPixelBus.h>

namespace ledio {

    class ColorEntry : public AnimationEntry {
        public:
            ColorEntry() : AnimationEntry(0, false), _color(0) { }
            ColorEntry(RgbColor color, uint16_t time, bool stop) :
                AnimationEntry(time, stop), _color(color) { }
                
            uint8_t getType() {
                return 0;
            }

            void setColor(RgbColor color) {
                this->_color = color;
            }
            
            RgbColor getColor() {
                return this->_color;
            }

            RgbColor getPixel(uint8_t pixel) {
                return RgbColor(0, 0, 0);
            }
            
        private:
            RgbColor _color;
    };

}