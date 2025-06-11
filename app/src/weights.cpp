// xxd -c 27 -i < quantised.bin > weights.cpp
alignas(64) constexpr const uint8_t weights_data[] {
#embed "quantised.bin"
};
