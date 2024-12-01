/*
 * MyClass.h
 *
 * Created: 2/17/2024 4:48:38 PM
 * Author: Gerald
 */

#ifndef _MYCLASS_h
#define _MYCLASS_h

// Arduino Builtin
#include <SPI.h>
#include <Wire.h>

// Arduino Optional
#include <ArduinoSTL.h>
#include <algorithm>
#include <vector>

// Third Party Hardware
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>
#include <Adafruit_Si4713.h>

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

class MyClass
{
 private:


 public:
	void setup();
	void loop();
};

extern MyClass myClass;

#endif

