#pragma once
#include <Arduino.h>
#include <WS2812FX.h>
#include <LinkedList.h>

#include "AnimationEntry.h"

namespace ledio {

    class Ws2812 {
    public:
        Ws2812(WS2812FX *ws2812fx);
        void clearAnimation();
        void addAnimationEntry(AnimationEntry *animationEntry);
        void staticColor(uint32_t color);
        void blinkColor(uint32_t color, uint16_t interval);
        void run();
        
    private:
        WS2812FX *_ws2812fx;
        LinkedList<AnimationEntry*> _animations;
        uint8_t _index;
        AnimationEntry *_currentAnimationEntry;
        uint8_t _state;
        uint32_t _waitStart;
    };

}