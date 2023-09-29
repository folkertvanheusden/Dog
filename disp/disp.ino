#include <MD_MAX72xx.h>

MD_MAX72XX mx(MD_MAX72XX::FC16_HW, 27, 25, 32, 4);  // data, clk, cs

void setup() {
	Serial.begin(1000000);

	mx.begin();

	// 32 x 8 display
	mx.control(MD_MAX72XX::INTENSITY, MAX_INTENSITY/3);
	mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
	mx.clear();

	Serial.println(F("Go!"));
}

void loop() {
	mx.setPoint(rand() & 7, rand() & 31, rand() & 1);
}
