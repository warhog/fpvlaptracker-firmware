#pragma once
#include <Arduino.h>
#include <LinkedList.h>
#include <NeoPixelBus.h>

#include "AnimationEntry.h"
#include "PixelEntry.h"
#include "ColorEntry.h"

namespace ledio {

    class Ws2812 {
    public:
        Ws2812(NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> *neopixels);
        void init();
        void clearAnimation();
        void addAnimationEntry(AnimationEntry *animationEntry);
        void staticColor(RgbColor color);
        void blinkColor(RgbColor color, uint16_t interval);
        void run();
        void right(RgbColor color, uint16_t interval);
        void left(RgbColor color, uint16_t interval);
        void expand(RgbColor color, uint16_t interval);

    private:
        NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> *_neopixels;
        LinkedList<AnimationEntry*> _animations;
        uint8_t _index;
        AnimationEntry *_currentAnimationEntry;
        uint8_t _state;
        uint32_t _waitStart;
    };

}