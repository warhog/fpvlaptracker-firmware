/*
 * fpvlaptracker-led firmware
 * led module for fpv lap tracking software
 * 
 * The MIT License (MIT)
 * 
 * Copyright (c) 2017-2019 warhog
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * 
 * 
 * blink codes:
 * slow blinking - standalone mode
 * fast blinking - connected mode
 * blinking 3 times - mdns not started
 * blinking 9 times - shutdown voltage
 * blinking 10 times - internal failure
 * 
 */
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>

#include "storage.h"
#include "wificomm.h"
#include "batterymgr.h"
#include "wifiwebserver.h"
#include "wifiap.h"
#include "ws2812.h"

// debug mode flags
#define DEBUG

#define VERSION "FLTLED-R1.0"

// pin configurations
const unsigned int PIN_WEB_UPDATE = 5;

NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> neopixels(32, 12);
ledio::Ws2812 ws2812(&neopixels);
util::Storage storage;
battery::BatteryMgr batteryMgr;
unsigned long loopTime = 0L;
unsigned long lastLoopTimeRun = 0L;
comm::WifiWebServer wifiWebServer(&storage, &batteryMgr, VERSION, &loopTime);
comm::WifiComm wifiComm(&storage, &batteryMgr, VERSION, &loopTime, &ws2812);
comm::WifiAp wifiAp;
unsigned long lowVoltageTimeout = 0L;

/*---------------------------------------------------
 * application setup
 *-------------------------------------------------*/
void setup() {
#ifdef DEBUG
	Serial.begin(115200);
	Serial.println("");
	Serial.println("");
	Serial.println(F("booting"));
	Serial.flush();
    uint64_t chipId = ESP.getChipId();
    char strChipId[15] = { 0 };
    sprintf(strChipId, "%u", chipId);
    String chipString = strChipId;
    Serial.printf("Chip ID: %s\n", chipString.c_str());
	Serial.flush();
#endif

#ifdef DEBUG
	Serial.println(F("setting up ports"));
#endif
	pinMode(PIN_WEB_UPDATE, INPUT_PULLUP);
	if (digitalRead(PIN_WEB_UPDATE) == LOW) {
#ifdef DEBUG
		Serial.println(F("force wifi ap mode"));
#endif
		wifiAp.connect();
	}

#ifdef DEBUG
	Serial.println(F("reading config from eeprom"));
#endif
	EEPROM.begin(512);
	storage.load();

#ifdef DEBUG
	Serial.println(F("setup batterymgr"));
#endif
	batteryMgr.detectCellsAndSetup();
#ifdef DEBUG
	Serial.printf("alarmVoltage: %f, shutdownVoltage: %f\n", batteryMgr.getAlarmVoltage(), batteryMgr.getShutdownVoltage());
#endif

#ifdef DEBUG
	Serial.println(F("init ws2812"));
#endif
	ws2812.init();

#ifdef DEBUG
	Serial.println(F("starting wifi"));
#endif
	if (wifiAp.isConnected()) {
#ifdef DEBUG
		Serial.println(F("starting wifi webserver on AP"));
#endif
		wifiWebServer.begin();
	} else {
		// try to connect to wifi
#ifdef DEBUG
		Serial.println(F("starting wifi connect"));
#endif
		wifiConnect();
	}

	// no wifi found, reboot and retry
	if (!wifiComm.isConnected() && !wifiAp.isConnected()) {
#ifdef DEBUG
		Serial.println(F("wifi not connected, starting wifi ap"));
#endif
        wifiAp.connect();
#ifdef DEBUG
		Serial.println(F("starting wifiwebserver"));
#endif
		wifiWebServer.begin();
	}

	if (wifiWebServer.isConnected()) {
#ifdef DEBUG
		Serial.println(F("setting up mdns"));
#endif
		MDNS.addService("http", "tcp", 80);
		if (!MDNS.begin("flt-led")) {
#ifdef DEBUG
			Serial.println(F("error setting up MDNS responder!"));
#endif
			blinkError(3);
		}
	}

#ifdef DEBUG
	Serial.println(F("entering main loop"));
#endif
}

/*---------------------------------------------------
 * application main loop
 *-------------------------------------------------*/
void loop() {

// 	batteryMgr.measure();
// 	if (batteryMgr.isShutdown()) {
// #ifdef DEBUG
// 		Serial.println(F("voltage isShutdown"));
// #endif
// 		blinkError(9);
// 	} else if (batteryMgr.isAlarm() && millis() > lowVoltageTimeout) {
// 		// undervoltage warning
// #ifdef DEBUG
// 		Serial.println(F("voltage isAlarm"));
// #endif
// 		lowVoltageTimeout = millis() + (30 * 1000);
// 		if (wifiComm.isConnected()) {
// #ifdef DEBUG
// 			Serial.println(F("wifi voltage sendAlarm"));
// #endif
// 			wifiComm.sendVoltageAlarm();
// 		}
// 	}

	ws2812.run();

	if (wifiComm.isConnected()) {
		// process incoming udp
		wifiComm.processIncomingMessage();
	}

	if (wifiWebServer.isConnected()) {
		// handle the wifi webserver data
		wifiWebServer.handle();
	}

	loopTime = micros() - lastLoopTimeRun;
	lastLoopTimeRun = micros();

}

void wifiConnect() {
#ifdef DEBUG
	Serial.println(F("connecting wifi"));
#endif
	wifiComm.connect();
	if (wifiComm.isConnected()) {
#ifdef DEBUG
		Serial.println(F("wifi connected, register and start webserver"));
#endif
		wifiComm.reg();
		wifiWebServer.begin();
#ifdef DEBUG
		Serial.println(F("registered and webserver started"));
	} else {
		Serial.println(F("wifi not connected"));
#endif
	}
}

/*---------------------------------------------------
 * error code blinker
 *-------------------------------------------------*/
void blinkError(unsigned int errorCode) {
#ifdef DEBUG
	Serial.print(F("error code: "));
	Serial.println(errorCode);
	Serial.flush();
#endif
	while (true) {
		for (int i = 0; i < errorCode; i++) {
//			led.on();
			delay(100);
//			led.off();
			delay(200);
		}
		delay(1000);
	}
}

