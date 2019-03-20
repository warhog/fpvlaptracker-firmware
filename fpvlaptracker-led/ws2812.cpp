#include "ws2812.h"

using namespace ledio;

Ws2812::Ws2812(WS2812FX *ws2812fx) : _ws2812fx(ws2812fx), _index(0), _state(0) {
    this->clearAnimation();
}

void Ws2812::clearAnimation() {

    this->_ws2812fx->setBrightness(BRIGHTNESS_MIN);
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

void Ws2812::staticColor(uint32_t color) {
    this->clearAnimation();
    this->addAnimationEntry(new AnimationEntry(FX_MODE_STATIC, BRIGHTNESS_MAX, SPEED_MAX, color, 0, true));
}

void Ws2812::blinkColor(uint32_t color, uint16_t interval) {
    this->clearAnimation();
    this->addAnimationEntry(new AnimationEntry(FX_MODE_STATIC, BRIGHTNESS_MAX, SPEED_MAX, color, interval, false));
    this->addAnimationEntry(new AnimationEntry(FX_MODE_STATIC, BRIGHTNESS_MIN, SPEED_MAX, BLACK, interval, false));
}

void Ws2812::addAnimationEntry(AnimationEntry *ae) {
    this->_animations.add(ae);
}

void Ws2812::run() {
    if (this->_state == 0) {
        // load new data
        if (this->_animations.size() > 0) {
            this->_currentAnimationEntry = this->_animations.get(this->_index);
            this->_state = 1;
            this->_waitStart = millis();
            this->_ws2812fx->setMode(this->_currentAnimationEntry->getMode());
            this->_ws2812fx->setBrightness(this->_currentAnimationEntry->getBrightness());
            this->_ws2812fx->setSpeed(this->_currentAnimationEntry->getSpeed());
            this->_ws2812fx->setColor(this->_currentAnimationEntry->getColor());
#ifdef DEBUG
            Serial.print("output new data: ");
            Serial.printf("mode: %d ", this->_currentAnimationEntry->getMode());
            Serial.printf("brightness: %d ", this->_currentAnimationEntry->getBrightness());
            Serial.printf("speed: %d ", this->_currentAnimationEntry->getSpeed());
            Serial.printf("color: %u ", this->_currentAnimationEntry->getColor());
            Serial.println();
#endif
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
