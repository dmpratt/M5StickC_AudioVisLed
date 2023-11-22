#include "LightingProcessor.h"

/* ----- From FFTProcessor ----- */
const uint8_t kFreqBandCount = 20;

/* ----- Fastled constants ----- */
const uint8_t kPinLedStrip = 26; // M5StickC grove port, yellow cable
const uint8_t kNumLeds = 139;
const uint8_t kLedStripBrightness = 255;
const uint32_t kMaxMilliamps = 2500;

const uint8_t numFreqLeds = floor(kNumLeds / 2 / (kFreqBandCount + 1));                     // (139 / 2) = (69.5 / 21) = 3
const uint8_t numBassLeds = floor(kNumLeds / 2) - kFreqBandCount * numFreqLeds;             // (139 / 2) = (69.5) - (20 * 3) = 9
const uint8_t numExtraLeds = kNumLeds - ((kFreqBandCount * numFreqLeds + numBassLeds) * 2); // 139 - (20 * 3 + 9) = 69 * 2 = 138 = 1

enum PrimaryDisplays
{
    Default,
    TwoTone,
    Solid
};

enum DisplayModifiers { None, Sparkle };

uint8_t displayPrimary = PrimaryDisplays::Default;
uint8_t displayPrimaryAttribute1 = 0; // to be interpreted by primary display
uint8_t displayModifier = DisplayModifiers::None;

/* ----- Fastled variables -----
0   = Red
21  = Orange
42  = Yellow
85  = Green
127 = Cyan
170 = Blue
191 = Indigo
212 = Purple
234 = Pink
*/
// LED strip controller
CRGB ledStrip_[kNumLeds];
uint8_t ledStripH[kNumLeds];
uint8_t ledStripS[kNumLeds];
uint8_t ledStripV[kNumLeds];

uint8_t kBassHue = 160; // Blueish
uint8_t beatVisIntensity_ = 0;
uint8_t beatCounter = 0;
uint8_t beatModifier = 0;
int *freqBinLightness;

// Normal color cycling
uint8_t colorStart = 30; // Orange-Yellow
uint8_t colorEnd = 210;  // Purple
uint8_t colorStep = floor((colorEnd - colorStart) / (numFreqLeds * kFreqBandCount));

// 2-tone mode
uint8_t colorOneStart = 250; // Red
uint8_t colorTwoStart = 80;  // Green

/* ------ Sparkle Vars ------ */
uint8_t sparkle_delay[1];
uint8_t sparkle_step[1];
uint8_t sparkle_led[1];
uint8_t sparkle_hue[1];
uint8_t sparkle_saturation[1];
uint8_t sparkle_brightness[1];

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

    if (beatVisIntensity_ == 150)
        beatCounter++;

    // Toggle colors
    if (beatCounter > 0 && beatCounter % 8 == 0)
    {
        beatCounter = 0;
        beatModifier = (beatModifier == 0) ? 1 : 0;
    }

    // Update mode if new mode signal received
    if (!modifier.isEmpty())
    {
        String mode = modifier;
        mode.toLowerCase();

        if (mode == "default")
            displayPrimary = PrimaryDisplays::Default;
        else if (mode == "christmas")
            displayPrimary = PrimaryDisplays::TwoTone;
        else if (mode == "sparkle")
            displayModifier = (displayModifier == DisplayModifiers::Sparkle) ? DisplayModifiers::None : DisplayModifiers::Sparkle;
        else if (mode.indexOf('-') >= 0)
        {
            String key = mode.substring(0, mode.indexOf('-'));
            key.trim();
            String value = mode.substring(mode.indexOf('-') + 1);
            value.trim();
            displayPrimaryAttribute1 = value.toInt();

            if (key == "solid")
            {
                displayPrimary = PrimaryDisplays::Solid;
            }
        }

        Serial.printf("Mode = %i, Modifier = %i, Extra = %i\n", displayPrimary, displayModifier, displayPrimaryAttribute1);
    }

    if (displayPrimary == PrimaryDisplays::Default)
        defaultSoundFx();
    else if (displayPrimary == PrimaryDisplays::TwoTone)
        twoToneSoundFx();
    else if (displayPrimary == PrimaryDisplays::Solid)
        constantFx(displayPrimaryAttribute1);

    if (displayModifier == DisplayModifiers::Sparkle)
        sparkleFx();

    FastLED.show();
}

void LightingProcessor::defaultSoundFx()
{
    uint8_t ledIndex = 0;
    uint8_t ledRevIndex = kNumLeds - 1;

    // Show beat detection at the beginning and end of the strip
    for (int i = 0; i < numBassLeds; i++)
    {
        setHSV(ledIndex++, kBassHue, 255, beatVisIntensity_);
        setHSV(ledRevIndex--, kBassHue, 255, beatVisIntensity_);
    }

    // Show frequency intensities on the remaining Leds
    uint8_t color = colorStart;
    for (int k = 0; k < kFreqBandCount; k++)
    {
        for (int j = 0; j < numFreqLeds; j++)
        {
            setHSV(ledIndex++, color, 255, freqBinLightness[k]);
            setHSV(ledRevIndex--, color, 255, freqBinLightness[k]);
            color += colorStep;
        }
    }

    // If extra leds are odd, give extra 1 to the last band aka the center band.
    if (numExtraLeds == 1)
    {
        setHSV(ledIndex, color, 255, freqBinLightness[kFreqBandCount - 1]);
    }

    kBassHue++; // Increment base hue so it slowly changes color
}

void LightingProcessor::twoToneSoundFx()
{
    uint8_t ledIndex = 0;
    uint8_t ledRevIndex = kNumLeds - 1;

    // Show beat detection at the beginning and end of the strip
    for (int i = 0; i < numBassLeds; i++)
    {
        setHSV(ledIndex++, kBassHue, 255, beatVisIntensity_);
        setHSV(ledRevIndex--, kBassHue, 255, beatVisIntensity_);
    }

    // Show frequency intensities on the remaining Leds
    uint8_t color = colorOneStart;
    colorStep = 2;
    for (int k = 0; k < kFreqBandCount; k++)
    {
        if (k % 10 == 0)
        {
            color = (beatModifier == 0) ? colorOneStart : colorTwoStart;
        }
        else if (k % 5 == 0)
        {
            color = (beatModifier == 0) ? colorTwoStart : colorOneStart;
        }

        for (int j = 0; j < numFreqLeds; j++)
        {
            setHSV(ledIndex++, color, 255, freqBinLightness[k]);
            setHSV(ledRevIndex--, color, 255, freqBinLightness[k]);
            color += colorStep;
        }
    }

    // If extra leds are odd, give extra 1 to the last band aka the center band.
    if (numExtraLeds == 1)
    {
        setHSV(ledIndex, color, 255, freqBinLightness[kFreqBandCount - 1]);
    }
}

void LightingProcessor::sparkleFx()
{
    for (int x = 0; x < sizeof(sparkle_led); x++)
    {
        if (sparkle_delay[x] == 0 && sparkle_step[x] == 0)
        {
            srand(time(NULL));
            uint8_t randStart = rand() % 50;
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
            ledStrip_[sparkle_led[x] - 1].setHSV(sparkle_hue[x], sparkle_saturation[x] + 100, sparkle_brightness[x]);
            ledStrip_[sparkle_led[x]].setHSV(sparkle_hue[x], sparkle_saturation[x], sparkle_brightness[x]);
            ledStrip_[sparkle_led[x] + 1].setHSV(sparkle_hue[x], sparkle_saturation[x] + 100, sparkle_brightness[x]);
            if (sparkle_saturation[x] == 0)
                sparkle_step[x]++;
            break;
        case 12:
            sparkle_brightness[x] -= 20;
            sparkle_saturation[x] += 5;
            ledStrip_[sparkle_led[x] - 1].setHSV(sparkle_hue[x], sparkle_saturation[x] + 100, sparkle_brightness[x]);
            ledStrip_[sparkle_led[x]].setHSV(sparkle_hue[x], sparkle_saturation[x], sparkle_brightness[x]);
            ledStrip_[sparkle_led[x] + 1].setHSV(sparkle_hue[x], sparkle_saturation[x] + 100, sparkle_brightness[x]);
            if (sparkle_brightness[x] < 50)
                sparkle_step[x] = 0;
            break;
        default:
            ledStrip_[sparkle_led[x] - 1].setHSV(sparkle_hue[x], sparkle_saturation[x] + 100, sparkle_brightness[x]);
            ledStrip_[sparkle_led[x]].setHSV(sparkle_hue[x], sparkle_saturation[x], sparkle_brightness[x]);
            ledStrip_[sparkle_led[x] + 1].setHSV(sparkle_hue[x], sparkle_saturation[x] + 100, sparkle_brightness[x]);
            sparkle_step[x]++;
        }
    }
}

void LightingProcessor::constantFx(uint8_t val)
{
    for (int i = 0; i < kNumLeds; i++)
    {
        ledStrip_[i].setHSV(val, 255, 255);
    }
}

void LightingProcessor::setHSV(u_int8_t index, uint8_t H, uint8_t S, uint8_t V)
{
    ledStrip_[index].setHSV(H, S, V);
    ledStripH[index] = H;
    ledStripS[index] = S;
    ledStripV[index] = V;
}