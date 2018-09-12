#include <Arduino.h>
#include "rssi.h"

using namespace lap;

Rssi::Rssi(unsigned int pin) : _currentRssiRawValue(0), _currentRssiValue(0), _rssiOffset(0), _pin(pin), _currentRssiSmoothed(0.0f), _filterRatio(0.01f) {
	pinMode(pin, INPUT);
}

void Rssi::process() {
	int rssi = analogRead(this->_pin);
	rssi -= this->getRssiOffset();
	if (rssi < 0) {
		rssi = 0;
	}
	this->_currentRssiRawValue = rssi;
	this->_currentRssiSmoothed = (this->_filterRatio * (double)this->_currentRssiRawValue) + ((1.0f - this->_filterRatio) * this->_currentRssiSmoothed);
	this->_currentRssiValue = (unsigned int)this->_currentRssiSmoothed;
}
