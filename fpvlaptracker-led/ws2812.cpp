#include "ws2812.h"

using namespace ledio;

#define DEBUG

Ws2812::Ws2812(NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> *neopixels) : _index(0), _state(0), _neopixels(neopixels) {
    this->clearAnimation();
}

void Ws2812::init() {
    this->_neopixels->Begin();
    this->_neopixels->Show();

    RgbColor red(255, 0, 0);
    RgbColor green(0, 255, 0);
    RgbColor blue(0, 0, 255);
    RgbColor white(255, 255, 255);
    RgbColor black(0, 0, 0);

    for (uint8_t i = 0; i < 32 / 4; i++) {
        this->_neopixels->SetPixelColor(i * 4, red);
        this->_neopixels->SetPixelColor(i * 4 + 1, green);
        this->_neopixels->SetPixelColor(i * 4 + 2, blue);
        this->_neopixels->SetPixelColor(i * 4 + 3, white);
    }
    this->_neopixels->Show();
    for (uint8_t i = 0; i < 32; i++) {
        this->_neopixels->SetPixelColor(i, black);
        this->_neopixels->Show();
        delay(10);
    }
}

void Ws2812::clearAnimation() {

    RgbColor black(0, 0, 0);
    this->_neopixels->ClearTo(black);
    this->_neopixels->Show();
    this->_index = 0;
    this->_state = 0;
    // free memory
    while (this->_animations.size() > 0) {
        AnimationEntry *ae = this->_animations.pop();
        delete ae;
    }
    // should be empty already but who knows
    this->_animations.clear();
}

void Ws2812::staticColor(RgbColor color) {
    this->clearAnimation();
    this->addAnimationEntry(new ColorEntry(color, 0, true));
}

void Ws2812::blinkColor(RgbColor color, uint16_t interval) {
    this->clearAnimation();
    this->addAnimationEntry(new ColorEntry(color, interval, false));
    RgbColor black(0, 0, 0);
    this->addAnimationEntry(new ColorEntry(black, interval, false));
}

void Ws2812::addAnimationEntry(AnimationEntry *ae) {
    this->_animations.add(ae);
}

void Ws2812::right(RgbColor color, uint16_t interval) {
    this->clearAnimation();
    for (uint8_t i = 0; i < 7; i++) {
        PixelEntry *entry = new PixelEntry(32, interval, false);
        entry->setPixel(i + 0, 0, color);
        entry->setPixel(i + 1, 1, color);
        entry->setPixel(i + 1, 2, color);
        entry->setPixel(i + 0, 3, color);
        this->addAnimationEntry(entry);
    }
}

void Ws2812::left(RgbColor color, uint16_t interval) {
    this->clearAnimation();
    for (uint8_t i = 7; i > 0; i--) {
        PixelEntry *entry = new PixelEntry(32, interval, false);
        entry->setPixel(i - 0, 0, color);
        entry->setPixel(i - 1, 1, color);
        entry->setPixel(i - 1, 2, color);
        entry->setPixel(i - 0, 3, color);
        this->addAnimationEntry(entry);
    }
}

void Ws2812::expand(RgbColor color, uint16_t interval) {
    this->clearAnimation();
    for (uint8_t i = 0; i < 3; i++) {
        PixelEntry *entry = new PixelEntry(32, interval, false);
        entry->setPixel(3 - i - 0, 0, color);
        entry->setPixel(3 - i - 1, 1, color);
        entry->setPixel(3 - i - 1, 2, color);
        entry->setPixel(3 - i - 0, 3, color);

        entry->setPixel(i + 3 + 1, 0, color);
        entry->setPixel(i + 3 + 2, 1, color);
        entry->setPixel(i + 3 + 2, 2, color);
        entry->setPixel(i + 3 + 1, 3, color);
        this->addAnimationEntry(entry);
    }
}

void Ws2812::run() {
    if (this->_state == 0) {
        // load new data
        if (this->_animations.size() > 0) {
            this->_currentAnimationEntry = this->_animations.get(this->_index);
            this->_state = 1;
            this->_waitStart = millis();

            if (this->_currentAnimationEntry->getType() == 0) {
                // animationentry
                this->_neopixels->ClearTo(this->_currentAnimationEntry->getColor());
#ifdef DEBUG
                Serial.print(F("output new data: "));
                Serial.printf("color: %u ", this->_currentAnimationEntry->getColor());
                Serial.println();
#endif
            } else if (this->_currentAnimationEntry->getType() == 1) {
                // pixelentry
#ifdef DEBUG
                Serial.println(F("output new pixel data:"));
                for (uint8_t y = 0; y < 4; y++) {
                    for (uint8_t x = 0; x < 8; x++) {
                        uint8_t i = y * 8 + x;
                        if (this->_currentAnimationEntry->getPixel(i).R == 0 && this->_currentAnimationEntry->getPixel(i).G == 0 && this->_currentAnimationEntry->getPixel(i).B == 0) {
                            Serial.print(" ");
                        } else {
                            Serial.print("X");
                        }
                    }
                    Serial.println("");
                }
#endif
                for (uint8_t i = 0; i < 32; i++) {
                    this->_neopixels->SetPixelColor(i, this->_currentAnimationEntry->getPixel(i));
                }
            }
            this->_neopixels->Show();
            this->_index++;
            if (this->_index >= this->_animations.size()) {
                this->_index = 0;
            }
        }
    } else if (this->_state == 1) {
        // wait
        if ((millis() - this->_waitStart) >= this->_currentAnimationEntry->getTime() && this->_currentAnimationEntry->getStop() == false) {
            // waiting is over
            this->_state = 0;
        }
    }
}
