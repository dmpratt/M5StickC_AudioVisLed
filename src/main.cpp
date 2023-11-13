/**
    M5StickC_AudioVisLedApp:
    This application has been developed to use an M5StickC device (ESP32)
    as an audio sampling and visualization device. It samples audio data
    from the built-in microphone using i2s. The sampled data is transformed
    into the frequency domain using the arduinoFFT library.
    Copyright (C) 2021 by Ernst Sikora

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Arduino.h>
#include <M5StickCPlus.h>
#include "FFTProcessor.h"
#include "LightingProcessor.h"
#include "BluetoothSerial.h"

FFTProcessor fftProcessor;
LightingProcessor light;
BluetoothSerial SerialBT;

void setup()
{
  SerialBT.begin("DJ Lights 1");
  log_d("M5.begin!");
  M5.begin();

  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);

  M5.Lcd.setTextSize(4);
  M5.Lcd.setTextColor(BLUE, BLACK);
  M5.Lcd.println("DJ Lights ");

  fftProcessor.setupI2Smic();
  fftProcessor.setupSpectrumAnalysis();
  light.setupLedStrip();

  log_d("Setup successfully completed.");
  log_d("portTICK_PERIOD_MS: %d", portTICK_PERIOD_MS);

  delay(1000);
}

void loop()
{
  String btData;
  if (SerialBT.available())
  {
    btData = SerialBT.readString();
    btData.trim();
  }

  fftProcessor.loop();
  light.updateLedStrip(fftProcessor.getLightness(), fftProcessor.getBeatHit(), btData);

  M5.update();
  if(M5.BtnB.wasPressed()) {
    int *lightness = fftProcessor.getLightness();
    for (uint8_t i = 0; i < sizeof(lightness); i++)
    {
        Serial.printf("LED %i = %i\n", i, lightness[i]);
    }
  }
}