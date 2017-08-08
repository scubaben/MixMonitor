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

#include <Adafruit_ADS1015.h>

class Sensor {
  protected:
    int sensorIndex;
    int target = 209;
    int tolerance = 15;
    boolean calibrationLoaded = false;
    float savedFactor = 0.0;
    Adafruit_ADS1115 _adc;

  public:
    Sensor() {};
    Sensor(int);
    bool isConnected();
    bool isCalibrated();
    bool isActive();
    bool isInTolerance();
    float factor();
    float mv();
    float oxygenContent();
    void saveCalibration(float);
    void setTarget(int);
    int getTarget();
    void setTolerance(int);
    int getTolerance();
};
