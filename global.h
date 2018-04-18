#pragma once

#define LIGHT_ALPHA 0.9
#define INF_ALPHA 0.1
#define INF_PUMP_THRESHOLD 0.9
#define INF_SOLENOID_THRESHOLD 0.3
#define PRESSURE_THRESHOLD_MULTIPLIER 1.2


#define PUMP_PIN1 5
//#define PUMP_PIN2 13

struct Threshold {
	bool isOn;
	float upperBound;
	float lowerBound;
};

static double HR_ALPHA = 0.95;
static uint16_t incr = 0;
static uint16_t averageHRInput = 0;
static uint16_t currHR = 80;
static float currNoise = 50.0;
static int8_t currTemp = 60;
static float currAccel = 1.0;
static float currLight = 300.0;
static float averageLight = 5.0;
static float currIntPressure = 0;

static Threshold hrThresh;
static Threshold noiseThresh;
static Threshold accelThresh;
static Threshold tempThresh;
static Threshold lightThresh;

static bool isProactive = false;
static bool shouldInflate = false; // only used in manual mode
static float weight;

static bool inflating = false;
static float inflationValue = 0.0;
static uint32_t inflationCountdown = 0;
static uint16_t prev0HR = 0;
static uint16_t prev1HR = 0;
static uint16_t prev2HR = 0;
static uint16_t prev3HR = 0;
static uint16_t prev4HR = 0;
static uint16_t prevT = 0;