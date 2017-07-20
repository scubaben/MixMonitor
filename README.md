# MixMonitor
Arduino based two-sensor oxygen analyzer designed for continuous blending.

## Introduction
This application uses an Adafruit Feather to read from two oxygen sensors positioned in the intake stream of a compressor and calculates the inferred helium percentage using the difference between the two sensor readings. It is also possible to use this system with a single sensor, see the notes below for details.  The quadrature encoder (spinny clicky knob) is used to enter the menu, navigate, adjust settings, and make selections.  Click the select button to enter the menu, scroll the knob left and right to navigate and click down to make a selection.

The left side of the screen will always display the readings from any connected and calibrated sensors, the right side of the screen will display either mV, sensor targets and/or inferred mix depending on the mode selected.

*NOTE: Feeding your compressor enriched air with an oxygen content greater than 21% poses significant risk, this software is provided without warranty and it may contain defects that could result in inaccurate sensor readings, it is your responsibility to take proper precautions to ensure your safety.*

*NOTE: This type of analysis is not a substitute to analyzing what actually ends up in your tanks, and you must analyze your gas after you have filled.*

## Setup
This analyzer is designed to be connected to two sensors on a nitrox / trimix blending stick.  Your stick should be setup to inject oxygen first, followed by sensor 1, then inject helium followed by sensor two, like so:

Fresh Air Intake -> Oxygen Injection -> Sensor 1 (S1) -> Helium Injection -> Sensor 2 (S2) -> Compressor Inlet

## Calibration
At startup the analyzer checks the calibration data in EEPROM.  If the data invalid or not found it will launch the calibration routine and allow you to calibrate the sensors.

Follow these steps to Calibrate:
1) Enter calibration mode
- Hold down the select button at startup *OR*
- Click the select button to enter the menu, scroll to the 'Calibration' option, and click to select.
2) Scroll left or right to choose your desired calibration FO2 and click to select.
3) The mV readings for any connected cells will be displayed on the screen, once they stabilize click to save the calibration data.

*NOTE: This analyzer must be used with sensors that read between 8-13mV in air.*

## Continuous Blending
You can use this sensor in any number of ways, but its main use case is for continuous blending.  The analyzer has three different modes, described below.
#### Standard Mode
The analyzer will always startup in Standard Mode.  In this mode it will display the oxygen content read by any connected and calibrated sensors, along with the mV of each sensor.  You can return to Standard Mode by selecting the 'Disable Targets' menu option.

#### Mix Target Mode
In Mix Target Mode the analyzer will allow you to specify the oxygen and helium content of your desired mix and calculate the target FO2 for S1 and S2 to create that mix.  It will also calculate and display the inferred mix based on S1 and S2 readings.

Follow these steps to use Mix Target Mode:
1) Click the select button and scroll to the 'Mix Target' option, click to select
2) Scroll to specify the Oxygen content of the target mix and click to select
3) Scroll to specify the Helium content of the target mix and click to select
4) Scroll to specify the tolerance in percentage points and click to select (see Alarms section below for further details on tolerance)

The right side of the screen will update to display the FO2 target to the right of each sensor reading, and the current inferred mix calculated from the current S1 and S2 readings.

*NOTE: The mix target function is in beta, use at your own risk please be familiar with the risks of running elevated oxygen levels through a compressor and ensure you are taking adequate precautions and appropriate safety measures.  It is your responsibility to ensure your intake FO2 remains within safe levels.*

*NOTE: If you are blending mixes where it is only necessary to add helium you can use this analyzer in single-sensor moder by connecting your sensor to S2.  In this configuration the analyzer will assume that the S1 reading is AIR and calculate the inferred mix based on an assumed S1 reading of .209*

#### Sensor Target Mode
In Sensor Target Mode the analzyer will allow you to directly specify the target FO2 for S1 and S2.  I didn't have a particular use case in mind for this mode, but I wanted to give you flexibility in how you use this analyzer.  This should cover most scenarios that are not already addressed by the Standard Mode or Mix Target Mode.

Follow these steps to use Sensor Target Mode:
1) Click the select button and scroll to the 'Sensor Target' option, click to select
2) Scroll to specify the Oxygen content target for S1 and click to select
3) Scroll to specify the Oxygen content target for S2 and click to select
4) Scroll to specify the tolerance in percentage points and click to select (see Alarms section below for further details on tolerance)

The right side of the screen will update to display the FO2 target to the right of each sensor reading.

#### Alarms
When using Mix Target or Sensor Target Modes you will specify a tolerance.  The analyzer will calculate the difference between the sensor readings, and the target and if the difference is greater than the tolerance specified it will light up the LED and set the OUT pin to HIGH.

## Parts List


##### Please see the license for permissions and limitations of the use of this software.
*Note: I'm trying out visual micro as an IDE and it has added a bunch of extra files, if you are using the arduino IDE you just need the .ino file.*
