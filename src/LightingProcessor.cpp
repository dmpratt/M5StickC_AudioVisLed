#include "LightingProcessor.h"

/* ----- From FFTProcessor ----- */
const uint8_t kFreqBandCount = 20;

/* ----- Fastled constants ----- */
const uint8_t kPinLedStrip = 26; // M5StickC grove port, yellow cable
const uint8_t kNumLeds = 139;
const uint8_t kLedStripBrightness = 255;
const uint32_t kMaxMilliamps = 2500;

/* ----- Fastled variables ----- */
// LED strip controller
CRGB ledStrip_[kNumLeds];
uint8_t ledStripH[kNumLeds];
uint8_t ledStripS[kNumLeds];
uint8_t ledStripV[kNumLeds];

uint8_t kBassHue = 250;
uint8_t beatVisIntensity_ = 0;
int *freqBinLightness;
String mode = "default";

/* ------ Sparkle Vars ------ */
uint8_t sparkle_delay[3];
uint8_t sparkle_step[3];
uint8_t sparkle_led[3];
uint8_t sparkle_hue[3];
uint8_t sparkle_saturation[3];
uint8_t sparkle_brightness[3];

const uint8_t numFreqLeds = floor(kNumLeds / 2 / (kFreqBandCount + 2));                     // 100 / 2 / 22 = 2
const uint8_t numBassLeds = floor(kNumLeds / 2) - kFreqBandCount * numFreqLeds;             // 50 - 40 = 10
const uint8_t numExtraLeds = kNumLeds - ((kFreqBandCount * numFreqLeds + numBassLeds) * 2); // 100 - 100 but could be an odd number.

uint8_t userTriggerB_ = 0;

LightingProcessor::LightingProcessor()
{
    // Constructor
}

void LightingProcessor::setupLedStrip()
{
    delay(500);
    FastLED.addLeds<NEOPIXEL, kPinLedStrip>(ledStrip_, kNumLeds);
    FastLED.clear();
    FastLED.setBrightness(kLedStripBrightness);
    FastLED.setMaxPowerInVoltsAndMilliamps(12, kMaxMilliamps); // Set maximum power consumption to 5 V and 2.5 A
    ledStrip_[0].setHSV(60, 255, 255);
    FastLED.show();

    Serial.printf("Total leds: %i, %i for each band and %i for bass. There are %i extras to be distributed.",
                  kNumLeds, numFreqLeds, numBassLeds, numExtraLeds);
}

void LightingProcessor::updateLedStrip(int lightness[], bool isBeatHit, String modifier)
{
    // Update class frequency bin lightness values
    freqBinLightness = lightness;

    // Detect magnitude peak
    beatVisIntensity_ = (isBeatHit) ? 250 : (beatVisIntensity_ > 0) ? beatVisIntensity_ -= 25
                                                                    : 0;

    // Update mode if new mode signal received
    if (!modifier.isEmpty())
    {
        mode = modifier;
        mode.toLowerCase();
    }

    if (mode == "default" || mode == "sparkle")
    {
        kBassHue++;
        mainSoundFx();
    }
    else if (mode.indexOf('-') >= 0)
    {
        String key = mode.substring(0, mode.indexOf('-'));
        key.trim();
        String value = mode.substring(mode.indexOf('-') + 1);
        value.trim();
        if (key == "solid")
        {
            constantFx(value);
        }
    }

    if (mode == "sparkle")
    {
        sparkleFx();
    }

    FastLED.show();
}

void LightingProcessor::mainSoundFx()
{
    uint8_t ledIndex = 0;

    // Show beat detection at the beginning of the strip
    for (int i = 0; i < numBassLeds; i++)
    {
        setHSV(ledIndex++, kBassHue, 255, beatVisIntensity_);
    }

    // Show frequency intensities on the remaining Leds
    const uint8_t colorStart = 30;
    const uint8_t colorEnd = 210;
    const uint8_t colorStep = 3; //(colorEnd - colorStart) / (kNumLeds - numBassLeds * 2) / 2;
    uint8_t color = colorStart;

    // First half
    for (int k = 0; k < kFreqBandCount; k++)
    {
        for (int j = 0; j < numFreqLeds; j++)
        {
            setHSV(ledIndex++, color, 255, freqBinLightness[k]);
            color += colorStep;
        }

        // If extra leds are odd, give extra 1 to the last band aka the center band.
        if (k == kFreqBandCount - 1)
        {
            if (numExtraLeds % 2 == 1)
            {
                setHSV(ledIndex++, color, 255, freqBinLightness[k]);
                color += colorStep;
            }
        }
    }

    // Second half
    for (int k = kFreqBandCount - 1; k >= 0; k--)
    {
        for (int j = 0; j < numFreqLeds; j++)
        {
            setHSV(ledIndex++, color, 255, freqBinLightness[k]);
            color -= colorStep;
        }
    }

    // Show beat detection at the end of the strip
    for (int i = 0; i < numBassLeds; i++)
    {
        setHSV(ledIndex++, kBassHue, 255, beatVisIntensity_);
    }
}

void LightingProcessor::sparkleFx()
{
    for (int x = 0; x < sizeof(sparkle_led); x++)
    {
        if (sparkle_delay[x] == 0 && sparkle_step[x] == 0)
        {
            srand(time(NULL));
            uint8_t randStart = rand() % 75;
            sparkle_delay[x] = randStart + 25;
            sparkle_led[x] = rand() % (kNumLeds - 2) + 1;
        }
        else if (sparkle_delay[x] > 0)
        {
            if (--sparkle_delay[x] == 0)
            {
                sparkle_step[x] = 1;
            }
        }

        switch (sparkle_step[x])
        {
        case 0:
            return;
        case 1:
            sparkle_brightness[x] = ledStripV[sparkle_led[x]];
            sparkle_saturation[x] = 200;
            sparkle_hue[x] = ledStripH[sparkle_led[x]];
            ledStrip_[sparkle_led[x]].setHSV(sparkle_hue[x], sparkle_saturation[x], sparkle_brightness[x]);
            sparkle_step[x]++;
            break;
        case 2:
            sparkle_brightness[x] > 205 ? sparkle_brightness[x] = 255 : sparkle_brightness[x] += 50;
            sparkle_saturation[x] -= 50;
            ledStrip_[sparkle_led[x]-1].setHSV(sparkle_hue[x], sparkle_saturation[x]+100, sparkle_brightness[x]);
            ledStrip_[sparkle_led[x]].setHSV(sparkle_hue[x], sparkle_saturation[x], sparkle_brightness[x]);
            ledStrip_[sparkle_led[x]+1].setHSV(sparkle_hue[x], sparkle_saturation[x]+100, sparkle_brightness[x]);
            if (sparkle_saturation[x] == 0)
                sparkle_step[x]++;
            break;
        case 12:
            sparkle_brightness[x] -= 20;
            sparkle_saturation[x] += 5;
            ledStrip_[sparkle_led[x]-1].setHSV(sparkle_hue[x], sparkle_saturation[x]+100, sparkle_brightness[x]);
            ledStrip_[sparkle_led[x]].setHSV(sparkle_hue[x], sparkle_saturation[x], sparkle_brightness[x]);
            ledStrip_[sparkle_led[x]+1].setHSV(sparkle_hue[x], sparkle_saturation[x]+100, sparkle_brightness[x]);
            if (sparkle_brightness[x] < 50)
                sparkle_step[x] = 0;
            break;
        default:
            ledStrip_[sparkle_led[x]-1].setHSV(sparkle_hue[x], sparkle_saturation[x]+100, sparkle_brightness[x]);
            ledStrip_[sparkle_led[x]].setHSV(sparkle_hue[x], sparkle_saturation[x], sparkle_brightness[x]);
            ledStrip_[sparkle_led[x]+1].setHSV(sparkle_hue[x], sparkle_saturation[x]+100, sparkle_brightness[x]);
            sparkle_step[x]++;
        }
    }
}

void LightingProcessor::constantFx(String val)
{
    uint8_t val_i = val.toInt();

    for (int i = 0; i < kNumLeds; i++)
    {
        ledStrip_[i].setHSV(val_i, 255, 255);
    }
}

void LightingProcessor::setHSV(u_int8_t index, uint8_t H, uint8_t S, uint8_t V)
{
    ledStrip_[index].setHSV(H, S, V);
    ledStripH[index] = H;
    ledStripS[index] = S;
    ledStripV[index] = V;
}