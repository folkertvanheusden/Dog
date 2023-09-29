#include <FastLED.h>

#define N_RING_LEDS 35
CRGB leds[N_RING_LEDS];

#include <MD_MAX72xx.h>

MD_MAX72XX mx(MD_MAX72XX::FC16_HW, 27, 25, 32, 4);  // data, clk, cs

#include <TM1637.h>

TM1637 tm(21, 22);

#include <string>
#include <vector>

void setup() {
	Serial.begin(1000000);

	// ledring
	FastLED.addLeds<NEOPIXEL, 4>(leds, N_RING_LEDS);  // IO4, N_RING_LEDS leds
	FastLED.setBrightness(64);

	memset(leds, 0x00, sizeof leds);
	FastLED.show();

	// 32 x 8 display
	mx.begin();

	mx.control(MD_MAX72XX::INTENSITY, MAX_INTENSITY/3);
	mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
	mx.clear();

	// TM1637
	tm.begin();
	tm.setBrightness(3);

	Serial.println(F("Go!"));
}

std::vector<std::string> split(std::string in, std::string splitter)
{
	std::vector<std::string> out;
	size_t splitter_size = splitter.size();

	for(;;)
	{
		size_t pos = in.find(splitter);
		if (pos == std::string::npos)
			break;

		std::string before = in.substr(0, pos);
		out.emplace_back(before);

		size_t bytes_left = in.size() - (pos + splitter_size);
		if (bytes_left == 0)
		{
			out.emplace_back("");
			return out;
		}

		in = in.substr(pos + splitter_size);
	}

	if (in.size() > 0)
		out.emplace_back(in);

	return out;
}

void hextobitmap(const std::string & in, uint8_t bitmap[])
{
	uint8_t v = 0;

	for(size_t i=0; i<in.size(); i++) {
		v <<= 4;

		char c = toupper(in.at(i));

		if (c >= 'A')
			v |= c - 'A' + 10;
		else
			v |= c - '0';

		if (i & 1)
			bitmap[i / 2] = v;
	}
}

char buffer[128] { 0 };
int offset = 0;

void loop() {
	if (Serial.available() == 0)
		return;

	char c = Serial.read();

	if (c >= 32 && c < 127) {
		if (offset < sizeof(buffer) - 1) {
			buffer[offset++] = c;

			Serial.write(c);
		}

		return;
	}
	
	if (c != 10)
		return;

	Serial.println(F(""));

	buffer[offset] = 0x00;
	offset = 0;

	auto parts = split(buffer, " ");

	if (parts.empty())
		return;

	if (parts.at(0) == "depth" && parts.size() == 2) {
		int depth = atoi(parts.at(1).c_str());

		bool red = false;

		if (depth > N_RING_LEDS) {
			red = true;

			depth = N_RING_LEDS;
		}

		for(int i=0; i<depth; i++)
			leds[i].g = 255;

		for(int i=depth; i<N_RING_LEDS; i++)
			leds[i].g = 0;

		leds[N_RING_LEDS - 1].r = red ? 255 : 0;

		FastLED.show();
	}
	else if (parts.at(0) == "move" && parts.size() == 2) {
		String temp = parts.at(1).c_str();

		tm.display(temp);
	}
	else if (parts.at(0) == "score" && parts.size() == 2) {
		int score = atoi(parts.at(1).c_str());

		bool red = false;

		int normalized = std::min(N_RING_LEDS, int(log10(abs(score) + 1) * N_RING_LEDS / 4));

		for(int i=0; i<normalized; i++)
			leds[i].b = 255;

		for(int i=normalized; i<N_RING_LEDS; i++)
			leds[i].b = 0;

		FastLED.show();
	}
	else if (parts.at(0) == "bitmap" && parts.size() == 3 && parts.at(2).size() == 16) {
		int col = atoi(parts.at(1).c_str());

		uint8_t bitmap[8] { 0 };
		hextobitmap(parts.at(2), bitmap);

		for(int i=col + 0; i<col + 8; i++)
			mx.setColumn(i, bitmap[i - col]);

		mx.transform(col / 8, col / 8, MD_MAX72XX::transformType_t::TRC);
	}
	else {
		static bool error_state = true;
		memset(leds, error_state ? 0xff : 0x00, sizeof leds);
		error_state = !error_state;

		FastLED.show();
	}
}
