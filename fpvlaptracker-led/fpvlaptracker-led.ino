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
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WS2812FX.h>

#include "ledcontrol.h"
#include "storage.h"
#include "wificomm.h"
#include "batterymgr.h"
#include "wifiwebserver.h"
#include "wifiap.h"
#include "ESP32_RMT_Driver.h"
#include "ws2812.h"

// debug mode flags
#define DEBUG

#define VERSION "FLTLED-R1.0"

// pin configurations
const unsigned int PIN_LED_WS2812 = 13;
const unsigned int PIN_LED = 19;
const unsigned int PIN_WEB_UPDATE = 32;
const unsigned int PIN_ANALOG_BATTERY = 33;

WS2812FX ws2812fx = WS2812FX(32, PIN_LED_WS2812, NEO_GRB + NEO_KHZ800); // 32 RGB LEDs driven by GPIO_12
ledio::Ws2812 ws2812(&ws2812fx);
ledio::LedControl led(PIN_LED);
util::Storage storage;
battery::BatteryMgr batteryMgr(PIN_ANALOG_BATTERY, &storage);
unsigned long loopTime = 0L;
unsigned long lastLoopTimeRun = 0L;
comm::WifiWebServer wifiWebServer(&storage, &batteryMgr, VERSION, &loopTime);
comm::WifiComm wifiComm(&storage, &batteryMgr, VERSION, &loopTime, &ws2812);
comm::WifiAp wifiAp;
unsigned long lowVoltageTimeout = 0L;

// Custom show functions which will use the RMT hardware to drive the LEDs.
// Need a separate function for each ws2812fx instance.
void customShow(void) {
	uint8_t *pixels = ws2812fx.getPixels();
	// numBytes is one more then the size of the ws2812fx's *pixels array.
	// the extra byte is used by the driver to insert the LED reset pulse at the end.
	uint16_t numBytes = ws2812fx.getNumBytes() + 1;
	rmt_write_sample(RMT_CHANNEL_1, pixels, numBytes, false); // channel 0
}

/*---------------------------------------------------
 * application setup
 *-------------------------------------------------*/
void setup() {
	led.off();
#if defined(DEBUG) || defined(MEASURE)
	Serial.begin(115200);
	Serial.println("");
	Serial.println("");
#ifdef DEBUG
	Serial.println(F("booting"));
    uint64_t chipId = ESP.getEfuseMac();
    char strChipId[15] = { 0 };
    sprintf(strChipId, "%u", chipId);
    String chipString = strChipId;
    Serial.printf("Chip ID: %s\n", chipString.c_str());
#endif
	Serial.flush();
#endif

	randomSeed(analogRead(PIN_ANALOG_BATTERY));

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
	// blink <cell number> of times to give feedback on number of connected battery cells and signal startup
	led.mode(ledio::modes::BLINK_SEQUENCE);
	led.blinkSequence(batteryMgr.getCells(), 15, 250);
	for (unsigned int i = 0; i < ((250 + 15) * batteryMgr.getCells()); i++) {
		led.run();
		delay(1);
	}
	led.off();

#ifdef DEBUG
	Serial.println(F("setting up ws2812fx"));
#endif
	ws2812fx.init();
	ws2812fx.setBrightness(255);
	rmt_tx_int(RMT_CHANNEL_1, ws2812fx.getPin());
	ws2812fx.setCustomShow(customShow);
	ws2812fx.setSegment(0, 0,  32-1, FX_MODE_COMET, BLUE,  1000, NO_OPTIONS);
	ws2812fx.start();

	ws2812fx.setColor(RED);

	while (true) {
		ws2812fx.service();
	}

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

	batteryMgr.measure();
	if (batteryMgr.isShutdown()) {
#ifdef DEBUG
		Serial.println(F("voltage isShutdown"));
#endif
		blinkError(9);
	} else if (batteryMgr.isAlarm() && millis() > lowVoltageTimeout) {
		// undervoltage warning
#ifdef DEBUG
		Serial.println(F("voltage isAlarm"));
#endif
		lowVoltageTimeout = millis() + (30 * 1000);
		if (wifiComm.isConnected()) {
#ifdef DEBUG
			Serial.println(F("wifi voltage sendAlarm"));
#endif
			wifiComm.sendVoltageAlarm();
		}
	}

	led.run();
	
	ws2812.run();
    ws2812fx.service();

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
	led.mode(ledio::modes::STATIC);
	while (true) {
		for (int i = 0; i < errorCode; i++) {
			led.on();
			delay(100);
			led.off();
			delay(200);
		}
		delay(1000);
	}
}

