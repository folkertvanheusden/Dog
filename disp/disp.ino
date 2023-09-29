#include <FastLED.h>

CRGB leds[32];

#include <MD_MAX72xx.h>

MD_MAX72XX mx(MD_MAX72XX::FC16_HW, 27, 25, 32, 4);  // data, clk, cs

void setup() {
	Serial.begin(1000000);

	FastLED.addLeds<NEOPIXEL, 4>(leds, 32);  // IO4, 32 leds

	mx.begin();

	// 32 x 8 display
	mx.control(MD_MAX72XX::INTENSITY, MAX_INTENSITY/3);
	mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
	mx.clear();

	Serial.println(F("Go!"));
}

void loop() {
	mx.setPoint(rand() & 7, rand() & 31, rand() & 1);

	leds[rand() & 31].r = rand() & 63;
	leds[rand() & 31].g = rand() & 63;
	leds[rand() & 31].b = rand() & 63;

	FastLED.show();
}
