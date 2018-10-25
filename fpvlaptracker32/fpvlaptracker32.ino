/*
 * fpvlaptracker32 firmware
 * fpv lap tracking software that uses rx5808 to keep track of the fpv video signals
 * rssi for lap detection
 * 
 * 
 * the channel mapping tables are copied from shea iveys rx5808-pro-diversity project
 * https://github.com/sheaivey/rx5808-pro-diversity/blob/master/src/rx5808-pro-diversity/rx5808-pro-diversity.ino
 * 
 * the spi rx5808 code is taken from chickadee laptimer58 project
 * https://github.com/chickadee-tech/laptimer58
 * 
 * The MIT License (MIT)
 * 
 * Copyright (c) 2017-2018 warhog
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
 * blinking 2 times - BT name command not OK
 * blinking 3 times - mdns not started
 * blinking 4 times - BT init failed
 * blinking 9 times - shutdown voltage
 * blinking 10 times - internal failure
 * 
 */
#include <EEPROM.h>
#include <BluetoothSerial.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "rssi.h"
#include "lapdetector.h"
#include "ledcontrol.h"
#include "storage.h"
#include "wificomm.h"
#include "btcomm.h"
#include "frequency.h"
#include "rx5808.h"
#include "statemanager.h"
#include "webupdate.h"
#include "batterymgr.h"
#include "wifiwebserver.h"

// debug mode flags
//#define DEBUG
//#define MEASURE

#define VERSION "FLT32-R1.0"

// pin configurations
const unsigned int PIN_SPI_SLAVE_SELECT = 16;
const unsigned int PIN_SPI_CLOCK = 17;
const unsigned int PIN_SPI_DATA = 18;
const unsigned int PIN_LED = 19;
const unsigned int PIN_ANALOG_RSSI = 32;
const unsigned int PIN_WEB_UPDATE = 34;
const unsigned int PIN_ANALOG_BATTERY = 33;

lap::Rssi rssi(PIN_ANALOG_RSSI);
ledio::LedControl led(PIN_LED);
util::Storage storage;
lap::LapDetector lapDetector(&storage, &rssi);
radio::Rx5808 rx5808(PIN_SPI_CLOCK, PIN_SPI_DATA, PIN_SPI_SLAVE_SELECT, PIN_ANALOG_RSSI);
BluetoothSerial btSerial;
battery::BatteryMgr batteryMgr(PIN_ANALOG_BATTERY, &storage);
statemanagement::StateManager stateManager;
unsigned long loopTime = 0L;
unsigned long lastLoopTimeRun = 0L;
comm::WifiWebServer wifiWebServer(&storage, &rssi, &rx5808, &lapDetector, &batteryMgr, VERSION, &stateManager, &loopTime);
comm::WifiComm wifiComm(&storage, &rssi, &rx5808, &lapDetector, &batteryMgr, VERSION, &stateManager, &loopTime);
comm::BtComm btComm(&btSerial, &storage, &rssi, &rx5808, &lapDetector, &batteryMgr, VERSION, &stateManager, &wifiComm, &loopTime);
unsigned long fastRssiTimeout = 0L;
WebUpdate webUpdate(&storage);
bool lowVoltageSent = false;

/*---------------------------------------------------
 * application setup
 *-------------------------------------------------*/
void setup() {
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
#ifdef MEASURE
	Serial.println(F("INFO: running measure mode"));
#endif
	Serial.flush();
#endif

	batteryMgr.enableVrefOutput();

	// blink led to show startup
	for (int i = 0; i < 20; i++) {
		led.toggle();
		delay(25);
	}
	led.off();

	randomSeed(analogRead(PIN_ANALOG_BATTERY));

#ifdef DEBUG
	Serial.println(F("setting up ports"));
#endif
	rx5808.init();
	pinMode(PIN_WEB_UPDATE, INPUT_PULLUP);
	if (digitalRead(PIN_WEB_UPDATE) == LOW) {
#ifdef DEBUG
		Serial.println(F("enabling webupdate mode"));
#endif		
		stateManager.setState(statemanagement::state_enum::WEBUPDATE);
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

	if (stateManager.isStateWebupdate()) {
		// add delay to make it possible to measure the vref output voltage
		// by the user
		delay(5000);
		// running in webupdate mode
#ifdef DEBUG
		Serial.println(F("starting webupdate mode"));
		Serial.println(F("setting up wifi ap"));
#endif
		WiFi.softAP("fltunit", "fltunit");
#ifdef DEBUG
		IPAddress myIP = WiFi.softAPIP();
		Serial.print(F("AP IP address: "));
		Serial.println(myIP);
#endif

#ifdef DEBUG
		Serial.println(F("setting up mdns"));
#endif
		if (!MDNS.begin("fltunit")) {
#ifdef DEBUG
			Serial.println(F("error setting up MDNS responder!"));
#endif
			blinkError(3);
		}
		webUpdate.setVersion(VERSION);
		webUpdate.begin();
	    MDNS.addService("http", "tcp", 80);

		// blink 5 times to show end of setup() and start of webupdate
		led.mode(ledio::modes::BLINK_SEQUENCE);
		led.blinkSequence(5, 500, 1000);
	} else {
#ifdef DEBUG
		Serial.println(F("starting in normal mode"));
#endif
		// non webupdate mode
		lapDetector.init();

#ifdef DEBUG
		Serial.println(F("setting radio frequency"));
#endif
		unsigned int channelData = freq::Frequency::getSPIFrequencyForChannelIndex(storage.getChannelIndex());
		rx5808.freq(channelData);
#ifdef DEBUG
		Serial.print(F("channel info: "));
		Serial.print(freq::Frequency::getFrequencyForChannelIndex(storage.getChannelIndex()));
		Serial.print(F(" MHz, "));
		Serial.println(freq::Frequency::getChannelNameForChannelIndex(storage.getChannelIndex()));
#endif

#ifdef DEBUG
		Serial.println(F("connecting wifi"));
#endif
		wifiComm.connect();
		if (wifiComm.isConnected()) {
#ifdef DEBUG
			Serial.println(F("wifi connected, starting node registration"));
#endif
			wifiComm.reg();
			wifiWebServer.begin();
#ifdef DEBUG
			Serial.println(F("node registration done"));
		} else {
			Serial.println(F("wifi not connected"));
#endif
		}

		if (!wifiComm.isConnected()) {
#ifdef DEBUG
			Serial.println(F("connecting bluetooth"));
#endif
			int bterr = btComm.connect();
			if (bterr < 0) {
#ifdef DEBUG
				Serial.print(F("bt module error: "));
				Serial.println(bterr);
#endif
				if  (bterr == comm::btErrorCode::NAME_COMMAND_FAILED) {
					blinkError(2);
				} else if  (bterr == comm::btErrorCode::INIT_FAILED) {
					blinkError(4);
				}
			}
#ifdef DEBUG
			Serial.println(F("bluetooth connected"));
#endif
		}
		// blink <cell number> times to show end of setup() and start of calibration
		led.mode(ledio::modes::BLINK_SEQUENCE);
		led.blinkSequence(batteryMgr.getCells(), 15, 250);
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
	if (batteryMgr.isShutdown() && !stateManager.isStateWebupdate()) {
#ifdef DEBUG
		Serial.println(F("voltage isShutdown"));
#endif
		blinkError(9);
	} else if (!lowVoltageSent && batteryMgr.isAlarm()) {
#ifdef DEBUG
		Serial.println(F("voltage isAlarm"));
#endif
		// undervoltage
		if (btComm.isConnected() && btComm.hasClient()) {
#ifdef DEBUG
			Serial.println(F("voltage sendAlarm"));
#endif
			btComm.sendVoltageAlarm();
			lowVoltageSent = true;
		}
	}

	led.run();
	
	rssi.process();
#ifdef MEASURE
	Serial.print(F("VAR: rssi="));
	Serial.println(rssi.getRssi());
#endif

	if (stateManager.isStateStartup()) {
#if defined(DEBUG) || defined(MEASURE)
		Serial.println(F("STATE: STARTUP"));
#endif
		stateManager.setState(statemanagement::state_enum::CALIBRATION);
		lapDetector.enableCalibrationMode();
		rssi.setFilterRatio(storage.getFilterRatioCalibration());
#ifdef DEBUG
		Serial.println(F("switch to calibration mode"));
#endif
		led.interval(50);
		led.mode(ledio::modes::BLINK);
	} else if (stateManager.isStateScan()) {
		rx5808.scan();
		if (rx5808.isScanDone()) {
			// scan is done, start over
			unsigned int currentChannel = rx5808.getScanChannelIndex();
			btComm.sendScanData(freq::Frequency::getFrequencyForChannelIndex(currentChannel), rx5808.getScanResult());
			currentChannel++;
			if (currentChannel >= freq::NR_OF_FREQUENCIES) {
				currentChannel = 0;
			}
			rx5808.startScan(currentChannel);
		}
	} else if (stateManager.isStateRssi()) {
		if (millis() > fastRssiTimeout) {
			fastRssiTimeout = millis() + 250;
			btComm.sendFastRssiData(rssi.getRssi());
		}
	} else if (stateManager.isStateCalibration()) {
#ifdef MEASURE
		Serial.println(F("STATE: CALIBRATION"));
#endif
		if (lapDetector.process()) {
#ifdef MEASURE
			Serial.println(F("INFO: lap detected, calibration is done"));
#endif
			stateManager.setState(statemanagement::state_enum::CALIBRATION_DONE);
		}
	} else if (stateManager.isStateCalibrationDone()) {
#if defined(DEBUG) || defined(MEASURE)
		Serial.println(F("STATE: CALIBRATION_DONE"));
#endif
		if (btComm.isConnected()) {
			btComm.sendCalibrationDone();
		}
		if (wifiComm.isConnected()) {
			wifiComm.sendCalibrationDone();
		}
		rssi.setFilterRatio(storage.getFilterRatio());
		stateManager.setState(statemanagement::state_enum::RACE);
		led.mode(ledio::modes::OFF);
	} else if (stateManager.isStateRace()) {
#ifdef MEASURE
		Serial.println(F("STATE: RACE"));
#endif
		if (lapDetector.process()) {
#ifdef MEASURE
			Serial.println(F("INFO: lap detected"));
			Serial.print(F("VAR: lastlaptime="));
			Serial.println(lapDetector.getLastLapTime());
			Serial.print(F("VAR: lastlaprssi="));
			Serial.println(lapDetector.getLastLapRssi());
#endif
			led.oneshot();
			if (wifiComm.isConnected()) {
				wifiComm.lap(lapDetector.getLastLapTime(), lapDetector.getLastLapRssi());
			}
			if (btComm.isConnected()) {
				btComm.lap(lapDetector.getLastLapTime(), lapDetector.getLastLapRssi());
			}
#ifdef DEBUG
			Serial.println(F("lap detected"));
			Serial.print(F("rssi="));
			Serial.println(lapDetector.getLastLapRssi());
			Serial.print(F("laptime="));
			Serial.println(lapDetector.getLastLapTime());
#endif
		}
	} else if (stateManager.isStateWebupdate()) {
		webUpdate.run();
	} else if (stateManager.isStateError()) {
#ifdef MEASURE
		Serial.println(F("STATE: ERROR"));
#endif
	} else {
		blinkError(10);
	}

	// if we are in network mode, process udp
	if (wifiComm.isConnected()) {
		wifiComm.processIncomingMessage();
	}

	if (btComm.isConnected()) {
		btComm.processIncomingMessage();
	}

	if (wifiWebServer.isConnected()) {
		wifiWebServer.handle();
	}

	loopTime = micros() - lastLoopTimeRun;
	lastLoopTimeRun = micros();
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

