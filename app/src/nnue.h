#pragma once

constexpr int HIDDEN_SIZE = 128;
constexpr int SCALE = 400;
constexpr std::int16_t QA = 255;
constexpr std::int16_t QB = 64;

struct Accumulator
{
    alignas(64) std::array<std::int16_t, HIDDEN_SIZE> vals;
};

class Eval
{
	// season to taste
	Accumulator white;
	Accumulator black;
public:
	Eval();

	int evaluate(bool white_to_move) const;
	void add_piece(const int piece, const int square, const bool is_white);
	void remove_piece(const int piece, const int square, const bool is_white);
};