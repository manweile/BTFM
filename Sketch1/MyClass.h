/*
 * MyClass.h
 *
 * Created: 2/17/2024 4:48:38 PM
 * Author: Gerald
 */

#ifndef _MYCLASS_h
#define _MYCLASS_h

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

