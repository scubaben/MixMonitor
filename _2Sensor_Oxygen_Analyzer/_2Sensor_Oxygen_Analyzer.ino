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

#include <Wire.h> // wiring library
#include <LiquidCrystal.h> //LCD library
#include <EEPROM.h> //eeprom library for saving calibration data
#include "Sensor.h"

#define VERSION "0.3"

#define buttonPin A3
#define encoderPinA 0
#define encoderPinB 1
#define ledPin A5
#define outPin A4
#define batteryPin A9

LiquidCrystal lcd(13, 12, 11, 10, 6, 5); //create LCD object, these pins are the ones I chose to use on the adafruit feather 32u4 proto board

int buttonState;
int lastButtonState = HIGH;
unsigned long lastSampleMillis = millis();
unsigned long lastDisplayMillis = millis();
unsigned long sampleRate = 400;
unsigned long debounceMillis = 0;
unsigned long debounceDelay = 50;
int displayMode = 0;
boolean updateRightDisplay = false;
int targetHe = 0;

//use volatie variables when they get changed by an ISR (interrupt service routine)
volatile bool aCurrentState;
volatile bool bCurrentState;
volatile bool aPreviousState;
volatile bool bPreviousState;
volatile int currentSetting;
volatile int encoderTicks;

//handy character designer: https://www.quinapalus.com/hd44780udg.html
byte thickSeparator[8] = { B1010, B100, B1010, B100, B1010, B100, B1010 };
byte thinSeparator[8] = { B100, B0, B100, B0, B100, B0, B100 };
byte arrowRight[8] = { B0, B1000, B1100, B1110, B1100, B1000, B0 };
byte arrowLeft[8] = { B0, B10, B110, B1110, B110, B10, B0 };
byte oTwo[8] = { B11, B1, B11110, B10111, B10100, B10100, B11100 };

Sensor sensor1(0, OXYGEN);
Sensor sensor2(1, OXYGEN);

void setup() {
	pinMode(buttonPin, INPUT_PULLUP);
	pinMode(encoderPinA, INPUT_PULLUP);
	pinMode(encoderPinB, INPUT_PULLUP);
	pinMode(ledPin, OUTPUT);
	pinMode(outPin, OUTPUT);

	attachInterrupt(digitalPinToInterrupt(encoderPinA), aEncoderInterrupt, CHANGE);
	attachInterrupt(digitalPinToInterrupt(encoderPinB), bEncoderInterrupt, CHANGE);

	lcd.begin(16, 2);
	lcd.createChar(0, thickSeparator);
	lcd.createChar(1, thinSeparator);
	lcd.createChar(2, arrowRight);
	lcd.createChar(3, arrowLeft);
	lcd.createChar(4, oTwo);

	lcd.setCursor(3, 0);
	lcd.print("MixMonitor");
	lcd.setCursor(0, 1);
	lcd.print(getVoltage());
	lcd.print("v");
	lcd.setCursor(11, 1);
	lcd.print("v ");
	lcd.print(VERSION);
	delay(2000);
	lcd.clear();

	if (digitalRead(buttonPin) == LOW) {
		calibrate();
	}
	else if ((sensor1.isConnected() && !sensor1.isCalibrated()) || (sensor2.isConnected() && !sensor2.isCalibrated())) {
		lcd.clear();
		lcd.print("Please");
		lcd.setCursor(0, 1);
		lcd.print("Recalibrate");
		delay(2000);
		calibrate();
	}
	lcd.clear();
}

void loop() {
	displayOxygen();
	displayRight();
	if (buttonDetect(buttonPin)) {
		optionsMenu();
	}
}

//uses edge detection and debouncing to detect button pushes
boolean buttonDetect(int detectPin) {
	boolean buttonPushed = false;
	buttonState = digitalRead(detectPin);
	if ((buttonState != lastButtonState) && buttonState == LOW) {
		debounceMillis = millis();
		lastButtonState = buttonState;
	}
	if (((millis() - debounceMillis) > debounceDelay) && buttonState != lastButtonState) {
		lastButtonState = buttonState;
		debounceMillis = millis();
		buttonPushed = true;
	}
	return buttonPushed;
}

float getVoltage() {
	float batteryVoltage = analogRead(batteryPin) * 2.0 * 3.3 / 1024; //* 2.0 because a voltage divider sends only half the voltage to the batteryPin, * 3.3 (reference voltage), / # of steps
	return batteryVoltage;
}

void displayOxygen() {
	if ((millis() - lastSampleMillis) > sampleRate) {
		if (sensor1.gasContent() > 0.1) {
			printFloat(sensor1.gasContent(), false, 0, 0);
			lcd.print("%");
		}
		else if (displayMode == 1) {
			lcd.setCursor(0, 0);
			lcd.print("-AIR-");
		}
		else {
			lcd.setCursor(0, 0);
			lcd.print("     ");
		}

		if (sensor2.gasContent() > 0.1) {
			printFloat(sensor2.gasContent(), false, 0, 1);
			lcd.print("%");
		}
		else {
			lcd.setCursor(0, 1);
			lcd.print("     ");
		}
		lcd.setCursor(6, 0);
		lcd.write(byte(0));
		lcd.setCursor(6, 1);
		lcd.write(byte(0));
		lastSampleMillis = millis();
		updateRightDisplay = true;
	}
}

void displayRight() {
	if (updateRightDisplay) {
		if (displayMode == 0) {
			if (!sensor1.isActive()) {
				lcd.setCursor(7, 0);
				lcd.print("         ");
			}
			else {
				lcd.setCursor(7, 0);
				lcd.write(byte(2));
				printFloat(sensor1.mv(), false, 8, 0);
				lcd.print(" mV");
			}
			if (!sensor2.isActive()) {
				lcd.setCursor(7, 1);
				lcd.print("         ");
			}
			else {
				lcd.setCursor(7, 1);
				lcd.write(byte(2));
				printFloat(sensor2.mv(), false, 8, 1);
				lcd.print(" mV");
			}
		}
		if (displayMode == 1) {
			if (!sensor2.isActive()) {
				lcd.setCursor(8, 0);
				lcd.print("S2 ERROR");
				lcd.setCursor(7, 1);
				lcd.print("         ");
				digitalWrite(ledPin, HIGH);
				digitalWrite(outPin, HIGH);
				displayOxygen();
			}

			if (sensor2.isActive()) {
				lcd.setCursor(7, 0);
				if (sensor1.isActive()) {
					lcd.write(byte(2));
					printInt((sensor1.getTarget() / 10), false, 8, 0);
				}
				else {
					lcd.print("   ");
				}
				lcd.setCursor(7, 1);
				lcd.write(byte(2));
				printInt((sensor2.getTarget() / 10), false, 8, 1);
				lcd.setCursor(10, 0);
				lcd.print("  Mix ");
				printInt((int)(sensor2.gasContent() + 0.5), false, 11, 1);
				lcd.print("/");
				printInt(calculateHe(sensor1.gasContent(), sensor2.gasContent()), false, 14, 1);

				if (!heInTolerance(calculateHe(sensor1.gasContent(), sensor2.gasContent()), sensor1.getTolerance())) {
					digitalWrite(ledPin, HIGH);
					digitalWrite(outPin, HIGH);
				}
				else {
					digitalWrite(ledPin, LOW);
					digitalWrite(outPin, LOW);
				}
			}
		}

		if (displayMode == 2) {
			lcd.setCursor(7, 0);
			if (sensor1.isActive()) {
				lcd.write(byte(2));
				printFloat((float)sensor1.getTarget() / 10.0, false, 8, 0);
			}
			else {
				lcd.print("         ");
			}
			lcd.setCursor(7, 1);
			if (sensor2.isActive()) {
				lcd.write(byte(2));
				printFloat((float)sensor2.getTarget() / 10.0, false, 8, 1);
			}
			else {
				lcd.print("         ");
			}
			if (!sensor1.isInTolerance() || !sensor2.isInTolerance()) {
				digitalWrite(ledPin, HIGH);
				digitalWrite(outPin, HIGH);
			}
			else {
				digitalWrite(ledPin, LOW);
				digitalWrite(outPin, LOW);
			}
		}
		updateRightDisplay = false;
	}
}

void optionsMenu() {
	lastDisplayMillis = millis();
	currentSetting = 0;
	int lastMenuSelection = currentSetting;
	boolean exitOptionsMenu = false;
	clearRightScreen();

	lcd.setCursor(8, 0);
	lcd.print("Options");
	lcd.setCursor(10, 1);
	lcd.print("Menu");
	while (((millis() - lastDisplayMillis) < 1000 && currentSetting == 0)) {
		displayOxygen();
	}
	clearRightScreen();
	currentSetting = 0;
	while (!exitOptionsMenu) {
		displayOxygen();
		if (currentSetting > 4) {
			currentSetting = 0;
		}
		else if (currentSetting < 0) {
			currentSetting = 4;
		}
		if (currentSetting != lastMenuSelection) {
			clearRightScreen();
			lastMenuSelection = currentSetting;
		}

		switch (currentSetting) {
		case 0:
			lcd.setCursor(7, 0);
			lcd.print("Set Mix");
			lcd.setCursor(7, 1);
			lcd.print("Target");
			if (buttonDetect(buttonPin)) {
				exitOptionsMenu = true;
				clearRightScreen();
				setMixTarget();
			}
			break;
		case 1:
			lcd.setCursor(7, 0);
			lcd.print("O2 Sensor");
			lcd.setCursor(7, 1);
			lcd.print("Targets");
			if (buttonDetect(buttonPin)) {
				exitOptionsMenu = true;
				clearRightScreen();
				setSensorTargets();
			}
			break;
		case 2:
			lcd.setCursor(7, 0);
			lcd.print("Disable");
			lcd.setCursor(7, 1);
			lcd.print("Targets");
			if (buttonDetect(buttonPin)) {
				clearRightScreen();
				displayMode = 0;
				digitalWrite(ledPin, LOW);
				digitalWrite(outPin, LOW);
				exitOptionsMenu = true;
			}
			break;
		case 3:
			lcd.setCursor(7, 0);
			lcd.print("Calibrate");
			if (buttonDetect(buttonPin)) {
				exitOptionsMenu = true;
				calibrate();
			}
			break;
		case 4:
			lcd.setCursor(7, 0);
			lcd.print("Exit");
			if (buttonDetect(buttonPin)) {
				clearRightScreen();
				exitOptionsMenu = true;
			}
			break;
		}
	}
}

void calibrate() {
	float calibrationPoint;
	lcd.clear();
	lcd.setCursor(4, 0);
	lcd.print("Entering");
	lcd.setCursor(0, 1);
	lcd.print("Calibration Mode");
	delay(1750);
	lcd.clear();
	while (digitalRead(buttonPin) == LOW) {
	}
	delay(250);
	lcd.print("Calibration FO2:");
	currentSetting = 209;

	//allow the user to pick their own calibration point
	while (!buttonDetect(buttonPin)) {
		if (currentSetting > 1000) {
			currentSetting = 1000;
		}
		else if (currentSetting < 0) {
			currentSetting = 0;
		}
		calibrationPoint = (float)currentSetting / 10.0;
		printFloat(calibrationPoint, true, 0, 1);
		lcd.print("% Oxygen");
	}
	lcd.clear();
	lcd.setCursor(7, 0);
	lcd.write(byte(0));
	lcd.print("Click to");
	lcd.setCursor(7, 1);
	lcd.write(byte(0));
	lcd.print("confirm");

	do {
		if (sensor1.mv() <= 0.0) {
			lcd.setCursor(0, 0);
			lcd.print("    ");
		}
		else {
			printFloat(sensor1.mv(), false, 0, 0);
		}
		lcd.print("mV");
		if (sensor2.mv() <= 0.0) {
			lcd.setCursor(0, 1);
			lcd.print("    ");
		}
		else {
			printFloat(sensor2.mv(), false, 0, 1);
		}
		lcd.print("mV");

	} while (!buttonDetect(buttonPin));

	if (sensor1.isConnected() && sensor1.validateCalibration(calibrationPoint)) {
		sensor1.saveCalibration(calibrationPoint / sensor1.mv());
	}
	else if (!sensor1.isConnected()) {
		sensor1.saveCalibration(0.0);
	}
	else {
		lcd.clear();
		lcd.print("Bad Calibration");
		lcd.setCursor(0, 1);
		lcd.print("Data");
		delay(3000);
		calibrate();
	}
	if (sensor2.isConnected() && sensor2.validateCalibration(calibrationPoint)) {
		sensor2.saveCalibration(calibrationPoint / sensor2.mv());
	}
	else if (!sensor2.isConnected()) {
		sensor2.saveCalibration(0.0);
	}
	else {
		lcd.clear();
		lcd.print("Bad Calibration");
		lcd.setCursor(0, 1);
		lcd.print("Data");
		delay(3000);
		calibrate();
	}
	if (!sensor1.isConnected() && !sensor2.isConnected()) {
		lcd.clear();
		lcd.print("Bad Calibration");
		lcd.setCursor(0, 1);
		lcd.print("Data");
		delay(3000);
		calibrate();
	}
	lcd.clear();
	lcd.print("Calibration");
	lcd.setCursor(0, 1);
	lcd.print("Saved");

	delay(1500);
	lcd.clear();
}

//update this logic to include more validations, ie s1 can't be less than 21. 
void setMixTarget() {
	lcd.setCursor(7, 0);
	lcd.print("Tgt. Mix:");

	// Set O2 Component
	currentSetting = (sensor2.getTarget() != 209) ? sensor2.getTarget() / 10 : 21;
	while (!buttonDetect(buttonPin)) {
		displayOxygen();
		if (currentSetting > 99) {
			currentSetting = 99;
		}
		else if (currentSetting < 0) {
			currentSetting = 0;
		}
		sensor2.setTarget(currentSetting * 10);
		printInt(sensor2.getTarget() / 10, true, 7, 1);
		lcd.print("/");
		printInt(targetHe, false, 13, 1);
	}

	lcd.setCursor(7, 1);
	lcd.print("        ");

	// Set He Component
	currentSetting = targetHe;
	while (!buttonDetect(buttonPin)) {

		displayOxygen();
		if (currentSetting + (sensor2.getTarget() / 10) > 99) {
			currentSetting = 100 - (sensor2.getTarget() / 10);
		}
		else if (currentSetting < 0) {
			currentSetting = 0;
		}
		sensor1.setTarget(((float)sensor2.getTarget() / 10.0) / (100.0 - (float)currentSetting) * 1000); // this sets the target for s1, the formula is: s1 = s2/1-he
		targetHe = currentSetting;  // this sets the target HE content
		printInt(sensor2.getTarget() / 10, false, 8, 1);
		lcd.print(" /");
		printInt(targetHe, true, 12, 1);
	}
	clearRightScreen();
	lcd.setCursor(7, 0);

	lcd.print("Tolerance");
	currentSetting = sensor1.getTolerance();
	while (!buttonDetect(buttonPin)) {
		displayOxygen();
		if (currentSetting > 1000) {
			currentSetting = 1000;
		}
		else if (currentSetting < 0) {
			currentSetting = 0;
		}
		sensor1.setTolerance(currentSetting);
		sensor2.setTolerance(currentSetting); //setting tolerance for both sensors at once for now, may add ability to set tolerance by sensor in the future.
		printFloat(((float)sensor1.getTolerance() / 10.0), true, 8, 1);
		lcd.print("%");
	}
	clearRightScreen();
	displayMode = 1;
}

float setSensorTargets() {
	if (sensor1.isActive()) {
		lcd.setCursor(7, 0);
		lcd.print("S1 Target");
		currentSetting = sensor1.getTarget();
		while (!buttonDetect(buttonPin)) {
			displayOxygen();
			if (currentSetting > 1000) {
				currentSetting = 1000;
			}
			else if (currentSetting < 0) {
				currentSetting = 0;
			}
			sensor1.setTarget(currentSetting);
			printFloat(((float)sensor1.getTarget() / 10.0), true, 7, 1);
			lcd.print("% ");
			lcd.write(byte(4));
		}
		clearRightScreen();
	}

	if (sensor2.isActive()) {
		lcd.setCursor(7, 0);
		lcd.print("S2 Target");
		currentSetting = sensor2.getTarget();
		while (!buttonDetect(buttonPin)) {
			displayOxygen();
			if (currentSetting > 1000) {
				currentSetting = 1000;
			}
			else if (currentSetting < 0) {
				currentSetting = 0;
			}
			sensor2.setTarget(currentSetting);
			printFloat(((float)sensor2.getTarget() / 10.0), true, 7, 1);
			lcd.print("% ");
			lcd.write(byte(4));
		}
		clearRightScreen();
	}
	lcd.setCursor(7, 0);

	lcd.print("Tolerance");
	currentSetting = sensor1.getTolerance();
	while (!buttonDetect(buttonPin)) {
		displayOxygen();
		if (currentSetting > 1000) {
			currentSetting = 1000;
		}
		else if (currentSetting < 0) {
			currentSetting = 0;
		}
		sensor1.setTolerance(currentSetting);
		sensor2.setTolerance(currentSetting);  //setting tolerance for both sensors at once for now, may add ability to set tolerance by sensor in the future.
		printFloat(((float)sensor1.getTolerance() / 10.0), true, 8, 1);
		lcd.print("%");
	}
	displayMode = 2;
	clearRightScreen();
}

int calculateHe(float s1, float s2) {
	int inferredHe;
	(sensor1.isActive()) ? inferredHe = (int)(((s1 - s2) / s1) * 100.0 + .5) : inferredHe = (int)(((20.9 - s2) / 20.9) * 100.0 + .5);
	return (inferredHe > 0) ? inferredHe : 0;
}

bool heInTolerance(int inferredHe, int tolerance) {
	if (!sensor2.isInTolerance() || (inferredHe > targetHe + sensor2.getTolerance()) || (inferredHe < targetHe - sensor2.getTolerance())) {
		return false;
	}
	return true;
}

//prints floats in a nicely formatted way so they don't jump around on the LCD screen
void printFloat(float floatToPrint, bool highlight, int column, int row) {
	String formattedValue = String(floatToPrint, 1);
	lcd.setCursor(column, row);
	if (highlight) {
		lcd.write(byte(2));
	}


	if (formattedValue.length() > 4) {
		formattedValue = formattedValue.substring(0, 3);
	}

	if (formattedValue.length() == 4) {
		lcd.print(formattedValue);
	}
	else {
		lcd.print(" ");
		lcd.print(formattedValue);
	}

	if (highlight) {
		lcd.setCursor(column + 5, row);
		lcd.write(byte(3));
	}
}

//prints ints in a nicely formatted way so they don't jump around on the LCD screen
void printInt(int intToPrint, bool highlight, int column, int row) {
	String formattedValue = String(intToPrint, DEC);

	lcd.setCursor(column, row);

	if (highlight) {
		lcd.write(byte(2));
	}

	if (formattedValue.length() == 2) {
		lcd.print(formattedValue);
	}
	else {
		lcd.print("0");
		lcd.print(formattedValue);
	}

	if (highlight) {
		lcd.setCursor(column + 3, row);
		lcd.write(byte(3));
	}

}

void clearRightScreen() {
	lcd.setCursor(7, 0);
	lcd.print("         ");
	lcd.setCursor(7, 1);
	lcd.print("         ");
}

//the first of two ISRs to detect pulses from the quadrature encoder
void aEncoderInterrupt() {
	aCurrentState = digitalRead(encoderPinA);
	bCurrentState = digitalRead(encoderPinB);

	if (aPreviousState && bPreviousState) {
		if (!aCurrentState && bCurrentState) {
			encoderTicks++;
		}
		if (aCurrentState && !bCurrentState) {
			encoderTicks--;
		}
	}
	else if (!aPreviousState && bPreviousState) {
		if (!aCurrentState && !bCurrentState) {
			encoderTicks++;
		}
		if (aCurrentState && bCurrentState) {
			encoderTicks--;
		}
	}
	else if (!aPreviousState && !bPreviousState) {
		if (aCurrentState && !bCurrentState) {
			encoderTicks++;
		}
		if (!aCurrentState && bCurrentState) {
			encoderTicks--;
		}
	}
	else if (aPreviousState && !bPreviousState) {
		if (aCurrentState && bCurrentState) {
			encoderTicks++;
		}
		if (!aCurrentState && !bCurrentState) {
			encoderTicks--;
		}
	}
	//the encoder I used has 4 pulses per detent, so I needed to add this if/else if statement to convert four pulses into a single adjustment point
	if (encoderTicks >= 4) {
		currentSetting++;
		encoderTicks = 0;
	}
	else if (encoderTicks <= -4) {
		currentSetting--;
		encoderTicks = 0;
	}
	aPreviousState = aCurrentState;
	bPreviousState = bCurrentState;
}

void bEncoderInterrupt() {
	aCurrentState = digitalRead(encoderPinA);
	bCurrentState = digitalRead(encoderPinB);
	if (aPreviousState && bPreviousState) {
		if (!aCurrentState && bCurrentState) {
			encoderTicks++;
		}
		if (aCurrentState && !bCurrentState) {
			encoderTicks--;
		}
	}
	else if (!aPreviousState && bPreviousState) {
		if (!aCurrentState && !bCurrentState) {
			encoderTicks++;
		}
		if (aCurrentState && bCurrentState) {
			encoderTicks--;
		}
	}
	else if (!aPreviousState && !bPreviousState) {
		if (aCurrentState && !bCurrentState) {
			encoderTicks++;
		}
		if (!aCurrentState && bCurrentState) {
			encoderTicks--;
		}
	}
	else if (aPreviousState && !bPreviousState) {
		if (aCurrentState && bCurrentState) {
			encoderTicks++;
		}
		if (!aCurrentState && !bCurrentState) {
			encoderTicks--;
		}
	}
	if (encoderTicks >= 4) {
		currentSetting++;
		encoderTicks = 0;
	}
	else if (encoderTicks <= -4) {
		currentSetting--;
		encoderTicks = 0;
	}
	aPreviousState = aCurrentState;
	bPreviousState = bCurrentState;
}
