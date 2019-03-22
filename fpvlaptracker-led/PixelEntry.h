#pragma once
#include <Arduino.h>
#include <NeoPixelBus.h>

namespace ledio {

    class PixelEntry : public AnimationEntry {
        public:
            PixelEntry() : AnimationEntry(0, false), _color(nullptr) { }
            PixelEntry(uint8_t size, uint16_t time, bool stop) :
                AnimationEntry(time, stop) {
                    this->_size = size;
                    this->_color = new RgbColor[size];
                    RgbColor black(0, 0, 0);
                    for (uint8_t i = 0; i < 32; i++) {
                        this->_color[i] = black;
                    }
                }

            ~PixelEntry() {
                Serial.println("deconstruct");
                delete _color;
            }

            uint8_t getType() {
                return 1;
            }
            
            void setPixel(uint8_t pixel, RgbColor color) {
                this->_color[pixel] = color;
            }

            void setPixel(uint8_t x, uint8_t y, RgbColor color) {
                this->_color[y * 8 + x] = color;
            }

            RgbColor getPixel(uint8_t pixel) {
                return this->_color[pixel];
            }

            RgbColor getColor() {
                return RgbColor(0, 0, 0);
            }

        private:
            uint8_t _size;
            RgbColor *_color;
    };

}