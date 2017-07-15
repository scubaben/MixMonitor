# MixMonitor
an arduino based program to read from two oxygen sensors.

I'm trying out visual micro as an IDE and it has added a bunch of extra files, if you are using the arduino IDE you just need the .ino file.

This program is designed to read from two oxygen sensors on a trimix stick, but can optionally be used with a single sensor (see notes  below).  To use this analyzer in the way it was designed your trimix stick should be setup to inject oxygen first, followed by sensor 1, then inject helium followed by sensor two, like so:

Fresh Air Intake -> Oxygen Injection -> Sensor 1 (S1) -> Helium Injection -> Sensor 2 (S2) -> Compressor Inlet

When using a single sensor, plug it into S2 on the analyzer.

The analyzer has 3 modes, and a calibration routine:
- Standard 
- Mix target
- Sensor Target

The analyzer will always startup in standard mode.  In this mode the analyzer will show you the O2 reading, and mV for any connected and calibrated cells.  If you have cells that are connected but not calibrated, it will ask you to calibrate on startup.  Optionally you can enter the calibration routine through the menu, or by holding down the select button at startup.

In mix target mode you can set your desired mix, and a tolerance and the analyzer will use the two connected sensors to read the level of oxygen and infer the level of helium in the mix.  When using a single sensor, the analyzer will assume that there is no Oxygen injection and the mix before the helium is injected will be air.  

If the inferred mix goes outside of your target the red LED will light and the OUT pin will go high.  The formula used to infer the helium content is (S1-S2)/S1, as an example if you were blending 10/50 - your S1 reading would be .21 (AIR) and your S2 reading would be .10, so (.21-.10)/.21 = .52, or a theoretical helium content of 52%.  This is not a substitute to analyzing what actually ends up in your tanks, and you must analyze your gas after you have filled.  Do not rely on this analyzer for life support purposes.

NOTE: The mix target function is in beta, use at your own risk please be familiar with the risks of running elevated oxygen levels through a compressor and ensure you are taking adequate precautions and appropriate safety measures.  If you plan on running over .21 FO2, please have a redundant analyzer to ensure that your readings are accurate - this code may have errors and produce inaccurate results.

In sensor target mode you can set your desired O2 targets for S1 and S2 individually, as with the Mix Target mode the LED and Out Pin will activate if the mix drifts outside the specified target/tolerance.

To access the menu click down on the select button, use the knob to scroll through the menu and set your targets, tolerance, calibration FO2 etc...

Please see the license for permissions and limitations of the use of this software.
