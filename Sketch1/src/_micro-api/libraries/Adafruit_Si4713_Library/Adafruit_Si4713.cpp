/***************************************************
  This is a library for the Si4713 FM Radio Transmitter with RDS

  Designed specifically to work with the Si4713 breakout in the
  adafruit shop
  ----> https://www.adafruit.com/products/1958

  These transmitters use I2C to communicate, plus reset pin.
  3 pins are required to interface
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ****************************************************/

#include "Adafruit_Si4713.h"

Adafruit_Si4713::Adafruit_Si4713(int8_t resetpin) {
  _rst = resetpin;
}


boolean Adafruit_Si4713::begin(uint8_t addr) {
  Wire.begin();

  _i2caddr = addr;

  reset();

  powerUp();

  // check for Si4713
  if (getRev() != 13) return false;



  return true;
}

void Adafruit_Si4713::reset() {
  if (_rst > 0) {
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, HIGH);
    delay(10);
    digitalWrite(_rst, LOW);
    delay(10);
    digitalWrite(_rst, HIGH);
  }
}


//////////////////////////////////////////////////////

void Adafruit_Si4713::setProperty(uint16_t property, uint16_t value) {
    _i2ccommand[0] = SI4710_CMD_SET_PROPERTY;
    _i2ccommand[1] = 0;
    _i2ccommand[2] = property >> 8;
    _i2ccommand[3] = property & 0xFF;
    _i2ccommand[4] = value >> 8;
    _i2ccommand[5] = value & 0xFF;
    sendCommand(6);

#ifdef SI4713_CMD_DEBUG
    Serial.print("Set Prop "); Serial.print(property);
    Serial.print(" = "); Serial.println(value);
#endif
}

void Adafruit_Si4713::sendCommand(uint8_t len) {
#ifdef SI4713_CMD_DEBUG
  Serial.print("\n*** Command:");
#endif
  Wire.beginTransmission(_i2caddr);
  for (uint8_t i=0; i<len; i++) {
#ifdef SI4713_CMD_DEBUG
    Serial.print(" 0x"); Serial.print(_i2ccommand[i], HEX);
#endif
    Wire.write(_i2ccommand[i]);
  }
  Wire.endTransmission();
#ifdef SI4713_CMD_DEBUG
  Serial.println();
#endif
    // Wait for status CTS bit
  uint8_t status = 0, timeout = 100;
    while (! (status & SI4710_STATUS_CTS)) {
      Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)1);
      while ((Wire.available() < 1) & (timeout != 0)) {
        //Serial.print('.');
        delay(1);
	timeout--;

      }
      if (timeout == 0)  {
	status = 0;
	return;
      }
      status = Wire.read();
#ifdef SI4713_CMD_DEBUG
      Serial.print("status: "); Serial.println(status, HEX);
#endif
    }
}

void Adafruit_Si4713::tuneFM(uint16_t freqKHz) {
  _i2ccommand[0] = SI4710_CMD_TX_TUNE_FREQ;
  _i2ccommand[1] = 0;
  _i2ccommand[2] = freqKHz >> 8;
  _i2ccommand[3] = freqKHz;
  sendCommand(4);
  while ((getStatus() & 0x81) != 0x81) {
    delay(10);
  }
}

void Adafruit_Si4713::setTXpower(uint8_t pwr, uint8_t antcap) {
  _i2ccommand[0] = SI4710_CMD_TX_TUNE_POWER;
  _i2ccommand[1] = 0;
  _i2ccommand[2] = 0;
  _i2ccommand[3] = pwr;
  _i2ccommand[4] = antcap;
  sendCommand(5);
}

void Adafruit_Si4713::readASQ(void) {
  _i2ccommand[0] = SI4710_CMD_TX_ASQ_STATUS;
  _i2ccommand[1] = 0x1;
  sendCommand(2);

  Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)5);

  Wire.read(); // status
  currASQ = Wire.read(); // resp1
  Wire.read(); // resp2 unused
  Wire.read(); // resp3 unused
  currInLevel =  Wire.read();
}


void Adafruit_Si4713::readTuneStatus(void) {
  _i2ccommand[0] = SI4710_CMD_TX_TUNE_STATUS;
  _i2ccommand[1] = 0x1; // INTACK
  sendCommand(2);

   Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)8);

    Wire.read(); Wire.read(); // status and resp1
    currFreq = Wire.read();   // resp2
    currFreq <<= 8;
    currFreq |=  Wire.read(); // resp3
    Wire.read();              // resp4
    currdBuV =  Wire.read();
    currAntCap = Wire.read();
    currNoiseLevel =  Wire.read();
}


void Adafruit_Si4713::readTuneMeasure(uint16_t freq) {
  // check freq is multiple of 50khz
  if (freq % 5 != 0) {
    freq -= (freq % 5);
  }
  //Serial.print("Measuring "); Serial.println(freq);
  _i2ccommand[0] = SI4710_CMD_TX_TUNE_MEASURE;
  _i2ccommand[1] = 0;
  _i2ccommand[2] = freq >> 8;
  _i2ccommand[3] = freq;
  _i2ccommand[4] = 0;

  sendCommand(5);
  while (getStatus() != 0x81) delay(10);
}

void Adafruit_Si4713::beginRDS(uint16_t programID) {
  // 66.25KHz (default is 68.25)
  setProperty(SI4713_PROP_TX_AUDIO_DEVIATION, 6625);

  // 2KHz (default)
  setProperty(SI4713_PROP_TX_RDS_DEVIATION, 200);

  // RDS IRQ
  setProperty(SI4713_PROP_TX_RDS_INTERRUPT_SOURCE, 0x0001);
  // program identifier
  setProperty(SI4713_PROP_TX_RDS_PI, programID);
  // 50% mix (default)
  setProperty(SI4713_PROP_TX_RDS_PS_MIX, 0x03);
  // RDSD0 & RDSMS (default)
  setProperty(SI4713_PROP_TX_RDS_PS_MISC, 0x1808);
  // 3 repeats (default)
  setProperty(SI4713_PROP_TX_RDS_PS_REPEAT_COUNT, 3);

  setProperty(SI4713_PROP_TX_RDS_MESSAGE_COUNT, 1);
  setProperty(SI4713_PROP_TX_RDS_PS_AF, 0xE0E0); // no AF
  setProperty(SI4713_PROP_TX_RDS_FIFO_SIZE, 0);

  setProperty(SI4713_PROP_TX_COMPONENT_ENABLE, 0x0007);
}

void Adafruit_Si4713::setRDSstation(char *s) {
  uint8_t i, len = strlen(s);
  uint8_t slots = (len+3) / 4;

  for (uint8_t i=0; i<slots; i++) {
    memset(_i2ccommand, ' ', 6); // clear it with ' '
    memcpy(_i2ccommand+2, s, min(4, strlen(s)));
    s+=4;
    _i2ccommand[6] = 0;
    //Serial.print("Set slot #"); Serial.print(i);
    //char *slot = (char *)( _i2ccommand+2);
    //Serial.print(" to '"); Serial.print(slot); Serial.println("'");
    _i2ccommand[0] = SI4710_CMD_TX_RDS_PS;
    _i2ccommand[1] = i; // slot #
    sendCommand(6);
  }
}

void Adafruit_Si4713::setRDSbuffer(char *s) {
  uint8_t i, len = strlen(s);
  uint8_t slots = (len+3) / 4;
  char slot[5];

  for (uint8_t i=0; i<slots; i++) {
    memset(_i2ccommand, ' ', 8); // clear it with ' '
    memcpy(_i2ccommand+4, s, min(4, strlen(s)));
    s+=4;
    _i2ccommand[8] = 0;
    //Serial.print("Set buff #"); Serial.print(i);
    //char *slot = (char *)( _i2ccommand+4);
    //Serial.print(" to '"); Serial.print(slot); Serial.println("'");
    _i2ccommand[0] = SI4710_CMD_TX_RDS_BUFF;
    if (i == 0)
      _i2ccommand[1] = 0x06;
    else
      _i2ccommand[1] = 0x04;

    _i2ccommand[2] = 0x20;
    _i2ccommand[3] = i;
    sendCommand(8);
  }


  /*
  // set time
   _i2ccommand[0] = SI4710_CMD_TX_RDS_BUFF;
   _i2ccommand[1] = 0x84;
   _i2ccommand[2] = 0x40; // RTC
   _i2ccommand[3] = 01;
   _i2ccommand[4] = 0xA7;
   _i2ccommand[5] = 0x0B;
   _i2ccommand[6] = 0x2D;
   _i2ccommand[7] = 0x6C;
   sendCommand(8);

   Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)6);
   Wire.read(); // status
   Serial.print("FIFO overflow: 0x"); Serial.println(Wire.read(), HEX);
   Serial.print("Circ avail: "); Serial.println(Wire.read());
   Serial.print("Circ used: "); Serial.println(Wire.read());
   Serial.print("FIFO avail: "); Serial.println(Wire.read());
   Serial.print("FIFO used: "); Serial.println(Wire.read());
   */

   // enable!
  //Serial.println("Enable RDS");
  setProperty(SI4713_PROP_TX_COMPONENT_ENABLE, 0x0007); // stereo, pilot+rds
  /*
  // wait till ready
  while (getStatus() != 0x80) {
    Serial.println(getStatus(), HEX);
    delay(100);
  }
  delay(500);
  _i2ccommand[0] = SI4710_CMD_TX_RDS_BUFF;
  _i2ccommand[1] = 0x01; // clear RDSINT
  _i2ccommand[2] = 0x0;
  _i2ccommand[3] = 0x0;
  _i2ccommand[4] = 0x0;
  _i2ccommand[5] = 0x0;
  _i2ccommand[6] = 0x0;
  _i2ccommand[7] = 0x0;
   sendCommand(8);
   // get reply!
   Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)6);
   for (uint8_t x=0; x<7; x++) {
     Wire.read();
   }
   */
   /*   or you can read the status
   Wire.read(); // status
   Serial.print("FIFO overflow: 0x"); Serial.println(Wire.read(), HEX);
   Serial.print("Circ avail: "); Serial.println(Wire.read());
   Serial.print("Circ used: "); Serial.println(Wire.read());
   Serial.print("FIFO avail: "); Serial.println(Wire.read());
   Serial.print("FIFO used: "); Serial.println(Wire.read());
   */
}

uint8_t Adafruit_Si4713::getStatus(void) {

   Wire.beginTransmission(_i2caddr);
   Wire.write(SI4710_CMD_GET_INT_STATUS);
   Wire.endTransmission();
   Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)1);
   while (Wire.available() < 1) {
     //Serial.print('.');
     delay(1);
   }
   return  Wire.read();
}

/**
* Powers up Si4713 in FM Transmission mode with analog inputs and without RDS
*/
void Adafruit_Si4713::powerUp(void) {
	/* Modified configuration, see Section 12.1, page 253 of AN332.pdf 
	Not patching or checking revision
	Not using all default settings
	Not using interrupts
	Not using GPO
	*/
		   
	/* Power Up, see Sec 5.1 pg 12
	CMD = POWER_UP							0x01
	ARG1 = 0b0001 0010 =					0x12
	CTS Interrupt Enable Bit 7 = 0			CTS interrupt disabled
	GPO2 Output Enable Bit 6 = 0			GPO2 output disabled
	Patch Enable Bit 5 = 0					Boot normally
	Crystal Oscillator Enable Bit 4	= 1		Use crystal oscillator
	Function[3:0] FM Transmit
	Bit 3:2 = 0, Bit 1 = 1, Bit 0 = 0		FM transmission enabled
	
	Analog Mode, see Sec 5.1 pg 13
	Arg 2 = 0b0101 0000 =					0x50
	Application Setting OPMODE[7:0]			Analog audio inputs (LIN/RIN)
	Bit 7 = 0, Bit 6 = 1, Bit 5 = 0, Bit 4 = 1, Bit 3:0 = 0
	*/
	_i2ccommand[0] = SI4710_CMD_POWER_UP;
	_i2ccommand[1] = SI4710_CMD_POWER_UP_ARG_1;
	_i2ccommand[2] = SI4710_CMD_POWER_UP_ARG_2;
    sendCommand(3);
	
	/* Reference Clock settings */
	// Reference Clock 0x0201 see pg 34
	// Reference Clock Prescale 0x0202 see page 35
	setProperty(SI4713_PROP_REFCLK_FREQ, 0x8000);
	setProperty(SI4713_PROP_REFCLK_PRESCALE, 0x0001);
	
	/*Stereo settings */
	// Pilot Deviation 0x2102 see page 37
	// Pilot Frequency 0x2107 see page 40
	// Enable Stereo Components 0x2100 LMR & PILOT, disable RDS, see page 36
	setProperty(SI4713_PROP_TX_PILOT_DEVIATION, 0x02A3);
	setProperty(SI4713_PROP_TX_PILOT_FREQUENCY, 0x4A38);
	setProperty(SI4713_PROP_TX_COMPONENT_ENABLE, 0x0003);
	
	/* Maximize Audio Volume Step 1 see Sec 11.4 pg 251 */
	// Transmission Line Input Level 0x2104 line attenuation and Line level 0x327C see pg  38 and pg 251
	// Audio Deviation 0x2101 see page 36
	// RDS Deviation 0x2103 needs to be set even if not using RDS, see page 37 and 251-252 and Table 54 pg 263
	// Audio Limiter Release Time 0x2205 see pg 44 and Sec 11.3 pg 251
	// Preemphasis 0x2106 see page 39
	setProperty(SI4713_PROP_TX_LINE_LEVEL_INPUT_LEVEL, 0x327C);
	setProperty(SI4713_PROP_TX_AUDIO_DEVIATION, 0x1AA9);
	setProperty(SI4713_PROP_TX_RDS_DEVIATION, 0x00C8);
	setProperty(SI4713_PROP_TX_LIMITER_RELEASE_TIME, 0x000D);
	setProperty(SI4713_PROP_TX_PREEMPHASIS, 0x0000);	
	
	/* Maximize Audio Volume Step 2 see Sec 11.4 pg 252 */
	// Audio Compressor Threshold for Audio Dynamic Range Control 0x2201 see page 41 & pg 252 example 2
	// Audio Compressor Attack Time 0x2202 see page 42 and 252 example 2
	// Audio Compressor Release Time 0x2203 pg 43 and 252 example 2
	// Audio Compressor Gain 0x2204 see pg 43 & pg 252 example 2
	// Audio Compressor Enable 0x2200 see pg 40
	setProperty(SI4713_PROP_TX_ACOMP_THRESHOLD, 0xFFF1);
	setProperty(SI4713_PROP_TX_ATTACK_TIME, 0x0000);
	setProperty(SI4713_PROP_TX_RELEASE_TIME, 0x0004);
	setProperty(SI4713_PROP_TX_ACOMP_GAIN, 0x0005);
    setProperty(SI4713_PROP_TX_ACOMP_ENABLE, 0x0003);	
}

uint8_t Adafruit_Si4713::getRev(void) {
    _i2ccommand[0] = SI4710_CMD_GET_REV;
    _i2ccommand[1] = 0;

    sendCommand(2);

    Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)9);

    Wire.read();
    uint8_t pn = Wire.read();
    uint8_t fw =  Wire.read();
    fw <<= 8;
    fw |= Wire.read();
    uint8_t patch =  Wire.read();
    patch <<= 8;
    patch |= Wire.read();
    uint8_t cmp =  Wire.read();
    cmp <<= 8;
    cmp |= Wire.read();
    uint8_t chiprev = Wire.read();

    /*
    Serial.print("Part # Si47"); Serial.print(pn);
    Serial.print("-"); Serial.println(fw, HEX);

    Serial.print("Firmware 0x"); Serial.println(fw, HEX);
    Serial.print("Patch 0x"); Serial.println(patch, HEX);
    Serial.print("Chip rev "); Serial.write(chiprev); Serial.println();
    */

    return pn;
}

void Adafruit_Si4713::setGPIOctrl(uint8_t x) {
  // Serial.println("GPIO direction");
  _i2ccommand[0] = SI4710_CMD_GPO_CTL;
  _i2ccommand[1] = x;

  sendCommand(2);
}

void Adafruit_Si4713::setGPIO(uint8_t x) {
  //Serial.println("GPIO set");
  _i2ccommand[0] = SI4710_CMD_GPO_SET;
  _i2ccommand[1] = x;

  sendCommand(2);
}


///

/*
void Adafruit_Si4713::rdstest() {
  _i2ccommand[0] = SI4710_CMD_TX_RDS_BUFF;
  _i2ccommand[1] = 0x02;
  _i2ccommand[2] = 0;
  _i2ccommand[3] = 0;
  _i2ccommand[4] = 0;
  _i2ccommand[5] = 0;
  _i2ccommand[6] = 0;
  _i2ccommand[7] = 0;
  sendCommand(8);
   Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)6);
   Wire.read(); // status
   Serial.print("FIFO overflow: 0x"); Serial.println(Wire.read(), HEX);
   Serial.print("Circ avail: "); Serial.println(Wire.read());
   Serial.print("Circ used: "); Serial.println(Wire.read());
   Serial.print("FIFO avail: "); Serial.println(Wire.read());
   Serial.print("FIFO used: "); Serial.println(Wire.read());

  _i2ccommand[0] = SI4710_CMD_TX_RDS_BUFF;
  _i2ccommand[1] = 0x04;
  _i2ccommand[2] = 0x20;
  _i2ccommand[3] = 0x00;
  _i2ccommand[4] = 0x54;
  _i2ccommand[5] = 0x65;
  _i2ccommand[6] = 0x73;
  _i2ccommand[7] = 0x74;
  sendCommand(8);
   Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)6);
   Wire.read(); // status
   Serial.print("FIFO overflow: 0x"); Serial.println(Wire.read(), HEX);
   Serial.print("Circ avail: "); Serial.println(Wire.read());
   Serial.print("Circ used: "); Serial.println(Wire.read());
   Serial.print("FIFO avail: "); Serial.println(Wire.read());
   Serial.print("FIFO used: "); Serial.println(Wire.read());


  _i2ccommand[0] = SI4710_CMD_TX_RDS_BUFF;
  _i2ccommand[1] = 0x04;
  _i2ccommand[2] = 0x20;
  _i2ccommand[3] = 0x01;
  _i2ccommand[4] = 0xD0;
  _i2ccommand[5] = 0x00;
  _i2ccommand[6] = 0x00;
  _i2ccommand[7] = 0x00;
  sendCommand(8);
   Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)6);
   Wire.read(); // status
   Serial.print("FIFO overflow: 0x"); Serial.println(Wire.read(), HEX);
   Serial.print("Circ avail: "); Serial.println(Wire.read());
   Serial.print("Circ used: "); Serial.println(Wire.read());
   Serial.print("FIFO avail: "); Serial.println(Wire.read());
   Serial.print("FIFO used: "); Serial.println(Wire.read());
}
*/
