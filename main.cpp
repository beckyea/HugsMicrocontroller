#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"

#include "BluefruitConfig.h"
#include "global.h"

#define USE_SERIAL 0

#ifdef abs
#undef abs
#endif
#define abs(x) ((x)>0?(x):-(x))


#if USE_SERIAL && SOFTWARE_SERIAL_AVAILABLE
  #include <SoftwareSerial.h>
#endif

// PREDEFINE FUNCTIONS
void writeValuesToBLE();
void readValuesFromBLE();
void parseInput(String str);
void parseTULthreshold(Threshold* th, String str);
void parseTLthreshold(Threshold* th, String str);
void parseTthreshold(Threshold* th, String str);
void parseSettings(String str);
void readAnalogInputs();
void readTimedAnalogInputs();
void calculateLight();
uint8_t determineState();


Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);


void setup(void) {
	if (USE_SERIAL) { 
		while (!Serial);
		Serial.begin(115200);
	}
	ble.begin(VERBOSE_MODE);
	ble.echo(false);
	ble.verbose(false);

	if (!ble.sendCommandCheckOK(F("AT+GAPDEVNAME=HUGS")) && USE_SERIAL ) {
		Serial.println("Can't set");
	}

	cli();
	TCCR0A = 0;
	TCCR0B = 0;
	TCNT0  = 0;
 	OCR0A = 255; // 2kHz
  	TCCR0A |= (1 << WGM01);
  	TCCR0B |= (1 << CS01) | (1 << CS00);   
  	TIMSK0 |= (1 << OCIE0A);
  	sei();
}

void loop(void) {
	if (ble.isConnected()) {
		writeValuesToBLE();
		if (!ble.waitForOK() && USE_SERIAL) {
			Serial.println("Failed to send");
		}
		readValuesFromBLE();
	}
	readAnalogInputs();
	if (!isProactive) { inflating = shouldInflate; }
	else {
		inflationValue = inflationValue * (1-INF_ALPHA) + determineState() * INF_ALPHA;
		if (inflationCountdown == 0) {
			inflating = (inflationValue > INF_PUMP_THRESHOLD);
			if (inflating) { inflationCountdown = 150000; }
		}
	}
}

ISR(TIMER0_COMPA_vect){
	incr++;
	if (incr % 5 == 0) {
		readTimedAnalogInputs();
	}
  	if (inflationCountdown > 0) { inflationCountdown--; }
}


void readValuesFromBLE() {
	ble.println("AT+BLEUARTRX");
	ble.readline();
	if (strcmp(ble.buffer, "OK") == 0) { return; }
	parseInput(ble.buffer);
	ble.waitForOK();
}


void writeValuesToBLE() {
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
	ble.println(inflating ? 1 : 0);
}

void readAnalogInputs() {
	float x = (analogRead(A0)-504.0)/108.0;
	float y = (analogRead(A1)-493.0)/100.0;
	float z = (analogRead(A2)-541.0)/105.0;
	currAccel = abs(pow(x*x+y*y+z*z,0.5) - 1.0);
	currNoise = 0.75 * currNoise + 0.25 * (abs(int(analogRead(A3))-511.0) * 0.488 + 62.514);
	currLight = analogRead(A5);
	averageLight = LIGHT_ALPHA * averageLight + (1.0-LIGHT_ALPHA) * currLight;
	currTemp = analogRead(A6)/100.0;
}

void readTimedAnalogInputs() {
	prev4HR = prev3HR;
	prev3HR = prev2HR;
	prev2HR = prev1HR;
	prev1HR = prev0HR;
	prev0HR = analogRead(A4);
	averageHRInput = uint8_t(averageHRInput * 0.995 + prev0HR * 0.005);
	if (prev2HR > averageHRInput + 70 && prev1HR > prev0HR && prev2HR > prev1HR && prev2HR > prev3HR && prev3HR > prev4HR) {
		uint16_t deltaT = incr - prevT < 0 ? uint32_t(incr - prevT + 65535) : incr - prevT;
		currHR = currHR * HR_ALPHA + (60.0 / (deltaT/1000.0)) * (1-HR_ALPHA);
		prevT = incr;
	}

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

uint8_t determineState() {
	return ((hrThresh.isOn && (currHR > hrThresh.upperBound ||currHR < hrThresh.lowerBound))|| 
			(tempThresh.isOn && (currTemp > tempThresh.upperBound ||currTemp < tempThresh.lowerBound)) ||
		    (noiseThresh.isOn && (currNoise > noiseThresh.upperBound)) ||
		    (accelThresh.isOn && (currAccel > accelThresh.upperBound)) ||
		    (lightThresh.isOn && (abs(currLight - averageLight) > 100.0)));

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
	weight = string.toDouble();
}