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


//include necessary libraries
#include <Wire.h> // wiring library
#include "Adafruit_ADS1015.h" //16-bit ADC library, see here: https://github.com/adafruit/Adafruit_ADS1X15
#include <LiquidCrystal.h> //LCD library
#include <EEPROM.h> //eeprom library for saving calibration data


//define  pins
#define buttonPin A3
#define encoderPinA 0
#define encoderPinB 1
#define ledPin A5
#define buzzerPin A4

//create objects
LiquidCrystal lcd(13, 12, 11, 10, 6, 5); //create LCD object, these pins are the ones i chose to use on the adafruit feather 32u4 proto board
Adafruit_ADS1115 ads1115;  //create ADC object

//global variables 
float o2Mv;
float o2MvFactor[2];
int sensor = 0;
int buttonState;
int lastButtonState = HIGH;
int sampleRate = 400;
unsigned long lastMillis = millis();
unsigned long debounceMillis = 0;
unsigned long debounceDelay = 50;
int targetOx[2] = {209, 209}; //Floats don't do comparison well, so I'm using ints for oxygen % and the tolerance, and then dividing by 10 where necessary
int targetTolerance = 15;
bool outOfTolerance[2] = {false, false};

//use volatie variables when they get changed by an ISR (interrupt service routine)
volatile bool aCurrentState;
volatile bool bCurrentState;
volatile bool aPreviousState;
volatile bool bPreviousState;
volatile int currentSetting;
volatile int encoderTicks;

void setup() {

  //set pin modes, use pullup resistors on the input pins to help filter out noise
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(encoderPinA, INPUT_PULLUP);
  pinMode(encoderPinB, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(encoderPinA), aEncoderInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderPinB), bEncoderInterrupt, CHANGE);


  lcd.begin(16, 2); //configure columns and rows for 16x2 lcd
  ads1115.begin();  //start ADC object
  ads1115.setGain(GAIN_SIXTEEN); //set gain on ADC to +/-.256v to get the best resolution on the o2 millivolts

//print a message at startup
lcd.setCursor(3,0);
  lcd.print("MixMonitor");
  lcd.setCursor(11, 1);
  lcd.print("v 0.1");
  delay(2000);


  //if the calibration button is down, run the calibrate routine
  if (digitalRead(buttonPin) == LOW) {
    calibrate();
  }

  //validate the calibration data at startup
  else {
    for (sensor = 0; sensor < 2; sensor++) {
      o2MvFactor[sensor] = getO2CalData(sensor);
      if (validateCalData(o2MvFactor[sensor])) {
        lcd.clear();
        lcd.print("Please");
        lcd.setCursor(0, 1);
        lcd.print("Recalibrate");
        calibrate();
      }
    }
  }
  lcd.clear();
}

void loop() {
  float oxygen;

//use millis() and an if statement to update the display at a given rate, so it doesn't flicker - 400 ms seems to work well
  if ((millis() - lastMillis) > sampleRate) {
    lcd.setCursor(0, 0);
    lcd.print("S1 O2% ");
    lcd.setCursor(0, 1);
    lcd.print("S2 O2% ");

//loop through each sensor and calculate the o2 reading, but don't print if o2% is less than .9, to avoid displaying bad readings
    for (sensor = 0; sensor < 2; sensor++) {
      oxygen = (getO2Mv(sensor) * o2MvFactor[sensor]);
      lcd.setCursor(7, sensor);
      if (o2MvFactor[sensor] == 0.0 || oxygen < 0.9) {
        lcd.print("     ");
      }
      else {
        printFloat(oxygen, 7, sensor);
      }

//compare the o2 reading for each sensor to the target and the tolerance, print ++ if it is too high, print -- if it is too low.  NOTE this could potentially cause a zero or near zero reading to not be displayed, and for the alert to not be triggered.
      if ((o2MvFactor[sensor] != 0.0 && oxygen > 0.9) && ((((int)(oxygen * 10.0)) - targetOx[sensor]) > targetTolerance)) {
        lcd.print(" ++");
        outOfTolerance[sensor] = true;

      }
      else if ((o2MvFactor[sensor] != 0.0 && oxygen > 0.9) && ((targetOx[sensor] - ((int)(oxygen * 10.0))) > targetTolerance)) {
        lcd.print(" --");
        outOfTolerance[sensor] = true;

      }
      else {
        outOfTolerance[sensor] = false;
        lcd.print("   ");
      }
    }
    lastMillis = millis();
  }

//if either one of the sensora are outside of tolerance then light up the led
  if (outOfTolerance[0] == true || outOfTolerance[1] == true) {
    digitalWrite(ledPin, HIGH);
    digitalWrite(buzzerPin, HIGH);
  }

  else if (outOfTolerance[0] == false && outOfTolerance[1] == false) {
    digitalWrite(ledPin, LOW);
    digitalWrite(buzzerPin, LOW);
  }

  if (buttonDetect(buttonPin) == true) {
    targetSettings();
  }

}

//calibrate
void calibrate() {
  float calibrationPoint;
  lcd.clear();
  lcd.setCursor(4,0);
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
    calibrationPoint = (float) currentSetting / 10.0;
    printFloat(calibrationPoint, 0, 1);
    lcd.print("% Oxygen");
  }

//display the sensor mv until the button is clicked
  lcd.clear();
  lcd.print("S1: ");
  lcd.setCursor(0, 1);
  lcd.print("S2: ");
  do {
    for (sensor = 0; sensor < 2; sensor++) {
      o2Mv = getO2Mv(sensor);  //read mv from O2 sensor
      lcd.setCursor(4, sensor);
      if (o2Mv < 0.1) {
        lcd.print("      ");
      }
      else {
        printFloat(o2Mv, 4, sensor);
      }
      lcd.print("mv");
    }

  } while (buttonDetect(buttonPin) == false);

//validate the calData and then save to eeprom if it is good.
  for (sensor = 0; sensor < 2; sensor++) {
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
  for (sensor = 0; sensor < 2; sensor++) {
    o2MvFactor[sensor] = getO2CalData(sensor);
  }

  delay(1500);
  lcd.clear();

}


//reads mv from oxygen sensor
float getO2Mv(int sensor) {
  if (sensor == 0) {
    o2Mv = ads1115.readADC_Differential_0_1() * 256.0 / 32767.0; //read from ADC and convert to mv
  }
  if (sensor == 1) {
    o2Mv = ads1115.readADC_Differential_2_3() * 256.0 / 32767.0; //read from ADC and convert to mv
  }
  return o2Mv;
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

//if the button is pressed while running through the main loop it will allow the user to specify targets for each o2 sensor
float targetSettings() {
  lcd.clear();
  lcd.print("    Entering");
  lcd.setCursor(0, 1);
  lcd.print("    Settings");
  delay(1500);


  for (sensor = 0; sensor < 2; sensor++) {
    lcd.clear();
    lcd.print("S");
    lcd.print(sensor + 1);
    lcd.print(" Target FO2");
    currentSetting = targetOx[sensor];

    while (buttonDetect(buttonPin) == false) {
      targetOx[sensor] = currentSetting;
      printFloat(((float) targetOx[sensor] / 10.0), 0, 1);
      lcd.print("% Oxygen");
    }
  }

  lcd.clear();

  lcd.print("FO2 Tolerance:");
  currentSetting = targetTolerance;
  while (buttonDetect(buttonPin) == false) {
    targetTolerance = currentSetting;
    printFloat(((float)targetTolerance / 10.0), 0, 1);
  }
  lcd.clear();

}

//prints floats in a nicely formatted way so they don't jump around on the LCD screen
void printFloat(float floatToPrint, int column, int row) {
  lcd.setCursor(column, row);
  if (floatToPrint > 99.99) {
    lcd.print(floatToPrint);
  }
  else if (floatToPrint > 9.99) {
    lcd.print(" ");
    lcd.print(floatToPrint);
  }
  else {
    lcd.print("  ");
    lcd.print(floatToPrint);
  }


}

//the first of two ISRs to detect pulses from the quadrature encoder
void aEncoderInterrupt() {
  noInterrupts();
  aCurrentState = digitalRead(encoderPinA);
  bCurrentState = digitalRead(encoderPinB);

  if (aPreviousState && bPreviousState) {
    if (!aCurrentState && bCurrentState && currentSetting < 1000) {
      encoderTicks++;
    }
    if (aCurrentState && !bCurrentState && currentSetting > 0) {
      encoderTicks--;
    }
  } else if (!aPreviousState && bPreviousState) {
    if (!aCurrentState && !bCurrentState && currentSetting < 1000) {
      encoderTicks++;
    }
    if (aCurrentState && bCurrentState && currentSetting > 0) {
      encoderTicks--;
    }
  } else if (!aPreviousState && !bPreviousState) {
    if (aCurrentState && !bCurrentState && currentSetting < 1000) {
      encoderTicks++;
    }
    if (!aCurrentState && bCurrentState && currentSetting > 0) {
      encoderTicks--;
    }
  } else if (aPreviousState && !bPreviousState) {
    if (aCurrentState && bCurrentState && currentSetting < 1000) {
      encoderTicks++;
    }
    if (!aCurrentState && !bCurrentState && currentSetting > 0) {
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
  interrupts();

}

void bEncoderInterrupt() {
  noInterrupts();
  aCurrentState = digitalRead(encoderPinA);
  bCurrentState = digitalRead(encoderPinB);
  if (aPreviousState && bPreviousState) {
    if (!aCurrentState && bCurrentState && currentSetting < 1000) {
      encoderTicks++;
    }
    if (aCurrentState && !bCurrentState && currentSetting > 0) {
      encoderTicks--;
    }
  } else if (!aPreviousState && bPreviousState) {
    if (!aCurrentState && !bCurrentState && currentSetting < 1000) {
      encoderTicks++;
    }
    if (aCurrentState && bCurrentState && currentSetting > 0) {
      encoderTicks--;
    }
  } else if (!aPreviousState && !bPreviousState) {
    if (aCurrentState && !bCurrentState && currentSetting < 1000) {
      encoderTicks++;
    }
    if (!aCurrentState && bCurrentState && currentSetting > 0) {
      encoderTicks--;
    }
  } else if (aPreviousState && !bPreviousState) {
    if (aCurrentState && bCurrentState && currentSetting < 1000) {
      encoderTicks++;
    }
    if (!aCurrentState && !bCurrentState && currentSetting > 0) {
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
  interrupts();

}


