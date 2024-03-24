/*
 * MyClass.cpp
 *
 * Created: 2/17/2024 4:48:38 PM
 * Author: Gerald
 */

// Project
#include "MyClass.h"

// Arduino
#include <ArduinoSTL.h>
#include <algorithm>
#include <vector>
#include <SPI.h>
#include <Wire.h>

// Third Party Hardware
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>
#include <Adafruit_Si4713.h>

// Third Party Functions
// @TODO prob don't need printex
//#include <PrintEx.h>
#include <RN52.h>

/* Pin definitions*/
// Si4713
#define RESETPIN 22					// reset Si4713 on Mega 2560

// RN52
//#define CMD_DATA ??				// @TODO Not sure if I need to drive GPIO 9 low to use RN52 library. May not need at all.
#define RN52_TX 18					// use uart1 so we are clear of touch screen shield
#define RN52_RX 19

// 2.8" Capacitive touch shield
// #define SD_CS 4						// @TODO verify description micro sd card chip select, only need pin if storing bmps on micro sd.
#define TFT_CS 10					// @TODO get description
#define TFT_DC 9					// @TODO get description

// define keys for #if's
#define AVAILABLE_FREQS (1)			// debugging : 0 - use defined FMSTATION, normal operation: 1 - scan for & use lowest noise available frequency
#define DEBUG_FM_FREQ (0)			// debugging - switches to const volatile vars to debug fm frequency acquisition
#define DEBUG_FM_TX (0)				// debugging - use  action breakpoint to show transmission info
#define SCAN_AVAILABLE (0)			// debugging - use  action breakpoint to scan for available frequencies and display
#define SHOW_AVAILABLE (0)			// debugging - use  action breakpoint to show available frequencies

// debugging NOP breakpoint, works in conjunction with #if definitions
#define NOP __asm__ __volatile__ ("nop\n\t")

// RN52 communications definitions
#define RN52_BAUD 38400				// per BAL docs, best baud rate for RN52

/* Immutable Globals */
#if DEBUG_FM_FREQ
	const volatile uint16_t FMSTATION = 8770;	// default station 8770 == 87.70 MHz
	const volatile uint16_t MAX_FREQ = 10790;	// upper end of FM band
	const volatile uint16_t MIN_FREQ = 8770;	// lower end of FM band
	const volatile uint8_t TX_POWER = 115;		// dBuV, 88-115 max
	const volatile uint8_t BROADCAST_LEVEL = 60;// noise level for broadcasting stations
#else
	#define FMSTATION 8770
	#define MAX_FREQ 10790
	#define MIN_FREQ 8770
	#define TX_POWER 115
	#define BROADCAST_LEVEL 60
#endif

/* Mutable Globals */
#if DEBUG_FM_TX
volatile uint8_t prevAntCap;
volatile uint8_t prevNoiseLevel;
volatile uint8_t prevASQ;
volatile int8_t prevInLevel;
#endif

//@TODO test if these can go to consts (presuming continue to use)
// tft screen drawing definitions
#define FRAME_X 10
#define FRAME_Y 10
#define FRAME_W 100
#define FRAME_H 50

// @TODO just for reference now, delete once direct control buttons are set up
#define REDBUTTON_X FRAME_X
#define REDBUTTON_Y FRAME_Y
#define REDBUTTON_W (FRAME_W/2)
#define REDBUTTON_H FRAME_H

#define GREENBUTTON_X (REDBUTTON_X + REDBUTTON_W)
#define GREENBUTTON_Y FRAME_Y
#define GREENBUTTON_W (FRAME_W/2)
#define GREENBUTTON_H FRAME_H

/* Global State */
Adafruit_FT6206 ts = Adafruit_FT6206();
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
Adafruit_Si4713 radio = Adafruit_Si4713(RESETPIN);
RN52 rn52 = RN52(RN52_RX, RN52_TX);

// @TODO just for reference now, delete once direct control buttons are set up
boolean recordOn = false;

/*Forward  Init Functions */
uint16_t availableChannels(uint8_t maxLevel, uint16_t defualtFreq, uint16_t loEnd, uint16_t hiEnd);
void drawFrame();
void redBtn();
void greenBtn();

void MyClass::setup()
{
	volatile uint16_t freq = 0;
	volatile uint16_t station = FMSTATION;

	// start the cap screen
	tft.begin();

	// error and exit
	if (!ts.begin(40)) {
		delay(500);
		exit(1);
	} else {
		// config tft screen
		tft.setRotation(1);
		tft.fillScreen(ILI9341_BLACK);
		tft.setCursor(0, 0);
		tft.setTextColor(ILI9341_WHITE);
		tft.setTextSize(3);
	}

	// Banner text
	tft.println("GAM Radio");

	// reset for screen space saving
	tft.setTextSize(1);

	// begin with address 0x63 (CS high default)
	if (!radio.begin()) {
		tft.println("Couldn't find Si4713 transceiver");
		delay(500);
		exit(2);
	}

	#if SCAN_AVAILABLE
		for (freq = MIN_FREQ; freq <= MAX_FREQ; freq += 20) {
			radio.readTuneMeasure(freq);
			radio.readTuneStatus();

			if( radio.currNoiseLevel >= BROADCAST_LEVEL) {
				// Brekapoint action message: Broadcast at {freq/100.0} Mhz, Current Noise Level: {radio.currNoiseLevel}
				NOP;
			}
		}
	#endif

	// normal operation
	#if AVAILABLE_FREQS
		tft.print("Scanning frequencies ...");
		station = availableChannels(BROADCAST_LEVEL, FMSTATION, MIN_FREQ, MAX_FREQ);
		tft.print(" using "); tft.print(station/100.00); tft.print(" Mhz");
		tft.println();
	#else
		tft.print("Using default frequency "); tft.print(FMSTATION/100.00); tft.print(" Mhz");
		tft.println();
	#endif

	/*
	readTuneMeasure(freq) in avalaibleChannels enters receive mode (disables transmitter output power)
	so have to call setTxPower after avalaibleChannels called
	*/
	radio.setTXpower(TX_POWER);
	radio.tuneFM(station);

	// read fm chip stats so we can set up for monitoring in loop
	#if DEBUG_FM_TX
		radio.readTuneStatus();
		radio.readASQ();

		prevAntCap = radio.currAntCap;
		prevASQ = radio.currASQ;
		prevInLevel = radio.currInLevel;
	#endif

	// @TODO set up buttons
	//			vol up
	//	prev	play/pause	next
	//			vol dn

	// @TODO just for reference now, delete once direct control buttons are set up
	// redBtn();
}

void MyClass::loop()
{
	// react to cap screen use input actions
	if (ts.touched()) {
		// Retrieve a point
		TS_Point p = ts.getPoint();

		// rotate coordinate system & flip it around to match the screen.
		p.x = map(p.x, 0, 240, 240, 0);
		p.y = map(p.y, 0, 320, 320, 0);
		int y = tft.height() - p.x;
		int x = p.y;

		// @TODO just for reference now, delete once direct control buttons are set up
		//if (recordOn) {
			//if((x > REDBUTTON_X) && (x < (REDBUTTON_X + REDBUTTON_W))) {
				//if ((y > REDBUTTON_Y) && (y <= (REDBUTTON_Y + REDBUTTON_H))) {
					//redBtn();
				//}
			//}
		//}
		//else {
			//if((x > GREENBUTTON_X) && (x < (GREENBUTTON_X + GREENBUTTON_W))) {
				//if ((y > GREENBUTTON_Y) && (y <= (GREENBUTTON_Y + GREENBUTTON_H))) {
					//greenBtn();
				//}
			//}
		//}
		
		// @TODO react to vol up touch
		
		// @TOD react to vol dn touch
		
		// @TODO react to prev touch
		
		// @TODO react to play/pause touch
		
		// @TODO react to next touch
		
	}

	#if DEBUG_FM_TX
		// for changes in antenna capacitance
		radio.readTuneStatus();

		if( radio.currAntCap != prevAntCap ) {
			// breakpoint action message: Curr ANT capacitance: {radio.currAntCap}
			prevAntCap = radio.currAntCap;
		}

		// for changes in ASQ
		radio.readASQ();
		if( radio.currASQ != prevASQ ) {
			// breakpoint action message: Curr ASQ: 0x{radio.currASQ, HEX}
			prevASQ = radio.currASQ;
		}

		// currInLevel changes too often by too little, so need a floor delta value
		if( abs(radio.currInLevel - prevInLevel) > 10 ) {
			// breakpoint action message: Curr InLevel: {radio.currInLevel}
			prevInLevel = radio.currInLevel;
		}
	#endif

	// @TODO if ASQ over modulating (confirm this value in docs), reduce rn-52 output volume 1 step and print to screen

	// @TODO if ASQ is not over modulating, increase rn-52 output volume 1 step and print to screen
}

/**
* FM Functions
*/

/**
* Scans FM band for stations with lowest noise level frequency stations and returns that frequency for broadcast use
* @param{uint8_t} maxLevel The maximum allowable noise level for an unused frequency
* @param{uint16_t} defualtFreq The failsafe defualt frequency
* @param{uint16_t} loEnd The low end of the FM band
* @param{uint16_t} hiEnd The high end of the FM band
* @return{uint16_t} newBroadcast The new frequency to broadcast on
*/
uint16_t availableChannels(uint8_t maxLevel, uint16_t defualtFreq, uint16_t loEnd, uint16_t hiEnd)
{
	// a scanned frequency
	uint16_t freq = 0;

	// failsafe return value
	uint16_t newBroadcast = defualtFreq;

	// vector of scanned frequencies and their respective current noise levels
	std::vector<std::pair<uint16_t, uint16_t>> scannedFreqs;

	// sort predicate for vector where left side of pair is frequency and right side of pair is noise level
	struct sort_pred {
		bool operator()(const std::pair<uint16_t, uint8_t> &left, const std::pair<uint16_t, uint8_t> &right) {
			return left.second < right.second;
		}
	};

	// radio tune measure requires .2 Mhz increments
	for (freq = loEnd; freq <= hiEnd; freq += 20) {
		radio.readTuneMeasure(freq);
		radio.readTuneStatus();

		if( radio.currNoiseLevel < maxLevel) {
			#if SHOW_AVAILABLE
				// breakpoint action message: Available frequency {freq/100.00} Mhz, Noise Level: {radio.currNoiseLevel}
				NOP;
			#endif
			
			scannedFreqs.push_back(std::make_pair(freq, radio.currNoiseLevel));
		}
	}

	// sort by low noise level ascending
	sort(scannedFreqs.begin(), scannedFreqs.end(), sort_pred());
	newBroadcast = scannedFreqs[0].first;

	return newBroadcast;
}

/**
* Capacitive Touch Functions
*/

// @TODO function header
void drawFrame()
{
	tft.drawRect(FRAME_X, FRAME_Y, FRAME_W, FRAME_H, ILI9341_BLACK);
}


// @TODO just for reference now, delete once direct control buttons are set up
void redBtn()
{
	tft.fillRect(REDBUTTON_X, REDBUTTON_Y, REDBUTTON_W, REDBUTTON_H, ILI9341_RED);
	tft.fillRect(GREENBUTTON_X, GREENBUTTON_Y, GREENBUTTON_W, GREENBUTTON_H, ILI9341_BLUE);
	drawFrame();
	tft.setCursor(GREENBUTTON_X + 6 , GREENBUTTON_Y + (GREENBUTTON_H/2));
	tft.setTextColor(ILI9341_WHITE);
	tft.setTextSize(1);
	tft.println("ON");
	recordOn = false;
}

// @TODO just for reference now, delete once direct control buttons are set up
void greenBtn()
{
	tft.fillRect(GREENBUTTON_X, GREENBUTTON_Y, GREENBUTTON_W, GREENBUTTON_H, ILI9341_GREEN);
	tft.fillRect(REDBUTTON_X, REDBUTTON_Y, REDBUTTON_W, REDBUTTON_H, ILI9341_BLUE);
	drawFrame();
	tft.setCursor(REDBUTTON_X + 6 , REDBUTTON_Y + (REDBUTTON_H/2));
	tft.setTextColor(ILI9341_WHITE);
	tft.setTextSize(1);
	tft.println("OFF");
	recordOn = true;
}

MyClass myClass;

