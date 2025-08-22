#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>

#include "main.h"
#include "nnue.h"
#include "weights.cpp"

struct Network {
	Accumulator feature_weights[2 * 6 * 64];
	Accumulator feature_bias;
	Accumulator output_weights[2];
	std::int16_t output_bias;

	int IRAM_ATTR evaluate(const Accumulator& us, const Accumulator& them) const {
		static_assert(sizeof(Network) == 197440);

		int output = 0;

		// side to move
		for (int i = 0; i < HIDDEN_SIZE; i++) {
			std::int16_t input = std::clamp(us.vals[i], std::int16_t{0}, QA);
			std::int16_t weight = input * this->output_weights[0].vals[i];
			output += int{input} * int{weight};
		}

		// not side to move
		for (int i = 0; i < HIDDEN_SIZE; i++) {
			std::int16_t input = std::clamp(them.vals[i], std::int16_t{0}, QA);
			std::int16_t weight = input * this->output_weights[1].vals[i];
			output += int{input} * int{weight};
		}

		output /= int{QA};
		output += this->output_bias;
		output *= SCALE;
		output /= int{QA} * int{QB};

		return std::clamp(output, -max_non_mate, max_non_mate);
	}

	void add_feature(Accumulator& acc, const int feature_idx) const {
		for (int i = 0; i < HIDDEN_SIZE; i++) {
			acc.vals[i] += this->feature_weights[feature_idx].vals[i];
		}
	}

	void remove_feature(Accumulator& acc, const int feature_idx) const {
		for (int i = 0; i < HIDDEN_SIZE; i++) {
			acc.vals[i] -= this->feature_weights[feature_idx].vals[i];
		}
	}
};

const Network *const NNUE = reinterpret_cast<const Network *>(weights_data);

Eval::Eval()
{
	reset();
}

Eval::Eval(const libchess::Position & pos)
{
	set(pos);
}

void Eval::reset()
{
	this->white = NNUE->feature_bias;
	this->black = NNUE->feature_bias;
}

void Eval::set(const libchess::Position & pos)
{
	reset();

        for(libchess::PieceType type : libchess::constants::PIECE_TYPES) {
                libchess::Bitboard piece_bb_w = pos.piece_type_bb(type, libchess::constants::WHITE);
                while (piece_bb_w) {
                        libchess::Square sq = piece_bb_w.forward_bitscan();
                        piece_bb_w.forward_popbit();
                        add_piece(type, sq, true);
                }

                libchess::Bitboard piece_bb_b = pos.piece_type_bb(type, libchess::constants::BLACK);
                while (piece_bb_b) {
                        libchess::Square sq = piece_bb_b.forward_bitscan();
                        piece_bb_b.forward_popbit();
                        add_piece(type, sq, false);
                }
        }
}

int IRAM_ATTR Eval::evaluate(bool white_to_move) const
{
	if (white_to_move)
		return NNUE->evaluate(this->white, this->black);

	return NNUE->evaluate(this->black, this->white);
}

void Eval::add_piece(const int piece, const int square, const bool is_white)
{
	assert(piece >= 0 && piece < 6);
	if (is_white) {
		NNUE->add_feature(this->white, 64 * piece + square);
		NNUE->add_feature(this->black, 64 * (6 + piece) + (square ^ 56));
	}
	else {
		NNUE->add_feature(this->black, 64 * piece + (square ^ 56));
		NNUE->add_feature(this->white, 64 * (6 + piece) + square);
	}
}

void Eval::remove_piece(const int piece, const int square, const bool is_white)
{
	assert(piece >= 0 && piece < 6);
	if (is_white) {
		NNUE->remove_feature(this->white, 64 * piece + square);
		NNUE->remove_feature(this->black, 64 * (6 + piece) + (square ^ 56));
	}
	else {
		NNUE->remove_feature(this->black, 64 * piece + (square ^ 56));
		NNUE->remove_feature(this->white, 64 * (6 + piece) + square);
	}
}
