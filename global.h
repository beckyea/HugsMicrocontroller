#pragma once

enum Sensor {
	HEART_RATE = 'h',
	NOISE = 'n',
	TEMP = 't',
	ACCEL = 'a',
	LIGHT = 'l'
};

struct Threshold {
	bool isOn;
	double upperBound;
	double lowerBound;
};


static uint8_t currHR = 80;
static double currNoise = 50.0;
static uint8_t currTemp = 60;
static double currAccel = 1.0;

// static Threshold hrThresh = {.isOn = false, .upperBound = 0.0, .lowerBound = 0.0 };
// static Threshold noiseThresh = {.isOn = false, .upperBound = 0.0, .lowerBound = 0.0 };
// static Threshold accelThresh = {.isOn = false, .upperBound = 0.0, .lowerBound = 0.0 };
// static Threshold tempThresh = {.isOn = false, .upperBound = 0.0, .lowerBound = 0.0 };
// static Threshold lightThresh = {.isOn = false, .upperBound = 0.0, .lowerBound = 0.0 };

static Threshold hrThresh;
static Threshold noiseThresh;
static Threshold accelThresh;
static Threshold tempThresh;
static Threshold lightThresh;

static bool isProactive;
static bool shouldInflate;
static double weight;

static bool inflating = false;