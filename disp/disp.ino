#include <FastLED.h>

CRGB leds[32];

#include <MD_MAX72xx.h>

MD_MAX72XX mx(MD_MAX72XX::FC16_HW, 27, 25, 32, 4);  // data, clk, cs

#include <TM1637.h>

TM1637 tm(21, 22);

void setup() {
	Serial.begin(1000000);

	// ledring
	FastLED.addLeds<NEOPIXEL, 4>(leds, 32);  // IO4, 32 leds

	mx.begin();

	// 32 x 8 display
	mx.control(MD_MAX72XX::INTENSITY, MAX_INTENSITY/3);
	mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
	mx.clear();

	// TM1637
	tm.begin();
	tm.setBrightness(3);

	Serial.println(F("Go!"));
}

void loop() {
	mx.setPoint(rand() & 7, rand() & 31, rand() & 1);

	leds[rand() & 31].r = rand() & 63;
	leds[rand() & 31].g = rand() & 63;
	leds[rand() & 31].b = rand() & 63;

	FastLED.show();

	char buffer[5] { 0 };
	for(int i=0; i<4; i++)
		buffer[i] = 'A' + (rand() % 26);

	String temp = buffer;

	tm.display(temp);
}
