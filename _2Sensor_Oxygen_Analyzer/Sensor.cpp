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

//this version of the gasContent function caculates oxygen content, and uses a quadratic curve fit to do a simple calculation for helium content (without correcting for oxygen content)
float Sensor::gasContent() {

	if (m_sensorType == OXYGEN) {
		if (m_sensorType == OXYGEN && this->isActive()) {
			return  this->mv() / this->factor() * 100.0;
		}
	}
	else if (m_sensorType == HELIUM) {
		const float a = -0.522067;
		const float b = 0.134723;
		const float c = -0.00002943842;
		const float d = 0.00000008343704;

		float adjustedMv = this->mv();
		//quadratic curve equation: y = a + bx + cx^2 + dx^3 where y is helium and x is mv
		return a + b*adjustedMv + c*sq(adjustedMv) + d*pow(adjustedMv, 3.0); //curve fitting based on a vq31mb at 3.8 mV in air
	}
	return 0.0;
}

//this version of the gasContent function uses a 3-dimensional curve fit to account for the non-linear response in varying oxygen and helium mixes. thanks to zunzun.com for their excellent curve fitting app
float Sensor::gasContent(float oxygenContent) {

	if (m_sensorType == HELIUM) {
		double heContent;
		heContent = 0.0;
		double a = 1.4610559504218952E0;
		double b = 1.0993977915597843E-1;
		double c = 5.5707457038157646E-5;
		double d = -6.6547586823996904E-2;
		double f = 5.9370452670676590E-4;
		double g = -1.4752241260518328E-6;
		heContent += a;
		heContent += b * this->mv();
		heContent += c * pow(this->mv(), 2.0);
		heContent += d * oxygenContent;
		heContent += f * oxygenContent * this->mv();
		heContent += g * oxygenContent * pow(this->mv(), 2.0);
		return heContent;
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

