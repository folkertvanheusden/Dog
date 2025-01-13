#include <thread>

#include <libchess/Position.h>

#include "eval.h"
#include "main.h"
#include "san.h"
#include "search.h"
#include "str.h"


void tests()
{
#define my_assert(x) \
	if (!(x)) { \
		fprintf(stderr, "assert fail at line %d (%s) in %s\n", __LINE__, __func__, __FILE__); \
		exit(1); \
	}

	set_thread_name("TESTS");

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

		clear_flag(sp.at(0)->stop);
		memset(sp.at(0)->history, 0x00, history_malloc_size);
		libchess::Move best_move  { 0 };
		int            best_score { 0 };
		std::tie(best_move, best_score) = search_it(p, 100, false, sp.at(0), -1, 0, { }, false);
		
		my_assert(best_move == *libchess::Move::from(entry.second));

		printf("Ok\n");
	}

	// - move sorting & generation
	{
		printf("move sorting & generation test\n");
		libchess::Position p { "rnbqkbnr/2p1p1pp/1p3p2/p2p4/Q1P1P3/8/PP1P1PPP/RNB1KBNR b KQkq - 0 1" };

		clear_flag(sp.at(0)->stop);
		memset(sp.at(0)->history, 0x00, history_malloc_size);

		libchess::MoveList move_list = p.pseudo_legal_move_list();
		my_assert(move_list.size() == 7);
		sort_movelist_compare smc(p, *sp.at(0));
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
			my_assert(tti.lookup(0).has_value() == false);
			my_assert(tti.lookup(1).has_value() == false);
			my_assert(tti.lookup(2).has_value() == true);
			my_assert(tti.lookup(3).has_value() == false);
			auto record1 = tti.lookup(2);
			my_assert(record1.has_value());
			auto data1 = record1.value();
			my_assert((data1.hash ^ data1.data_.data) == 2);
			my_assert(libchess::Move(data1.data_._data.m) == *libchess::Move::from("e2e4"));
			my_assert(data1.data_._data.depth == 3);
			my_assert(data1.data_._data.score == 4);
			my_assert(data1.data_._data.flags == EXACT);
		}

		printf("Ok\n");
	}

	// bool is_insufficient_material_draw(const libchess::Position & pos)
	{
		printf("is_insufficient_material_draw test\n");

		// start position
		{
			libchess::Position p1 { libchess::constants::STARTPOS_FEN };
			my_assert(is_insufficient_material_draw(p1) == false);
		}

		// two kings
		{
			libchess::Position p1 { "8/8/8/2k5/8/5K2/8/8 w - - 0 1" };
			my_assert(is_insufficient_material_draw(p1) == true);
		}

		// A king + any(pawn, rook, queen) is sufficient.
		{
			std::vector<std::string> tests { "8/8/5p2/2k5/8/5K2/8/8 w - - 0 1", "8/8/5R2/2k5/8/5K2/8/8 w - - 0 1", "8/8/5Q2/2k5/8/5K2/8/8 w - - 0 1" };
			for(auto & test: tests) {
				// printf(" %s\n", test.c_str());
				libchess::Position p1 { test };
				my_assert(is_insufficient_material_draw(p1) == false);
			}
		}

		// A king and more than one other type of piece is sufficient (e.g. knight + bishop).
		{
			libchess::Position p1 { "8/8/5nb1/2k5/8/5K2/8/8 w - - 0 1" };
			my_assert(is_insufficient_material_draw(p1) == false);
		}

		// A king and two (or more) knights is sufficient.
		{
			libchess::Position p1 { "8/8/5nn1/2k5/8/5K2/8/8 w - - 0 1" };
			my_assert(is_insufficient_material_draw(p1) == false);
		}

		// King + knight against king + any(rook, bishop, knight, pawn) is sufficient.
		{
			std::vector<std::string> tests { "8/8/5nR1/2k5/8/5K2/8/8 w - - 0 1", "8/8/5nB1/2k5/8/5K2/8/8 w - - 0 1", "8/8/5nN1/2k5/8/5K2/8/8 w - - 0 1", "8/8/5nP1/2k5/8/5K2/8/8 w - - 0 1" };
			for(auto & test: tests) {
				// printf(" %s\n", test.c_str());
				libchess::Position p1 { test };
				my_assert(is_insufficient_material_draw(p1) == false);
			}
		}

		// King + bishop against king + any(knight, pawn) is sufficient.
		{
			std::vector<std::string> tests { "8/8/2b5/2k5/5N2/5K2/8/8 w - - 0 1", "8/8/2b5/2k5/5P2/5K2/8/8 w - - 0 1" };
			for(auto & test: tests) {
				// printf(" %s\n", test.c_str());
				libchess::Position p1 { test };
				my_assert(is_insufficient_material_draw(p1) == false);
			}
		}

		// King + bishop(s) is also sufficient if there's bishops on opposite colours (even king + bishop against king + bishop).

		// tests from https://github.com/toanth/motors/blob/main/gears/src/games/chess.rs#L1564-L1610
		// insufficient
		{
			std::vector<std::string> tests { "8/4k3/8/8/8/8/8/2K5 w - - 0 1",
				"8/4k3/8/8/8/8/5N2/2K5 w - - 0 1",
				"8/8/8/6k1/8/2K5/5b2/6b1 w - - 0 1",  // bishops on same color
				"8/8/3B4/7k/8/8/1K6/6b1 w - - 0 1",
				"8/6B1/8/6k1/8/2K5/8/6b1 w - - 0 1",
				"3b3B/2B5/1B1B4/B7/3b4/4b2k/5b2/1K6 w - - 0 1",  // bishops on same color
				"3B3B/2B5/1B1B4/B6k/3B4/4B3/1K3B2/2B5 w - - 0 1"  // bishops on same color
			};
			for(auto & test: tests) {
				libchess::Position p1 { test };
				my_assert(is_insufficient_material_draw(p1) == true);
			}
		}
		// sufficient
		{
			std::vector<std::string> tests { "8/8/4k3/8/8/1K6/8/7R w - - 0 1",
				"5r2/3R4/4k3/8/8/1K6/8/8 w - - 0 1",
				"8/8/4k3/8/8/1K6/8/6BB w - - 0 1",
				"8/8/4B3/8/8/7K/8/6bk w - - 0 1",
				"3B3B/2B5/1B1B4/B6k/3B4/4B3/1K3B2/1B6 w - - 0 1",
				"8/3k4/8/8/8/8/NNN5/1K6 w - - 0 1"
			};
			for(auto & test: tests) {
				libchess::Position p1 { test };
				my_assert(is_insufficient_material_draw(p1) == false);
			}
		}
		// sufficient but unreasonable
		{
			std::vector<std::string> tests { "6B1/8/8/6k1/8/2K5/8/6b1 w - - 0 1",
				"8/8/4B3/8/8/7K/8/6bk b - - 0 1",
				"8/8/4B3/7k/8/8/1K6/6b1 w - - 0 1",
				"8/3k4/8/8/8/8/1NN5/1K6 w - - 0 1",
				"8/2nk4/8/8/8/8/1NN5/1K6 w - - 0 1",
			};
			for(auto & test: tests) {
				libchess::Position p1 { test };
				my_assert(is_insufficient_material_draw(p1) == false);
			}
		}

		printf("Ok\n");
	}

	// san
	{
		printf("SAN parsing test\n");

		std::vector<std::tuple<const std::string, const std::string, const std::string> > tests {
			{ "7r/3r1p1p/6p1/1p6/2B5/5PP1/1Q5P/1K1k4 b - - 0 38", "bxc4", "7r/3r1p1p/6p1/8/2p5/5PP1/1Q5P/1K1k4 w - - 0 39" },
			{ "2n1r1n1/1p1k1p2/6pp/R2pP3/3P4/8/5PPP/2R3K1 b - - 0 30", "Nge7", "2n1r3/1p1knp2/6pp/R2pP3/3P4/8/5PPP/2R3K1 w - - 1 31" },
			{ "8/5p2/1kn1r1n1/1p1pP3/6K1/8/4R3/5R2 b - - 9 60", "Ngxe5+", "8/5p2/1kn1r3/1p1pn3/6K1/8/4R3/5R2 w - - 0 61" },
			{ "r3k2r/pp1bnpbp/1q3np1/3p4/3N1P2/1PP1Q2P/P1B3P1/RNB1K2R b KQkq - 5 15", "Ng8", "r3k1nr/pp1bnpbp/1q4p1/3p4/3N1P2/1PP1Q2P/P1B3P1/RNB1K2R w KQkq - 6 16" },
		};

		for(auto & test: tests) {
			libchess::Position pos(std::get<0>(test));
			pos.make_move(SAN_to_move(std::get<1>(test), pos).value());
			my_assert(std::get<2>(test) == pos.fen());
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

#if !defined(ESP32)
std::vector<std::pair<libchess::Position, const std::string> > load_epd(const std::string & filename)
{
	std::vector<std::pair<libchess::Position, const std::string> > out;

	FILE *fh = fopen(filename.c_str(), "r");
	while(!feof(fh)) {
		char buffer[4096];
		if (!fgets(buffer, sizeof buffer, fh))
			break;

		auto parts = split(buffer, " ");
		std::string fen = parts[0] + " " + parts[1] + " " + parts[2] + " " + parts[3];

		auto check = parts[4];
		if (check != "bm")
			continue;

		auto   move = parts[5];
		size_t sc   = move.find(';');
		if (sc != std::string::npos)
			move = move.substr(0, sc);

		out.push_back({ libchess::Position(fen), move });
	}

	return out;
}

void test_mate_finder(const std::string & filename, const int search_time)
{
	init_lmr();

	int         mates_found = 0;
	auto        positions   = load_epd(filename);
	size_t      n           = positions.size();
	printf("Loaded %zu tests\n", n);

	for(size_t i=0; i<n; i++) {
		clear_flag(sp.at(0)->stop);
		auto rc  = search_it(positions.at(i).first, search_time, false, sp.at(0), -1, 0, { }, false);

		bool hit = abs(rc.second) >= 9800;
		mates_found += hit;
	}

	printf("%d %.2f %zu\n", mates_found, mates_found * 100. / n, n);
}
#endif
