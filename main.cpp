#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

#include "BluefruitConfig.h"
#include "global.h"

#define USE_SERIAL 1
#define FACTORY_RESET 0 

#if USE_SERIAL && SOFTWARE_SERIAL_AVAILABLE
  #include <SoftwareSerial.h>
#endif


void parseInput(String str);
void parseTULthreshold(Threshold* th, String str);
void parseTLthreshold(Threshold* th, String str);
void parseTthreshold(Threshold* th, String str);
void parseSettings(String str);


Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// A small helper
void error(const __FlashStringHelper*err) {
	Serial.println(err);
	while (1);
}


void setup(void) {
	delay(500);

	if (USE_SERIAL) { 
		while (!Serial);
		delay(500);
		Serial.begin(115200);
		Serial.println(F("Initializing Bluetooth"));
	}

	if (!ble.begin(VERBOSE_MODE) && USE_SERIAL) {
		error(F("Couldn't find device"));
	}

	if (FACTORY_RESET) {
		if (!ble.factoryReset(1) && USE_SERIAL) {
			error(F("Failed"));
		}
	}

	ble.echo(false);
	ble.verbose(false);

	if (!ble.sendCommandCheckOK(F("AT+GAPDEVNAME=HUGS")) && USE_SERIAL ) {
		error(F("Could not set device name"));
	}

	while (!ble.isConnected()) {
		delay(500);
	}
}

void loop(void) {
	// create thresholds to produce
	ble.print("AT+BLEUARTTX=");
	ble.print('h');
	ble.print(currHR);
	ble.print('n');
	ble.print(currNoise);
	ble.print('t');
	ble.print(currTemp);
	ble.print('a');
	ble.print(currAccel);
	ble.print('i');
	ble.print(inflating ? 1 : 0);
	ble.println('0');

	//ble.println('\\n');
   // check response stastus
	if (!ble.waitForOK() && USE_SERIAL) {
		Serial.println(F("Failed to send"));
	}
	delay(5);

  // Check for incoming characters from Bluefruit
	ble.println("AT+BLEUARTRX");
	ble.readline();
	if (strcmp(ble.buffer, "OK") == 0) { return; }
	parseInput(ble.buffer);
	ble.waitForOK();
}

void parseInput(String str) {
	switch(str[0]) {
		case 'h': parseTULthreshold(&hrThresh, str); break;
		case 't': parseTLthreshold(&tempThresh, str); break;
		case 'n': parseTULthreshold(&noiseThresh, str); break;
		case 'a': parseTLthreshold(&accelThresh, str); break;
		case 'l': parseTthreshold(&lightThresh, str); break;
		case 's': parseSettings(str); break;
	}
}

void parseTULthreshold(Threshold* th, String str) {
	if (str.length() >= 3) { th->isOn = (str[2] == '1'); } 
	uint8_t index = 4;
	String boundString = "";
	while (index < str.length()-1 && str[index] != ',') { 
		boundString += str[index]; 
		index++;
	}
	th->lowerBound = boundString.toDouble();
	index++;
	boundString = "";
	while (index < str.length()-1 && str[index] != '\n') { 
		boundString += str[index]; 
		index++;
	}
	th->upperBound = boundString.toDouble();
}

void parseTLthreshold(Threshold* th, String str) {
	if (str.length() >= 3) { th->isOn = (str[2] == '1'); } 
	uint8_t index = 4;
	String boundString = "";
	while (index < str.length()-1 && str[index] != '\n') { 
		boundString += str[index]; 
		index++;
	}
	th->lowerBound = boundString.toDouble();
}

void parseTthreshold(Threshold* th, String str) {
	if (str.length() >= 3) { th->isOn = (str[2] == '1'); } 
}

void parseSettings(String str) {
	if (str.length() >= 3) { isProactive = (str[2] == '1'); } 
	if (str.length() >= 5) { shouldInflate = (str[4] == '1'); } 
	uint8_t index = 6;
	String string = "";
	while (index < str.length()-1 && str[index] != '\n') { 
		string += str[index]; 
		index++;
	}
	inflating = shouldInflate;
	weight = string.toDouble();
}