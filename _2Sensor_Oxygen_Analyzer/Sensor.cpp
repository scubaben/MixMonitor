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
	this->sensorIndex = sensorNumber;
	this->m_sensorType = sensorType;
	_adc.begin();
	switch (sensorType) {
	case OXYGEN:
		this->lowerFactorLimit = 38.09;
		this->upperFactorLimit = 61.90;
		this->m_gain = GAIN_SIXTEEN;
		this->adcRange = 256.0;
		break;
	case HELIUM:
		this->lowerFactorLimit = 0.1; //need to update these values to appropriate values for the pellistor
		this->upperFactorLimit = 700.0;  //need to update these values to appropriate values for the pellistor
		this->m_gain = GAIN_FOUR;
		this->adcRange = 1024.0;
		this->calibrationLoaded = true;
		break;
	}
}

boolean Sensor::isConnected() {
	if (this->mv() > 0.01 || m_sensorType == HELIUM) return true;

	return false;
}

boolean Sensor::isCalibrated() {
	if (this->factor() > 0.0 || m_sensorType == HELIUM) return true;

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
	_adc.setGain(this->m_gain);
	if (sensorIndex == 0) {
		return _adc.readADC_Differential_0_1() * adcRange / 32767.0; //read from ADC and convert to mv
	}
	if (sensorIndex == 1) {
		return _adc.readADC_Differential_2_3() * adcRange / 32767.0; //read from ADC and convert to mv
	}
	return 0.0;
}

float Sensor::gasContent() {

	if (m_sensorType == OXYGEN) {
		if (m_sensorType == OXYGEN && this->isActive()) {
			return  this->mv() / this->factor() * 100.0;
		}
	}
	else if (m_sensorType == HELIUM) {
		//these constants are for the helium calculation equation without correcting for o2 content.  these values are still being fine tuned.
		const float a = 0.2485473;
		const float b = 0.1184121;
		const float c = 0.00004771562;

		float adjustedMv = this->mv();
		//quadratic curve equation: y = a + bx + cx^2 where y is helium and x is mv
		return a + b*adjustedMv + c*sq(adjustedMv); //curve fitting based on a vq31mb zero'd (.2mV) in air
	}
	return 0.0;
}

//this version of the gasContent function is still undegoing testing
float Sensor::gasContent(float oxygenContent) {

	if (m_sensorType == HELIUM) {
		//these constants are for the helium calculation equation with a correction for o2 content. these values are still being fine tuned.
		const float a = 0.04202149;
		const float b = 0.1160009;
		const float c = 0.00005523657;
		const float o2AdjustmentFactor = 0.39240506;  // represents the mV per % of o2 that is added or subtracted for difference from air.

		float adjustedMv = this->mv();
		adjustedMv -= (oxygenContent - 21.0) * o2AdjustmentFactor; //adjust mv for oxygen contents different than air
		return a + b*adjustedMv + c*sq(adjustedMv); //curve fitting based on a vq31mb zero'd (.2mV) in air AND the effect of oxygen on thermal conductivity
	}
	return 0.0;
}

void Sensor::saveCalibration(float calData) {
	int eeAddress = sensorIndex * sizeof(float) * 2;
	EEPROM.put(eeAddress, calData);
	this->calibrationLoaded = false;
}

bool Sensor::validateCalibration(float calibrationPoint) {
	if (m_sensorType == HELIUM) {
		return true;
	}
	else if (this->mv() / calibrationPoint > this->lowerFactorLimit && this->mv() / calibrationPoint < this->upperFactorLimit) {
		return true;
	}
	return false;
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

