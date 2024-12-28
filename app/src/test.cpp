#include <thread>

#include <libchess/Position.h>

#include "eval.h"
#include "main.h"
#include "search.h"


void tests()
{
#define my_assert(x) \
	if (!(x)) { \
		fprintf(stderr, "assert fail at line %d (%s) in %s\n", __LINE__, __func__, __FILE__); \
		exit(1); \
	}

	set_thread_name("TESTS");

	sp1.parameters = &default_parameters;
	sp1.is_t2 = false;
	
	printf("Size of int must be 32 bit\n");
	my_assert(sizeof(int) == 4);
	printf("Ok\n");

	// these are from https://github.com/kz04px/rawr/blob/master/tests/search.rs#L14
	// - mate in 1
	const std::vector<std::pair<std::string, std::string> > mate_in_1 {
            {"6k1/R7/6K1/8/8/8/8/8 w - - 0 1", "a7a8"},
            {"8/8/8/8/8/6k1/r7/6K1 b - - 0 1", "a2a1"},
            {"6k1/4R3/6K1/q7/8/8/8/8 w - - 0 1", "e7e8"},
            {"8/8/8/8/Q7/6k1/4r3/6K1 b - - 0 1", "e2e1"},
            {"6k1/8/6K1/q3R3/8/8/8/8 w - - 0 1", "e5e8"},
            {"8/8/8/8/Q3r3/6k1/8/6K1 b - - 0 1", "e4e1"},
            {"k7/6R1/5R1P/8/8/8/8/K7 w - - 0 1", "f6f8"},
            {"k7/8/8/8/8/5r1p/6r1/K7 b - - 0 1", "f3f1"},
	};

	for(auto & entry: mate_in_1) {
		printf("Testing \"%s\" for mate-in-1\n", entry.first.c_str());
		libchess::Position p { entry.first };
		p.make_move(*libchess::Move::from(entry.second));
		my_assert(p.game_state() == libchess::Position::GameState::CHECKMATE);
	}

	printf("Ok\n");

	// - underpromotions
	const std::vector<std::pair<std::string, std::string> > underpromotions {
            {"8/5P1k/8/4B1K1/8/1B6/2N5/8 w - - 0 1", "f7f8n"},
            {"8/2n5/1b6/8/4b1k1/8/5p1K/8 b - - 0 1", "f2f1n"},
	};

	for(auto & entry: underpromotions) {
		printf("Testing \"%s\" for underpromotions\n", entry.first.c_str());
		libchess::Position p { entry.first };
		my_assert(p.fen() == entry.first);

		clear_flag(sp1.stop);
		memset(sp1.history, 0x00, history_malloc_size);
		libchess::Move best_move  { 0 };
		int            best_score { 0 };
		chess_stats_t  cs         {   };
		std::tie(best_move, best_score) = search_it(&p, 100, false, &sp1, -1, 0, { }, &cs);
		
		my_assert(best_move == *libchess::Move::from(entry.second));

		printf("Ok\n");
	}

	// - move sorting & generation
	{
		printf("move sorting & generation test\n");
		libchess::Position p { "rnbqkbnr/2p1p1pp/1p3p2/p2p4/Q1P1P3/8/PP1P1PPP/RNB1KBNR b KQkq - 0 1" };

		clear_flag(sp1.stop);
		memset(sp1.history, 0x00, history_malloc_size);

		libchess::MoveList move_list = p.pseudo_legal_move_list();
		my_assert(move_list.size() == 7);
		sort_movelist_compare smc(&p, &sp1);
		move_list.sort([&smc](const libchess::Move move) { return smc.move_evaluater(move); });

		int prev_v = 32767;
		for(auto & m: move_list) {
			int cur_v = smc.move_evaluater(m);
			my_assert(cur_v <= prev_v);
			prev_v = cur_v;
		}

		printf("Ok\n");
	}

	// eval function: count_board
	{
		printf("count_board test\n");
		libchess::Position p1 { libchess::constants::STARTPOS_FEN };

		int counts[2][6] { };
		count_board(p1, counts);
		my_assert(counts[libchess::constants::WHITE][libchess::constants::PAWN  ] == 8);
		my_assert(counts[libchess::constants::BLACK][libchess::constants::PAWN  ] == 8);
		my_assert(counts[libchess::constants::WHITE][libchess::constants::ROOK  ] == 2);
		my_assert(counts[libchess::constants::BLACK][libchess::constants::ROOK  ] == 2);
		my_assert(counts[libchess::constants::WHITE][libchess::constants::BISHOP] == 2);
		my_assert(counts[libchess::constants::BLACK][libchess::constants::BISHOP] == 2);
		my_assert(counts[libchess::constants::WHITE][libchess::constants::KNIGHT] == 2);
		my_assert(counts[libchess::constants::BLACK][libchess::constants::KNIGHT] == 2);
		my_assert(counts[libchess::constants::WHITE][libchess::constants::QUEEN ] == 1);
		my_assert(counts[libchess::constants::BLACK][libchess::constants::KING  ] == 1);

		printf("Ok\n");
	}

	// eval function: is_piece
	{
		printf("is_piece test\n");

		libchess::Position p1 { libchess::constants::STARTPOS_FEN };
		my_assert(is_piece(p1, libchess::constants::WHITE, libchess::constants::PAWN, 0, 1) == true);  // A2
		my_assert(is_piece(p1, libchess::constants::WHITE, libchess::constants::PAWN, 0, 0) == false);  // A1
		my_assert(is_piece(p1, libchess::constants::BLACK, libchess::constants::PAWN, 0, 6) == true);  // A7
		my_assert(is_piece(p1, libchess::constants::BLACK, libchess::constants::PAWN, 0, 7) == false);  // A8
		my_assert(is_piece(p1, libchess::constants::WHITE, libchess::constants::ROOK, 0, 1) == false);  // A2
		my_assert(is_piece(p1, libchess::constants::WHITE, libchess::constants::ROOK, 0, 0) == true);  // A1
		my_assert(is_piece(p1, libchess::constants::BLACK, libchess::constants::ROOK, 0, 6) == false);  // A7
		my_assert(is_piece(p1, libchess::constants::BLACK, libchess::constants::ROOK, 0, 7) == true);  // A8

		printf("Ok\n");
	}

	// eval function: eval_piece
	{
		printf("eval_piece test\n");

		my_assert(eval_piece(libchess::constants::PAWN,   default_parameters) == TUNE_PAWN  );
		my_assert(eval_piece(libchess::constants::BISHOP, default_parameters) == TUNE_BISHOP);
		my_assert(eval_piece(libchess::constants::QUEEN,  default_parameters) == TUNE_QUEEN );
		my_assert(eval_piece(libchess::constants::KING,   default_parameters) == 10000      );
		my_assert(eval_piece(libchess::constants::ROOK,   default_parameters) == TUNE_ROOK  );
		my_assert(eval_piece(libchess::constants::KNIGHT, default_parameters) == TUNE_KNIGHT);

		printf("Ok\n");
	}

	// eval function: game_phase
	{
		printf("game_phase test\n");

		int counts1[2][6] { };
		my_assert(game_phase(counts1, default_parameters) == 256);

		int counts2[2][6] {
			{ 8, 2, 2, 2, 1, 1 },
			{ 8, 2, 2, 2, 1, 1 }
		};
		my_assert(game_phase(counts2, default_parameters) == 0);

		printf("Ok\n");
	}

	// eval function: scan_pawns
	{
		printf("scan_pawns test\n");

		libchess::Position p1 { "rnbqkbnr/2p1p1pp/1p2p1p1/p7/Q3PP2/4PP2/PP3P2/RNB1KBNR b KQkq - 0 1" };

		int whiteYmax[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
		int blackYmin[8] = { 8, 8, 8, 8, 8, 8, 8, 8 };
		int n_pawns_w[8] { };
		int n_pawns_b[8] { };
		scan_pawns(p1, whiteYmax, blackYmin, n_pawns_w, n_pawns_b);

		my_assert(whiteYmax[0] ==  1);
		my_assert(whiteYmax[1] ==  1);
		my_assert(whiteYmax[2] == -1);
		my_assert(whiteYmax[3] == -1);
		my_assert(whiteYmax[4] ==  3);
		my_assert(whiteYmax[5] ==  3);
		my_assert(whiteYmax[6] == -1);
		my_assert(whiteYmax[7] == -1);

		my_assert(blackYmin[0] == 4);
		my_assert(blackYmin[1] == 5);
		my_assert(blackYmin[2] == 6);
		my_assert(blackYmin[3] == 8);
		my_assert(blackYmin[4] == 5);
		my_assert(blackYmin[5] == 8);
		my_assert(blackYmin[6] == 5);
		my_assert(blackYmin[7] == 6);

		my_assert(n_pawns_w[0] == 1);
		my_assert(n_pawns_w[1] == 1);
		my_assert(n_pawns_w[2] == 0);
		my_assert(n_pawns_w[3] == 0);
		my_assert(n_pawns_w[4] == 2);
		my_assert(n_pawns_w[5] == 3);
		my_assert(n_pawns_w[6] == 0);
		my_assert(n_pawns_w[7] == 0);

		my_assert(n_pawns_b[0] == 1);
		my_assert(n_pawns_b[1] == 1);
		my_assert(n_pawns_b[2] == 1);
		my_assert(n_pawns_b[3] == 0);
		my_assert(n_pawns_b[4] == 2);
		my_assert(n_pawns_b[5] == 0);
		my_assert(n_pawns_b[6] == 2);
		my_assert(n_pawns_b[7] == 1);

		printf("Ok\n");
	}

	// eval function: calc_psq
	{
		printf("calc_psq test\n");

		libchess::Position p1 { "8/8/8/8/8/8/8/4K3 w - - 0 1" };
		my_assert(calc_psq(p1, 0, default_parameters) > calc_psq(p1, 255, default_parameters));

		libchess::Position p2 { "3K4/8/8/8/8/8/8/8 w - - 0 1" };
		my_assert(calc_psq(p1, 255, default_parameters) < calc_psq(p2, 255, default_parameters));

		printf("Ok\n");
	}

	// eval function: passed pawns
	{
		printf("passed pawns test\n");

		{
			int score1 = 0;
			int score2 = 0;

			{
				libchess::Position p1 { "8/4k3/8/3PP3/8/8/8/8 w - - 0 1" };  // has pp

				int whiteYmax[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
				int blackYmin[8] = { 8, 8, 8, 8, 8, 8, 8, 8 };
				int n_pawns_w[8] { };
				int n_pawns_b[8] { };
				scan_pawns(p1, whiteYmax, blackYmin, n_pawns_w, n_pawns_b);
				score1 = calc_passed_pawns(p1, whiteYmax, blackYmin, n_pawns_w, n_pawns_b, default_parameters);
			}

			{
				libchess::Position p1 { "8/2p1k3/8/3PP3/8/8/8/8 w - - 0 1" };  // has 1 less

				int whiteYmax[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
				int blackYmin[8] = { 8, 8, 8, 8, 8, 8, 8, 8 };
				int n_pawns_w[8] { };
				int n_pawns_b[8] { };
				scan_pawns(p1, whiteYmax, blackYmin, n_pawns_w, n_pawns_b);
				score2 = calc_passed_pawns(p1, whiteYmax, blackYmin, n_pawns_w, n_pawns_b, default_parameters);
			}

			my_assert(score2 < score1);
		}

		{
			libchess::Position p1 { "8/2p1k3/8/2PPP3/8/8/8/8 w - - 0 1" };  // has pp

			int whiteYmax[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
			int blackYmin[8] = { 8, 8, 8, 8, 8, 8, 8, 8 };
			int n_pawns_w[8] { };
			int n_pawns_b[8] { };
			scan_pawns(p1, whiteYmax, blackYmin, n_pawns_w, n_pawns_b);
			my_assert(calc_passed_pawns(p1, whiteYmax, blackYmin, n_pawns_w, n_pawns_b, default_parameters) != 0);
		}

		{
			libchess::Position p1 { libchess::constants::STARTPOS_FEN };  // has no pp

			int whiteYmax[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
			int blackYmin[8] = { 8, 8, 8, 8, 8, 8, 8, 8 };
			int n_pawns_w[8] { };
			int n_pawns_b[8] { };
			scan_pawns(p1, whiteYmax, blackYmin, n_pawns_w, n_pawns_b);
			my_assert(calc_passed_pawns(p1, whiteYmax, blackYmin, n_pawns_w, n_pawns_b, default_parameters) == 0);
		}

		printf("Ok\n");
	}

	// eval function: king_shield
	{
		printf("king_shield test\n");

		libchess::Position p1 { "8/1k6/8/8/8/ppp1K3/2P3PP/8 w - - 0 1" };
		my_assert(king_shield(p1, libchess::constants::WHITE) == 0);
		my_assert(king_shield(p1, libchess::constants::BLACK) == 0);

		libchess::Position p2 { "8/8/8/8/1k6/ppp1K3/2P3PP/8 w - - 0 1" };
		my_assert(king_shield(p2, libchess::constants::WHITE) == 0);
		my_assert(king_shield(p2, libchess::constants::BLACK) != 0);

		printf("Ok\n");
	}

	// tt
	{
		printf("tt test\n");

		tti.reset();
		// initial state
		my_assert(tti.lookup(1).has_value() == false);
		my_assert(tti.lookup(0).has_value() == true);

		// just set a record
		{
			tti.store(2, EXACT, 3, 4, *libchess::Move::from("e2e4"));
			my_assert(tti.lookup(0).has_value() == true);
			my_assert(tti.lookup(1).has_value() == false);
			auto record1 = tti.lookup(2);
			my_assert(record1.has_value());
			auto data1 = record1.value();
			my_assert((data1.hash ^ data1.data_.data) == 2);
			my_assert(libchess::Move(data1.data_._data.m) == *libchess::Move::from("e2e4"));
			my_assert(data1.data_._data.depth == 3);
			my_assert(data1.data_._data.score == 4);
			my_assert(data1.data_._data.flags == EXACT);
		}

		// increase age set a record: should replace
		{
			tti.inc_age();
			tti.store(2, EXACT, 9, 10, *libchess::Move::from("h7h5"));
			my_assert(tti.lookup(0).has_value() == true);
			my_assert(tti.lookup(1).has_value() == false);
			auto record2 = tti.lookup(2);
			my_assert(record2.has_value());
			auto data2 = record2.value();
			my_assert((data2.hash ^ data2.data_.data) == 2);
			my_assert(libchess::Move(data2.data_._data.m) == *libchess::Move::from("h7h5"));
			my_assert(data2.data_._data.depth == 9);
			my_assert(data2.data_._data.score == 10);
			my_assert(data2.data_._data.flags == EXACT);
		}

		// set a record with shallower depth: should not replace
		{
			tti.store(2, EXACT, 8, 20, *libchess::Move::from("c1b3"));
			my_assert(tti.lookup(0).has_value() == true);
			my_assert(tti.lookup(1).has_value() == false);
			auto record2 = tti.lookup(2);
			my_assert(record2.has_value());
			auto data2 = record2.value();
			my_assert((data2.hash ^ data2.data_.data) == 2);
			my_assert(libchess::Move(data2.data_._data.m) == *libchess::Move::from("h7h5"));
			my_assert(data2.data_._data.depth == 9);
			my_assert(data2.data_._data.score == 10);
			my_assert(data2.data_._data.flags == EXACT);
		}

		// set a record with bounds: should not replace
		{
			tti.store(2, LOWERBOUND, 9, 20, *libchess::Move::from("a2a4"));
			my_assert(tti.lookup(0).has_value() == true);
			my_assert(tti.lookup(1).has_value() == false);
			auto record2 = tti.lookup(2);
			my_assert(record2.has_value());
			auto data2 = record2.value();
			my_assert((data2.hash ^ data2.data_.data) == 2);
			my_assert(libchess::Move(data2.data_._data.m) == *libchess::Move::from("h7h5"));
			my_assert(data2.data_._data.depth == 9);
			my_assert(data2.data_._data.score == 10);
			my_assert(data2.data_._data.flags == EXACT);
		}
		{
			tti.store(2, UPPERBOUND, 9, 20, *libchess::Move::from("a2a4"));
			my_assert(tti.lookup(0).has_value() == true);
			my_assert(tti.lookup(1).has_value() == false);
			auto record2 = tti.lookup(2);
			my_assert(record2.has_value());
			auto data2 = record2.value();
			my_assert((data2.hash ^ data2.data_.data) == 2);
			my_assert(libchess::Move(data2.data_._data.m) == *libchess::Move::from("h7h5"));
			my_assert(data2.data_._data.depth == 9);
			my_assert(data2.data_._data.score == 10);
			my_assert(data2.data_._data.flags == EXACT);
		}

		// just set a record with upperbound
		{
			tti.store(99, UPPERBOUND, 3, 4, *libchess::Move::from("b1c3"));
			auto record1 = tti.lookup(99);
			my_assert(record1.has_value());
			auto data1 = record1.value();
			my_assert((data1.hash ^ data1.data_.data) == 99);
			my_assert(libchess::Move(data1.data_._data.m) == *libchess::Move::from("b1c3"));
			my_assert(data1.data_._data.depth == 3);
			my_assert(data1.data_._data.score == 4);
			my_assert(data1.data_._data.flags == UPPERBOUND);
		}
		// set an exact record
		{
			tti.store(99, EXACT, 3, 7, *libchess::Move::from("h7h5"));
			auto record1 = tti.lookup(99);
			my_assert(record1.has_value());
			auto data1 = record1.value();
			my_assert((data1.hash ^ data1.data_.data) == 99);
			my_assert(libchess::Move(data1.data_._data.m) == *libchess::Move::from("h7h5"));
			my_assert(data1.data_._data.depth == 3);
			my_assert(data1.data_._data.score == 7);
			my_assert(data1.data_._data.flags == EXACT);
		}

		printf("Ok\n");
	}
}

void run_tests()
{
	// because of ESP32 stack
	auto th = new std::thread{tests};
	th->join();
	delete th;
}
