#ifndef LIGHTINGPROCESSOR_H
#define LIGHTINGPROCESSOR_H

#include <Arduino.h>
#include <M5StickCPlus.h>
#include <FastLED.h>

class LightingProcessor
{
private:
    void defaultSoundFx();
    void twoToneSoundFx();
    void sparkleFx();
    void constantFx(uint8_t val, uint8_t saturation, uint8_t brightness);
    void setHSV(u_int8_t index, uint8_t H, uint8_t S, uint8_t V);

public:
    LightingProcessor();

    void setupLedStrip();
    void loop();
    void updateLedStrip(int lightness[], bool isBeatHit, String modifier);
};

#endif