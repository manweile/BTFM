/*
 * @file MyClass.cpp
 *
 * @created 2/17/2024 4:48:38 PM
 * @author Gerald Manweiler
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

/* Pin definitions*/
// Si4713
#define RESETPIN 22					// reset Si4713 on Mega 2560

// 2.8" Capacitive touch shield
#define TFT_CS 10					// TFT chip select
#define TFT_DC 9					// TFT data/command select 

// define keys for #if's
#define AVAILABLE_FREQS (1)			// sets transmission frequency to use, 1: scan for & use lowest noise available frequency (production), 0: use defined FMSTATION (debugging)
#define DEBUG_FM_FREQ (0)			// sets how immutable globals are declared, 1: const volatile (debugging), 0: const (production)
#define DEBUG_FM_TX (0)				// declare volatile vars for debugging fm transmission, use conditional breakpoint to display, 1: declare vars (debugging), 0: do not declare vars (production)
#define SHOW_AVAILABLE (0)			// shows frequencies found by availableChannels function, use conditional breakpoint to display, 1: show frequencies (debugging), 0: do not show frequencies (production)
#define SCAN_TX (0)					// scans for transmitting frequencies, use conditional breakpoint to display, 1: scan (debugging), 0: do not scan (production)

// debugging NOP breakpoint, works in conjunction with #if definitions
#define NOP __asm__ __volatile__ ("nop\n\t")

/* Immutable Globals */
#if DEBUG_FM_FREQ
	const volatile uint16_t FMSTATION = 8770;	// default station 8770 == 87.70 MHz
	const volatile uint16_t MAX_FREQ = 10790;	// upper end of FM band
	const volatile uint16_t MIN_FREQ = 8770;	// lower end of FM band
	const volatile uint8_t TX_POWER = 115;		// dBuV, 88-115 max
	const volatile uint8_t BROADCAST_LEVEL = 60;// noise level for broadcasting stations
#else
	const uint16_t FMSTATION = 8770;	
	const uint16_t MAX_FREQ = 10790;	
	const uint16_t MIN_FREQ = 8770;	
	const uint8_t TX_POWER = 115;		
	const uint8_t BROADCAST_LEVEL = 60;
#endif

/* Mutable Globals */
#if DEBUG_FM_TX
volatile uint8_t prevAntCap;
volatile uint8_t prevNoiseLevel;
volatile uint8_t prevASQ;
volatile int8_t prevInLevel;
#endif

/* Global State */
Adafruit_FT6206 ts = Adafruit_FT6206();
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
Adafruit_Si4713 radio = Adafruit_Si4713(RESETPIN);

/*Forward Init Functions */
/**
 * @brief Scans FM band for stations.
 * @details Finds lowest noise level frequency stations and returns that frequency for broadcast use.
 * @return uint16_t newBroadcast The new frequency to broadcast on
 */
uint16_t availableChannels();

/**
 * @brief Sets up hardware.
 * @details Initializes lcd and fm transceiver.
 */
void MyClass::setup()
{
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
	
	tft.println("v1.5.1");

	// begin with address 0x63 (CS high default)
	if (!radio.begin()) {
		tft.println("Couldn't find Si4713 transceiver");
		delay(500);
		exit(2);
	}

	#if SCAN_TX
		for (uint16_t freq = MIN_FREQ; freq <= MAX_FREQ; freq += 20) {
			radio.readTuneMeasure(freq);
			radio.readTuneStatus();

			if( radio.currNoiseLevel >= BROADCAST_LEVEL) {
				// Breakpoint action message: Broadcast at {freq/100.0} Mhz, Current Noise Level: {radio.currNoiseLevel}
				NOP;
			}
		}
	#endif

	// normal operation
	#if AVAILABLE_FREQS
		tft.print("Scanning frequencies ...");
		station = availableChannels();
		tft.print(" using "); tft.print(station/100.00); tft.print(" Mhz");
		tft.println();
	#else
		tft.print("Using default frequency "); tft.print(FMSTATION/100.00); tft.print(" Mhz");
		tft.println();
	#endif

	/*
	readTuneMeasure(freq) in availableChannels enters receive mode (disables transmitter output power)
	so have to call setTxPower after availableChannels called
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
}

/**
 * @brief Allows program to change and respond.
 * @details Loops continuously until power down.
 */
void MyClass::loop()
{
	// get fm chip stats 
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

uint16_t availableChannels()
{
	volatile uint16_t FMSTATION = 8770;		// default station 8770 == 87.70 MHz
	volatile uint16_t MAX_FREQ = 10790;		// upper end of FM band
	volatile uint16_t MIN_FREQ = 8770;		// lower end of FM band
	volatile uint8_t BROADCAST_LEVEL = 60;	// max noise level for broadcasting stations
	
	// a scanned frequency
	uint16_t freq = MIN_FREQ;

	// failsafe return value
	uint16_t newBroadcast = FMSTATION;

	// vector of scanned frequencies and their respective current noise levels
	std::vector<std::pair<uint16_t, uint16_t>> scannedFreqs;

	// sort predicate for vector where left side of pair is frequency and right side of pair is noise level
	struct sort_pred {
		bool operator()(const std::pair<uint16_t, uint8_t> &left, const std::pair<uint16_t, uint8_t> &right) {
			return left.second < right.second;
		}
	};

	// radio tune measure requires .2 Mhz increments
	for (freq; freq <=  MAX_FREQ; freq += 20) {
		radio.readTuneMeasure(freq);
		radio.readTuneStatus();

		if( radio.currNoiseLevel < BROADCAST_LEVEL) {
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

MyClass myClass;