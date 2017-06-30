//a two sensor oxygen analyzer by ben shiner
//minimum parts list:
//Arduino or compatible (I used the Adafruit Feather 32u4 proto board)
//ADS1115 (also available from Adafruit)
//Quadrature encoder
//led + resistor

/*Copyright (c) 2017 Ben Shiner

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
#include "Adafruit_ADS1015.h" //16-bit ADC library, see here: https://github.com/adafruit/Adafruit_ADS1X15
#include <LiquidCrystal.h> //LCD library
#include <EEPROM.h> //eeprom library for saving calibration data

#define buttonPin A3
#define encoderPinA 0
#define encoderPinB 1
#define ledPin A5
#define outPin A4
#define batteryPin A9

LiquidCrystal lcd(13, 12, 11, 10, 6, 5); //create LCD object, these pins are the ones i chose to use on the adafruit feather 32u4 proto board
Adafruit_ADS1115 ads1115;  //create ADC object

int buttonState;
int lastButtonState = HIGH;
unsigned long lastSampleMillis = millis();
unsigned long lastDisplayMillis = millis();
unsigned long sampleRate = 400;
unsigned long debounceMillis = 0;
unsigned long debounceDelay = 50;
int targetOx[2] = {209, 209}; //Floats don't do comparison well, so I'm using ints for oxygen % and the tolerance, and then dividing by 10 where necessary
int targetTolerance = 15;
int displayMode = 0;
boolean withinTolerance[2] = {true, true};
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
byte thickSeparator[8]  = {B1010, B100, B1010, B100, B1010, B100, B1010};
byte thinSeparator[8] = {B100, B0, B100, B0, B100, B0, B100};
byte targetSymbol[8] = {B0, B1000, B1100, B1110, B1100, B1000, B0};

class Sensor {
    int sensorIndex;
    int target = 209;
    boolean calibrationLoaded = false;
    float savedFactor = 0.0;
  public:
    Sensor(int sensorNumber) {
      sensorIndex = sensorNumber;
    }

    boolean isConnected() {
      if (this->mv() > 0.0) {
        return true;
      }
      return false;
    }

    boolean isCalibrated() {
      if (this->factor() > 0.0) {
        return true;
      }
      return false;
    }

    boolean isInTolerance() {
      if (this->isCalibrated() && this->isConnected()) {
        if ((this->oxygenContent() > (float) this->target / 10.0 + (float)targetTolerance / 10.0) || (this->oxygenContent() < (float) this->target / 10.0 - (float)targetTolerance / 10.0)) {
          return false;
        }
        return true;
      }
      return true;
    }

    float factor() {
      int eeAddress = sensorIndex * sizeof(float);
      if (!this->calibrationLoaded) {
        EEPROM.get(eeAddress, this->savedFactor);
        this->calibrationLoaded = true;
        if (this->savedFactor < 1.615 || this->savedFactor > 2.625) {
          this->savedFactor = 0.0;
        }
      }
      return this->savedFactor;
    }

    float mv() {
      if (sensorIndex == 0) {
        return ads1115.readADC_Differential_0_1() * 256.0 / 32767.0; //read from ADC and convert to mv
      }
      if (sensorIndex == 1) {
        return ads1115.readADC_Differential_2_3() * 256.0 / 32767.0; //read from ADC and convert to mv
      }
      return 0.0;
    }

    float oxygenContent() {
      if (this->isCalibrated() && this->isConnected() && this->mv() > 0.0) {
        return  this->mv() * this->factor();
      }
      return 0.0;
    }

    void saveCalibration(float calData) {
      int eeAddress = sensorIndex * sizeof(float);
      EEPROM.put(eeAddress, calData);
    }

    void setTarget(int target) {
      this->target = target;
    }

    int getTarget() {
      return this->target;
    }
};

Sensor sensor1(0);
Sensor sensor2(1);
void setup() {


  //set pin modes, use pullup resistors on the input pins to help filter out noise
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(encoderPinA, INPUT_PULLUP);
  pinMode(encoderPinB, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(outPin, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(encoderPinA), aEncoderInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderPinB), bEncoderInterrupt, CHANGE);


  lcd.begin(16, 2); //configure columns and rows for 16x2 lcd
  lcd.createChar(0, thickSeparator);
  lcd.createChar(1, thinSeparator);
  lcd.createChar(2, targetSymbol);
  ads1115.begin();  //start ADC object
  ads1115.setGain(GAIN_SIXTEEN); //set gain on ADC to +/-.256v to get the best resolution on the o2 millivolts

  //print a message at startup
  lcd.setCursor(3, 0);
  lcd.print("MixMonitor");
  lcd.setCursor(0, 1);
  lcd.print(getVoltage());
  lcd.print("v");
  lcd.setCursor(11, 1);
  lcd.print("v 0.2");
  delay(2000);
  lcd.clear();


  //if the calibration button is down, run the calibrate routine, otherwise validate the calibration data
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
  float batteryVoltage = analogRead(batteryPin) * 2.0 * 3.3 / 1024;
  return batteryVoltage;
}

void displayOxygen() {
  if ((millis() - lastSampleMillis) > sampleRate) {
    if (sensor1.oxygenContent() > 0.0) {
      printFloat(sensor1.oxygenContent(), 0, 0);
      lcd.print("%");
    }
    else {
      lcd.setCursor(0, 0);
      lcd.print("     ");
    }
    if (sensor2.oxygenContent() > 0.0) {
      printFloat(sensor2.oxygenContent(), 0, 1);
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
      if (!sensor1.isConnected() || !sensor1.isCalibrated()  || sensor1.mv() <= 0.0) {
        lcd.setCursor(7, 0);
        lcd.print("         ");
      }
      else {
        lcd.setCursor(7, 0);
        lcd.write(byte(2));
        printFloat(sensor1.mv(), 8, 0);
        lcd.print(" mV");
      }
      if (!sensor2.isConnected() || !sensor2.isCalibrated() || sensor2.mv() <= 0.0) {
        lcd.setCursor(7, 1);
        lcd.print("         ");
      }
      else {
        lcd.setCursor(7, 1);
        lcd.write(byte(2));
        printFloat(sensor2.mv(), 8, 1);
        lcd.print(" mV");
      }
    }
    if (displayMode == 1) {
      //needs error handling for when only 1 sensor is connected, and tolerance checking.
              lcd.setCursor(7, 0);
      if (sensor1.isConnected() && sensor1.isCalibrated()) {
        lcd.write(byte(2));
        printInt((sensor1.getTarget() / 10), 8, 0);
      }
      else {
        lcd.print("   ");
      }

      lcd.setCursor(12, 0);
      lcd.print("Mix");
      lcd.setCursor(7, 1);
      if (sensor2.isConnected() && sensor2.isCalibrated()) {
        lcd.write(byte(2));
        printInt((sensor2.getTarget() / 10), 8, 1);
      }
      else {
        lcd.print("   ");
      }

      printInt((int)(sensor2.oxygenContent() + 0.5), 11, 1);
      lcd.print("/");
      printInt((int)(((sensor1.oxygenContent() - sensor2.oxygenContent() / sensor1.oxygenContent()) * 100.0) + .5), 14, 1);

    }
    if (displayMode == 2) {
      lcd.setCursor(7, 0);
      lcd.write(byte(2));
      lcd.setCursor(7, 1);
      lcd.write(byte(2));
      if (sensor1.isConnected() && sensor1.isCalibrated()) {
        printFloat((float)sensor1.getTarget() / 10.0, 8, 0);
      }
      if (sensor2.isConnected() && sensor2.isCalibrated()) {
        printFloat((float)sensor2.getTarget() / 10.0, 8, 1);
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
  }
  updateRightDisplay = false;
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
  while (((millis() - lastDisplayMillis) < 1750 && currentSetting == 0)) {
    displayOxygen();
  }
  clearRightScreen();
  currentSetting = 0;
  while (!exitOptionsMenu) {
    displayOxygen();
    if (currentSetting > 4) {
      currentSetting = 0;
    }
    if (currentSetting < 0) {
      currentSetting = 4;
    }
    if (currentSetting != lastMenuSelection) {
      clearRightScreen();
      lastMenuSelection = currentSetting;
    }

    switch (currentSetting) {
      case 0:
        lcd.setCursor(7, 0);
        lcd.print("Calibrate");
        if (buttonDetect(buttonPin)) {
          exitOptionsMenu = true;
          calibrate();
        }
        break;
      case 1:
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
      case 2:
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
      case 3:
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
  float o2Mv;
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
    calibrationPoint = (float) currentSetting / 10.0;
    printFloat(calibrationPoint, 0, 1);
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
      printFloat(sensor1.mv(), 0, 0);
    }
    lcd.print("mV");
    if (sensor2.mv() <= 0.0) {
      lcd.setCursor(0, 1);
      lcd.print("    ");
    }
    else {
      printFloat(sensor2.mv(), 0, 1);
    }
    lcd.print("mV");


  } while (!buttonDetect(buttonPin));

  if (sensor1.isConnected() && ((calibrationPoint / sensor1.mv() > 1.615 && calibrationPoint / sensor1.mv() < 2.625))) {
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
  if (sensor2.isConnected() && ((calibrationPoint / sensor2.mv() > 1.615 && calibrationPoint / sensor2.mv() < 2.625))) {
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

  lcd.clear();
  lcd.print("Calibration");
  lcd.setCursor(0, 1);
  lcd.print("Saved");

  delay(1500);
  lcd.clear();

}

//update this logic to include more validations, ie s1 can't be less than 21. also make it clearer which #s you are updating by flashing or something...
void setMixTarget() {
  displayMode = 1;
  lcd.setCursor(7, 0);
  lcd.print("Tgt. Mix:");
  currentSetting = 21; //when setting the target mix, we'll use whole percentages instead of tenths.
  while (!buttonDetect(buttonPin)) {
    displayOxygen();
    if (currentSetting > 99) {
      currentSetting = 99;
    }
    else if (currentSetting < 0) {
      currentSetting = 0;
    }
    sensor2.setTarget(currentSetting * 10);
    printInt(currentSetting, 7, 1);
    lcd.print("/00");
  }

  currentSetting = 0;
  while (!buttonDetect(buttonPin)) {

    displayOxygen();
    if (currentSetting + (sensor2.getTarget() / 10) > 99) {
      currentSetting = 100 - (sensor2.getTarget() / 10);
    }
    else if (currentSetting < 0) {
      currentSetting = 0;
    }
    sensor1.setTarget(((float) sensor2.getTarget() / 10.0) / (100.0 - (float)currentSetting) * 1000); // this sets the target for s1, the formula is: s1 = s2/1-he
    targetHe = currentSetting;  // this sets the target HE content
    printInt(targetHe, 10, 1);
  }
  clearRightScreen();
}




float setSensorTargets() {
  if (sensor1.isConnected() && sensor1.isCalibrated()) {
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
      printFloat(((float) sensor1.getTarget() / 10.0), 7, 1);
      lcd.print("% O2");
    }
    clearRightScreen();

  }
  if (sensor2.isConnected() && sensor2.isCalibrated()) {

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
      printFloat(((float) sensor2.getTarget() / 10.0), 7, 1);
      lcd.print("% O2");
    }
    clearRightScreen();
  }
  lcd.setCursor(7, 0);

  lcd.print("Tolerance");
  currentSetting = targetTolerance;
  while (!buttonDetect(buttonPin)) {
    displayOxygen();
    if (currentSetting > 1000) {
      currentSetting = 1000;
    }
    else if (currentSetting < 0) {
      currentSetting = 0;
    }
    targetTolerance = currentSetting;
    printFloat(((float)targetTolerance / 10.0), 7, 1);
    lcd.print(" pts.");
  }
  displayMode = 2;
  lcd.setCursor(7, 0);
  lcd.print("         ");
  lcd.setCursor(7, 1);
  lcd.print("         ");

}


//prints floats in a nicely formatted way so they don't jump around on the LCD screen
void printFloat(float floatToPrint, int column, int row) {
  String formattedValue = String(floatToPrint, 1);

  if (formattedValue.length() > 4) {
    formattedValue = formattedValue.substring(0, 3);
  }
  lcd.setCursor(column, row);

  if (formattedValue.length() == 4) {
    lcd.print(formattedValue);
  }
  else {
    lcd.print(" ");
    lcd.print(formattedValue);
  }
}

//prints ints in a nicely formatted way so they don't jump around on the LCD screen
void printInt(int intToPrint, int column, int row) {
  String formattedValue = String(intToPrint, DEC);
  lcd.setCursor(column, row);

  if (formattedValue.length() == 2) {
    lcd.print(formattedValue);
  }
  else {
    lcd.print("0");
    lcd.print(formattedValue);
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
  } else if (!aPreviousState && bPreviousState) {
    if (!aCurrentState && !bCurrentState) {
      encoderTicks++;
    }
    if (aCurrentState && bCurrentState) {
      encoderTicks--;
    }
  } else if (!aPreviousState && !bPreviousState) {
    if (aCurrentState && !bCurrentState) {
      encoderTicks++;
    }
    if (!aCurrentState && bCurrentState) {
      encoderTicks--;
    }
  } else if (aPreviousState && !bPreviousState) {
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
  } else if (!aPreviousState && bPreviousState) {
    if (!aCurrentState && !bCurrentState) {
      encoderTicks++;
    }
    if (aCurrentState && bCurrentState) {
      encoderTicks--;
    }
  } else if (!aPreviousState && !bPreviousState) {
    if (aCurrentState && !bCurrentState) {
      encoderTicks++;
    }
    if (!aCurrentState && bCurrentState) {
      encoderTicks--;
    }
  } else if (aPreviousState && !bPreviousState) {
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
