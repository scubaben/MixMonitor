/*Copyright (c) 2017 Ben Shiner
Special thanks to JJ Crawford for his technical guidance and numerous contributions

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
 */

#include <EEPROM.h>
#include "Sensor.h"

Sensor::Sensor(int sensorNumber, sensorType_t sensorType) {
	sensorIndex = sensorNumber;
	m_sensorType = sensorType;
	_adc.begin();
	switch (sensorType) {
	case OXYGEN:
		lowerFactorLimit = 1.615;
		upperFactorLimit = 2.625;
		m_gain = GAIN_SIXTEEN;
		adcRange = 256.0;
		break;
	case HELIUM:
		lowerFactorLimit = 0.01; //need to update these values to appropriate values for the pellistor
		upperFactorLimit = 1.0;  //need to update these values to appropriate values for the pellistor
		m_gain = GAIN_FOUR;
		adcRange = 1024.0;
		break;
	}
}

boolean Sensor::isConnected() {
	if (this->mv() > 0.01) return true;

	return false;
}

boolean Sensor::isCalibrated() {
	if (this->factor() > 0.0) return true;

	return false;
}

boolean Sensor::isActive() {
	if (this->isCalibrated() && this->isConnected()) return true;

	return false;
}

boolean Sensor::isInTolerance() {
	float upperLimit = (float)this->target / 10.0 + (float)this->tolerance / 10.0;
	float lowerLimit = (float)this->target / 10.0 - (float)this->tolerance / 10.0;

	if (!this->isActive()) return true;

	if ((this->gasContent() > upperLimit) || (this->gasContent() < lowerLimit)) return false;

	return true;
}

float Sensor::factor() {
	int eeAddress = sensorIndex * sizeof(float) * 2;
	if (!this->calibrationLoaded) {
		EEPROM.get(eeAddress, this->savedFactor);
		this->calibrationLoaded = true;
		if (this->savedFactor < this->lowerFactorLimit || this->savedFactor > this->upperFactorLimit) {
			this->savedFactor = 0.0;
		}
	}
	return this->savedFactor;
}

float Sensor::offset() {
	int eeAddress = sensorIndex * sizeof(float) * 2 + sizeof(float);
	if (m_sensorType == HELIUM) {
		EEPROM.get(eeAddress, this->savedOffset);
	}
	else {
		this->savedOffset = 0.0;
	}
	return this->savedOffset;
}

float Sensor::mv() {
	_adc.setGain(m_gain);
	if (sensorIndex == 0) {
		return _adc.readADC_Differential_0_1() * adcRange / 32767.0; //read from ADC and convert to mv
	}
	if (sensorIndex == 1) {
		return _adc.readADC_Differential_2_3() * adcRange / 32767.0; //read from ADC and convert to mv
	}
	return 0.0;
}

float Sensor::gasContent() {
	if (this->isActive() && this->mv() > 0.0) {
		return  (this->mv() - this->offset()) * this->factor();
	}
	return 0.0;
}

void Sensor::saveCalibration(float calData) {
	int eeAddress = sensorIndex * sizeof(float) * 2;
	EEPROM.put(eeAddress, calData);
	this->calibrationLoaded = false;
}

void Sensor::saveCalibration(float calData, float calOffset) {
	int eeAddress = sensorIndex * sizeof(float) * 2;
	EEPROM.put(eeAddress, calData);
	eeAddress = eeAddress + sizeof(float);
	EEPROM.put(eeAddress, calOffset);
	this->calibrationLoaded = false;
}

void Sensor::setTarget(int target) {
	this->target = target;
}

int Sensor::getTarget() {
	return this->target;
}

void Sensor::setTolerance(int tolerance) {
	this->tolerance = tolerance;
}

int Sensor::getTolerance() {
	return this->tolerance;
}

sensorType_t Sensor::getSensorType() {
	return this->m_sensorType;
}

