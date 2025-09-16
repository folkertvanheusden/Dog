#pragma once

#if defined(ESP32)
constexpr int HIDDEN_SIZE = 128;
#else
constexpr int HIDDEN_SIZE = 256;
#endif
