#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"

#include "BluefruitConfig.h"
#include "global.h"


#define HUGS_GARMENT 0
// 1: adult
// 0: baby
#ifdef abs
#undef abs
#endif
#define abs(x) ((x)>0?(x):-(x))


#if USE_SERIAL && SOFTWARE_SERIAL_AVAILABLE
  #include <SoftwareSerial.h>
#endif

#define PUMP_PIN2 13
#define PRESSURE_ATM 150
#define SOLENOID_PIN 10

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
void setDigitalOutputs();

Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);
float desiredPressure = PRESSURE_ATM + 10;

void setup(void) {
	ble.begin(VERBOSE_MODE);
	ble.echo(false);
	ble.verbose(false);
	if (HUGS_GARMENT == 1) {
		ble.sendCommandCheckOK(F("AT+GAPDEVNAME=ADULT_HUGS"));
	}
	if (HUGS_GARMENT == 0) {
		ble.sendCommandCheckOK(F("AT+GAPDEVNAME=CHILD_HUGS"));
		digitalWrite(12, LOW);
		digitalWrite(11, LOW);
  		digitalWrite(9, LOW);
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

  	// set pins
  	pinMode(PUMP_PIN1, OUTPUT);
  	pinMode(PUMP_PIN2, OUTPUT);
  	pinMode(SOLENOID_PIN, OUTPUT);
  	pinMode(A0, INPUT);
  	pinMode(A1, INPUT);
  	pinMode(A2, INPUT);
  	pinMode(A3, INPUT);
  	pinMode(A4, INPUT);
  	pinMode(A5, INPUT);
  	pinMode(A7, INPUT);
  	pinMode(A9, INPUT);
}

void loop(void) {
	if (ble.isConnected()) {
		writeValuesToBLE();
		ble.waitForOK();
		readValuesFromBLE();
	}
	readAnalogInputs();
	if (!isProactive) { 
		inflating = shouldInflate; 
	} else {
		inflationValue = inflationValue * (1-INF_ALPHA) + determineState() * INF_ALPHA;
		Serial.println(inflationValue);
		if (inflationCountdown == 0) {
			inflating = (inflationValue > INF_PUMP_THRESHOLD);
			if (inflating) { inflationCountdown = 150000; }
		}
	}
	//Serial.print(isProactive); Serial.print(" "); Serial.println(inflating);
	setDigitalOutputs();
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
	ble.print(F("AT+BLEUARTTX="));
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
	float x, y, z;
	//if (HUGS_GARMENT == 1) {
		x = (analogRead(A0)-511.0)/102.0;
		y = (analogRead(A1)-511.0)/103.0;
		z = (analogRead(A2)-517.0)/102.0;
	//} else {
	// 	x = (analogRead(A0)-504.0)/108.0;
	// 	y = (analogRead(A1)-493.0)/100.0;
	// 	z = (analogRead(A2)-541.0)/105.0;
	// }
	Serial.print(analogRead(A0)); Serial.print(" "); Serial.print(analogRead(A1)); Serial.print(" "); Serial.print(analogRead(A2)); Serial.print(" ");
	currAccel = abs(pow(x*x+y*y+z*z,0.5) - 1.0);
	Serial.print(x); Serial.print(" "); Serial.print(y); Serial.print(" "); Serial.print(z); Serial.print(" "); Serial.println(currAccel);
	currNoise = 0.75 * currNoise + 0.25 * (abs(int(analogRead(A3))-511.0) * 0.488 + 62.514);
	currLight = analogRead(A5);
	averageLight = LIGHT_ALPHA * averageLight + (1.0-LIGHT_ALPHA) * currLight;
	//Serial.print(currLight); Serial.print(" "); Serial.println(averageLight);
	currTemp = -0.2762*analogRead(A9)+156.98;
	currIntPressure = analogRead(A7);
	//Serial.print(currIntPressure);
}

void readTimedAnalogInputs() {
	prev4HR = prev3HR;
	prev3HR = prev2HR;
	prev2HR = prev1HR;
	prev1HR = prev0HR;
	prev0HR = analogRead(A4);
	averageHRInput = uint8_t(averageHRInput * 0.995 + prev0HR * 0.005);
	if (prev2HR > averageHRInput + 70 && prev1HR > prev0HR && prev2HR > prev1HR && prev2HR > prev3HR && prev3HR > prev4HR) {
		uint16_t deltaT = int(incr - prevT) < 0 ? uint32_t(incr - prevT + 65535) : incr - prevT;
		currHR = currHR * HR_ALPHA + (60.0 / (deltaT/1000.0)) * (1-HR_ALPHA);
		prevT = incr;
	}

}

void setDigitalOutputs() {
	//Serial.print("curr:"); Serial.print(currIntPressure); Serial.print(" des:"); Serial.println(desiredPressure);
	if (inflating) {
		if (currIntPressure > desiredPressure * PRESSURE_THRESHOLD_MULTIPLIER) {
			// deflate 
			digitalWrite(SOLENOID_PIN, HIGH);
			digitalWrite(PUMP_PIN1, LOW);
			digitalWrite(PUMP_PIN2, LOW);
			Serial.println("i, d");
		} else if (inflating && currIntPressure < desiredPressure) {
			// inflate
			digitalWrite(SOLENOID_PIN, LOW); 
			digitalWrite(PUMP_PIN1, HIGH);
			digitalWrite(PUMP_PIN2, HIGH);
			Serial.println("i, i");
		} else {
			// hold pressure
			digitalWrite(SOLENOID_PIN, LOW); 
			digitalWrite(PUMP_PIN1, LOW);
			digitalWrite(PUMP_PIN2, LOW);
			Serial.println("i, ~");
		}
	} else {
		if (currIntPressure < PRESSURE_ATM) {
			// deflate 
			digitalWrite(SOLENOID_PIN, HIGH);
			digitalWrite(PUMP_PIN1, LOW);
			digitalWrite(PUMP_PIN2, LOW);
			Serial.println("d, d");
		} else {
			// hold pressure
			digitalWrite(SOLENOID_PIN, LOW);
			digitalWrite(PUMP_PIN1, LOW);
			digitalWrite(PUMP_PIN2, LOW);
			//Serial.println("d, ~");
		}
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
	//Serial.println((lightThresh.isOn && (abs(currLight - averageLight) > 50.0)));
	return ((hrThresh.isOn && (currHR > hrThresh.upperBound ||currHR < hrThresh.lowerBound))|| 
			(tempThresh.isOn && (currTemp > tempThresh.upperBound ||currTemp < tempThresh.lowerBound)) ||
		    (noiseThresh.isOn && (currNoise > noiseThresh.upperBound)) ||
		    (accelThresh.isOn && (currAccel > accelThresh.upperBound)) ||
		    (lightThresh.isOn && (abs(currLight - averageLight) > 50.0)));

}

void parseTULthreshold(Threshold* th, String str) {
	if (str.length() >= 3) { th->isOn = (str[2] == '1'); } 
	uint8_t index = 4;
	String boundString = "";
	while (index < str.length() && str[index] != ',') { 
		boundString += str[index]; 
		index++;
	}
	th->lowerBound = boundString.toDouble();
	index++;
	boundString = "";
	while (index < str.length() && str[index] != '\n') { 
		boundString += str[index]; 
		index++;
	}
	th->upperBound = boundString.toDouble();
}

void parseTLthreshold(Threshold* th, String str) {
	if (str.length() >= 3) { th->isOn = (str[2] == '1'); } 
	uint8_t index = 4;
	String boundString = "";
	while (index < str.length() && str[index] != '\n') { 
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
	while (index < str.length() && str[index] != ',') { 
		string += str[index]; 
		index++;
	}
	weight = string.toDouble();
	index++;
	string = "";
	while (index < str.length() && str[index] != '\n') { 
		string += str[index]; 
		index++;
	}
	desiredPressure = PRESSURE_ATM + 100 * (string.toDouble() - .3);
}