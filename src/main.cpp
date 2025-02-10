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
#include <NimBLEDevice.h>
#include "FFTProcessor.h"
#include "LightingProcessor.h"

FFTProcessor fftProcessor;
LightingProcessor light;

/*------------------------------------------------------------------------------
  BLE instances & variables
  ----------------------------------------------------------------------------*/
BLEServer *pServer = NULL;
BLECharacteristic *pModeCharacteristic;
BLECharacteristic *pAddonCharacteristic;

// bool deviceConnected = false;
// bool oldDeviceConnected = false;
String currentMode = "";

#define SERVICE_UUID "78ac6f8b-8b47-40aa-b5ce-af08cb78befd"
#define CHARACTERISTIC_MODE_UUID "a216b303-23bc-4b36-8006-55e2ff2cc8e7"
#define CHARACTERISTIC_ADDON_UUID "847dba9e-8db5-447f-bb17-02a5ee2defc6"

/*------------------------------------------------------------------------------
  BLE Server callback
  ----------------------------------------------------------------------------*/
class ServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override
    {
        Serial.printf("Client address: %s\n", connInfo.getAddress().toString().c_str());

        /**
         *  We can use the connection handle here to ask for different connection parameters.
         *  Args: connection handle, min connection interval, max connection interval
         *  latency, supervision timeout.
         *  Units; Min/Max Intervals: 1.25 millisecond increments.
         *  Latency: number of intervals allowed to skip.
         *  Timeout: 10 millisecond increments.
         */
        pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 18);
    }

    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override
    {
        Serial.printf("Client disconnected - start advertising\n");
        NimBLEDevice::startAdvertising();
    }

    void onMTUChange(uint16_t MTU, NimBLEConnInfo &connInfo) override
    {
        Serial.printf("MTU updated: %u for connection ID: %u\n", MTU, connInfo.getConnHandle());
    }
} serverCallbacks;

/** Handler class for characteristic actions */
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
    void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
    {
        Serial.printf("%s : onRead(), value: %s\n",
                      pCharacteristic->getUUID().toString().c_str(),
                      pCharacteristic->getValue().c_str());
    }

    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
    {
        Serial.printf("%s : onWrite(), value: %s\n",
                      pCharacteristic->getUUID().toString().c_str(),
                      pCharacteristic->getValue().c_str());

        //if (pCharacteristic->getUUID().toString().c_str() == CHARACTERISTIC_MODE_UUID) {
            currentMode = pCharacteristic->getValue().c_str();
            
            M5.Lcd.setCursor(0,40);
            M5.Lcd.setTextSize(4);
            M5.Lcd.setTextColor(GREEN, BLACK);
            M5.Lcd.printf("%s          ", currentMode);
        //}
    }

    /**
     *  The value returned in code is the NimBLE host return code.
     */
    void onStatus(NimBLECharacteristic *pCharacteristic, int code) override
    {
        Serial.printf("Notification/Indication return code: %d, %s\n", code, NimBLEUtils::returnCodeToString(code));
    }

    /** Peer subscribed to notifications/indications */
    void onSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue) override
    {
        std::string str = "Client ID: ";
        str += connInfo.getConnHandle();
        str += " Address: ";
        str += connInfo.getAddress().toString();
        if (subValue == 0)
        {
            str += " Unsubscribed to ";
        }
        else if (subValue == 1)
        {
            str += " Subscribed to notifications for ";
        }
        else if (subValue == 2)
        {
            str += " Subscribed to indications for ";
        }
        else if (subValue == 3)
        {
            str += " Subscribed to notifications and indications for ";
        }
        str += std::string(pCharacteristic->getUUID());

        Serial.printf("%s\n", str.c_str());
    }
} chrCallbacks;

/** Handler class for descriptor actions */
class DescriptorCallbacks : public NimBLEDescriptorCallbacks
{
    void onWrite(NimBLEDescriptor *pDescriptor, NimBLEConnInfo &connInfo) override
    {
        std::string dscVal = pDescriptor->getValue();
        Serial.printf("Descriptor written value: %s\n", dscVal.c_str());
    }

    void onRead(NimBLEDescriptor *pDescriptor, NimBLEConnInfo &connInfo) override
    {
        Serial.printf("%s Descriptor read\n", pDescriptor->getUUID().toString().c_str());
    }
} dscCallbacks;

/*----------------------------------------------------------------------------*/


void setup()
{

/*----------------------------------------------------------------------------*/
  log_d("M5.begin!");
  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(4);
  M5.Lcd.setTextColor(BLUE, BLACK);
  M5.Lcd.println("DJ Lights ");

/*----------------------------------------------------------------------------*/
  // 1. Create the BLE Device
  NimBLEDevice::init("DJ Lights");

  // 2. Create the BLE server
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&serverCallbacks);

  // 3. Create BLE Service
  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  // 4. Create BLE Characteristics inside the service(s)
  pModeCharacteristic = pService->createCharacteristic(CHARACTERISTIC_MODE_UUID, NIMBLE_PROPERTY::WRITE);
  pModeCharacteristic->setValue("black");
  pModeCharacteristic->setCallbacks(&chrCallbacks);

  pAddonCharacteristic = pService->createCharacteristic(CHARACTERISTIC_ADDON_UUID, NIMBLE_PROPERTY::WRITE);
  pAddonCharacteristic->setValue("none");
  pAddonCharacteristic->setCallbacks(&chrCallbacks);

  // 5. Start the service(s)
  pService->start();

  // 6. Start advertising
  pServer->getAdvertising()->addServiceUUID(pService->getUUID());
  pServer->getAdvertising()->start();

  NimBLEDevice::startAdvertising();
  Serial.println("Waiting for a client connection to notify...");
/*----------------------------------------------------------------------------*/

  fftProcessor.setupI2Smic();
  fftProcessor.setupSpectrumAnalysis();
  light.setupLedStrip();

  log_d("Setup successfully completed.");
  log_d("portTICK_PERIOD_MS: %d", portTICK_PERIOD_MS);

  delay(500);
}

void loop()
{
  fftProcessor.loop();
  light.updateLedStrip(fftProcessor.getLightness(), fftProcessor.getBeatHit(), currentMode);
  currentMode = "";
  M5.update();
  if(M5.BtnB.wasPressed()) {
    int *lightness = fftProcessor.getLightness();
    for (uint8_t i = 0; i < sizeof(lightness); i++)
    {
        Serial.printf("LED %i = %i\n", i, lightness[i]);
    }
  }
}