#include <FastLED.h>

#define N_RING_LEDS 64
CRGB leds[N_RING_LEDS];

uint32_t counts[8][8];

void setup() {
  Serial.begin(1000000);

  // ledring
  FastLED.addLeds<NEOPIXEL, 2>(leds, N_RING_LEDS);  // IO2, N_RING_LEDS leds
  FastLED.setBrightness(32);

  memset(leds, 0x00, sizeof leds);
  FastLED.show();

  memset(counts, 0x00, sizeof counts);

  Serial.println(F("Go!"));
}

char buffer[128] { 0 };
int offset = 0;

uint64_t hextov(const char *v)
{
  uint64_t vo = 0;

  while(isalnum(*v)) {
    vo <<= 4;
    int c = toupper(*v);
    v++;
    vo |= c > '9' ? c - 'A' + 10 : (c - '0');
  }

  return vo;
}

CRGB hlsToRGB(int h, int l, int s) {
    int r, g, b;

    if (s == 0) {
        // Achromatic case (gray)
        r = g = b = (l * 255) / 100;
    } else {
        auto hueToRGB = [](int p, int q, int t) -> int {
            if (t < 0) t += 360;
            if (t > 360) t -= 360;
            if (t < 60)  return p + (q - p) * t / 60;
            if (t < 180) return q;
            if (t < 240) return p + (q - p) * (240 - t) / 60;
            return p;
        };

        int q = (l < 50) ? (l * (100 + s)) / 100 : (l + s - (l * s) / 100);
        int p = 2 * l - q;

        r = hueToRGB(p, q, h + 120);
        g = hueToRGB(p, q, h);
        b = hueToRGB(p, q, h - 120);

        // Scale to 0-255
        r = (r * 255) / 100;
        g = (g * 255) / 100;
        b = (b * 255) / 100;
    }

    return {r, g, b};
}

void loop() {
  if (Serial.available() == 0)
    return;

  char c = Serial.read();

  if (c >= 32 && c < 127) {
    if (offset < sizeof(buffer) - 1) {
      buffer[offset++] = c;

//      Serial.write(c);
    }

    return;
  }

  if (c != 10)
    return;

  // set count
  if (buffer[0] == 'M') {
    uint32_t move_ = hextov(&buffer[1]);

    counts[(move_ >> 3) & 7][move_ & 7]++;  // from
    counts[(move_ >> 9) & 7][(move_ >> 6) & 7]++;  // to
  }

  // determine max val
  uint32_t mv = 0;
  uint32_t miv = -1;
  for(int y=0; y<8; y++) {
    for(int x=0; x<8; x++) {
      mv = max(mv, counts[y][x]);
      miv = min(miv, counts[y][x]);
    }
  }

  uint16_t diff = mv - miv;
  if (diff == 0)
    return;

  // put pixels
  byte i = 0;
  for(int y=0; y<8; y++) {
    for(int x=0; x<8; x++) {
      uint16_t v = (counts[y][x] - miv) * uint64_t(360) / diff;
      leds[i++] = hlsToRGB(v, 50, 100);
    }
  }

  FastLED.show();
}
