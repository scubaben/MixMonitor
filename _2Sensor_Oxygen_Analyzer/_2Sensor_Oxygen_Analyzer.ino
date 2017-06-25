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

float o2MvFactor[2];
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

//use volatie variables when they get changed by an ISR (interrupt service routine)
volatile bool aCurrentState;
volatile bool bCurrentState;
volatile bool aPreviousState;
volatile bool bPreviousState;
volatile int currentSetting;
volatile int encoderTicks;

//handy character designer: https://www.quinapalus.com/hd44780udg.html
byte separatorChar[8]  = {B1010, B100, B1010, B100, B1010, B100, B1010
                         };
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
  lcd.createChar(0, separatorChar);
  ads1115.begin();  //start ADC object
  ads1115.setGain(GAIN_SIXTEEN); //set gain on ADC to +/-.256v to get the best resolution on the o2 millivolts

  //print a message at startup
  lcd.setCursor(3, 0);
  lcd.print("MixMonitor");
  lcd.setCursor(0, 1);
  lcd.print(getVoltage());
  lcd.print("v");
  lcd.setCursor(11, 1);
  lcd.print("v 0.1");
  delay(2000);


  //if the calibration button is down, run the calibrate routine, otherwise validate the calibration data
  if (digitalRead(buttonPin) == LOW) {
    calibrate();
  }
  else {
    for (int sensor = 0; sensor < 2; sensor++) {
      o2MvFactor[sensor] = getO2CalData(sensor);
      if (validateCalData(o2MvFactor[sensor])) {
        lcd.clear();
        lcd.print("Please");
        lcd.setCursor(0, 1);
        lcd.print("Recalibrate");
        delay(2000);
        calibrate();
      }
    }
  }
  lcd.clear();
}

void loop() {
  displayOxygen();
  displayTarget();
  if (buttonDetect(buttonPin)) {
    optionsMenu();
  }
}




//reads mv from oxygen sensor
float getO2Mv(int sensor) {
  if (sensor == 0) {
    return ads1115.readADC_Differential_0_1() * 256.0 / 32767.0; //read from ADC and convert to mv
  }
  if (sensor == 1) {
    return ads1115.readADC_Differential_2_3() * 256.0 / 32767.0; //read from ADC and convert to mv
  }
  return 0.0;
}


//saves calibration data to EEPROM
void setCalData(int sensor, float savedFactor) {
  int eeAddress = sensor * sizeof(float);
  EEPROM.put(eeAddress, savedFactor);
}

//reads O2 calibration data from EEPROM
float getO2CalData(int sensor) {
  int eeAddress = sensor * sizeof(float);
  float savedFactor;
  EEPROM.get(eeAddress, savedFactor);
  return savedFactor;
}

//validates o2Mv calibration to ensure they are within logical limits
int validateCalData(float validateFactor) {
  if ((validateFactor > 1.615 && validateFactor < 2.625) || validateFactor == 0.0) {
    return 0;
  }
  else {
    return 1;
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
  float oxygen;
  if ((millis() - lastSampleMillis) > sampleRate) {
    //loop through each sensor and calculate the o2 reading, but don't print if o2% is less than .9, to avoid displaying bad readings
    for (int sensor = 0; sensor < 2; sensor++) {
      oxygen = (getO2Mv(sensor) * o2MvFactor[sensor]);
      if (o2MvFactor[sensor] == 0.0 || oxygen < 0.9) {
        lcd.setCursor(0, sensor);
        lcd.print("     ");
      }
      else {
        printFloat(oxygen, 0, sensor);
        lcd.print("%");
      }
      withinTolerance[sensor] = checkTolerance(oxygen, sensor);
    }
    lcd.setCursor(6, 0);
    lcd.write(byte(0));
    lcd.setCursor(6, 1);
    lcd.write(byte(0));
    lastSampleMillis = millis();
  }



}

void displayTarget() {
  if (displayMode == 2) {
    lcd.setCursor(7, 0);
    lcd.print("Tgt: ");
    lcd.setCursor(7, 1);
    lcd.print("Tgt: ");
    for (int sensor = 0; sensor < 2; sensor ++) {
      printFloat((float) targetOx[sensor] / 10.0, 12, sensor);
    }
  }
  if (displayMode == 2) {
    if (!withinTolerance[0] || !withinTolerance[1]) {
      digitalWrite(ledPin, HIGH);
      digitalWrite(outPin, HIGH);
    }
    else {
      digitalWrite(ledPin, LOW);
      digitalWrite(outPin, LOW);
    }
  }
}


float readOxygenSensor(int sensor) {
  return (getO2Mv(sensor) * o2MvFactor[sensor]);
}

bool checkTolerance(float oxygen, int sensor) {
  if ((o2MvFactor[sensor] != 0.0 && oxygen > 0.9) && (((((int)(oxygen * 10.0)) - targetOx[sensor]) > targetTolerance) || ((targetOx[sensor] - ((int)(oxygen * 10.0))) > targetTolerance))) {
    return false;
  }

  else {
    return true;
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
  while (((millis() - lastDisplayMillis) < 1750)) {
    displayOxygen();
  }
  clearRightScreen();
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
  while (buttonDetect(buttonPin) == false) {
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

  //display the sensor mv until the button is clicked
  lcd.clear();
  lcd.print("S1:");
  lcd.setCursor(0, 1);
  lcd.print("S2:");
  do {
    for (int sensor = 0; sensor < 2; sensor++) {
      o2Mv = getO2Mv(sensor);  //read mv from O2 sensor

      if (o2Mv < 0.1) {
        lcd.setCursor(3, sensor);
        lcd.print("    ");
      }
      else {
        printFloat(o2Mv, 3, sensor);
      }
      lcd.print("mv");
    }

  } while (!buttonDetect(buttonPin));

  //validate the calData and then save to eeprom if it is good.
  for (int sensor = 0; sensor < 2; sensor++) {
    o2MvFactor[sensor] = calibrationPoint / getO2Mv(sensor);
    if (validateCalData(o2MvFactor[sensor]) == 0.0) {
      setCalData(sensor, o2MvFactor[sensor]);
    }

    else if (getO2Mv(sensor) < 0.1) {
      setCalData(sensor, 0.0);
    }

    else {
      lcd.clear();
      lcd.print("Bad Calibration");
      lcd.setCursor(0, 1);
      lcd.print("Data");
      delay(5000);
      calibrate();
    }
  }

  lcd.clear();
  lcd.print("Calibration");
  lcd.setCursor(0, 1);
  lcd.print("Saved");

  //read the calData before returning to the main loop
  for (int sensor = 0; sensor < 2; sensor++) {
    o2MvFactor[sensor] = getO2CalData(sensor);
  }

  delay(1500);
  lcd.clear();

}

void setMixTarget() {
  displayMode = 1;
  //add code for setting mixes
}


float setSensorTargets() {

  for (int sensor = 0; sensor < 2; sensor++) {
    lcd.setCursor(7, 0);
    lcd.print("S");
    lcd.print(sensor + 1);
    lcd.print(" Target");
    currentSetting = targetOx[sensor];

    while (buttonDetect(buttonPin) == false) {
      displayOxygen();
      if (currentSetting > 1000) {
        currentSetting = 1000;
      }
      else if (currentSetting < 0) {
        currentSetting = 0;
      }
      targetOx[sensor] = currentSetting;
      printFloat(((float) targetOx[sensor] / 10.0), 7, 1);
      lcd.print("% O2");
    }
  }
  clearRightScreen();
  lcd.setCursor(7, 0);

  lcd.print("Tolerance");
  currentSetting = targetTolerance;
  while (buttonDetect(buttonPin) == false) {
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
