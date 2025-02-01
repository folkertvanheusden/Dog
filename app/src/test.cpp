#include <cinttypes>
#include <thread>

#include <libchess/Position.h>

#include "eval.h"
#include "main.h"
#include "nnue.h"
#include "san.h"
#include "search.h"
#include "str.h"


#define my_assert(x) \
	if (!(x)) { \
		fprintf(stderr, "assert fail at line %d (%s) in %s\n", __LINE__, __func__, __FILE__); \
		delete_threads(); \
		exit(1); \
	}

int get_nnue_score(libchess::Position &pos)
{
	Eval e;
	e.set(pos);
	return nnue_evaluate(&e, pos);
}

uint64_t do_nnue_verify_perft(Eval *const ev, libchess::Position &pos, int depth, const int max_depth)
{
        libchess::MoveList move_list = pos.legal_move_list();
        if (depth == 1)
                return move_list.size();

        uint64_t count = 0;
        for(const libchess::Move & move: move_list) {
		auto undo_actions = make_move(ev, pos, move);
                count += do_nnue_verify_perft(ev, pos, depth - 1, max_depth);
		unmake_move(ev, pos, undo_actions);

		{
			int a = 0, b = 0;
			if ((a = get_nnue_score(pos)) != (b = nnue_evaluate(ev, pos))) {
				printf("fail @ %d: %s %s (%d != %d)\n", depth, pos.fen().c_str(), move.to_str().c_str(), a, b);
				for(auto & action: undo_actions) {
					printf("%s: %s %c %s\n", action.is_put ? "ADD":"REM", action.location.to_str().c_str(), action.type.to_char(), action.is_white ? "white":"black");
				}
				my_assert(false);
			}
		}
        }

        return count;
}

void nnue_verify_perft(Eval *const ev, libchess::Position &pos, const std::vector<long> & depths)
{
	ev->set(pos);
	for(size_t i=0; i<depths.size(); i++) {
		uint64_t result = do_nnue_verify_perft(ev, pos, i + 1, i + 1);
		if (result != depths.at(i)) {
			printf("Count mismatch, got %" PRIu64 ", expected %ld\n", result, depths.at(i));
			my_assert(false);
		}
	}
}

void tests()
{
	using namespace libchess;

	set_thread_name("TESTS");

	printf("Size of int must be 32 bit\n");
	my_assert(sizeof(int) == 4);
	printf("OK\n");

	allocate_threads(1);

	{
		printf("NNUE perft\n");

		std::vector<std::pair<std::string, std::vector<long> > > perfts {
			{ constants::STARTPOS_FEN, { 20, 400, 8902, 197281, 4865609 } },
			{ "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", { 48, 2039, 97862, 4085603 } },
			{ "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", { 14, 191, 2812, 43238, 674624, 11030083 } },
			{ "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", { 6, 264, 9467, 422333, 15833292 } },
			{ "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", { 44, 1486, 62379, 2103487 } },
			{ "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", { 46, 2079, 89890, 3894594 } },
		};

		for(auto & record: perfts) {
			Eval *e = new Eval();
			Position pos { record.first };
			e->set(pos);
			nnue_verify_perft(e, pos, record.second);
			delete e;
		}

		printf("OK\n");
	}

	// NNUE incremental
	{
		printf("NNUE incremental update test\n");

		int before = nnue_evaluate(sp.at(0)->ev, sp.at(0)->pos);
		std::string before_str = sp.at(0)->pos.fen();

		// depth 1
		{
			for(auto & move: sp.at(0)->pos.legal_move_list()) {
				auto undo_actions = make_move(sp.at(0)->ev, sp.at(0)->pos, move);
				my_assert(sp.at(0)->pos.fen() != before_str);
				unmake_move(sp.at(0)->ev, sp.at(0)->pos, undo_actions);
				my_assert(before == nnue_evaluate(sp.at(0)->ev, sp.at(0)->pos));
			}
		}

		// depth 2
		{
			auto undo_actions1 = make_move(sp.at(0)->ev, sp.at(0)->pos, { constants::D2, constants::D4, Move::Type::DOUBLE_PUSH });
			std::string before_str2 = sp.at(0)->pos.fen();
			for(auto & move: sp.at(0)->pos.legal_move_list()) {
				auto undo_actions2 = make_move(sp.at(0)->ev, sp.at(0)->pos, move);
				my_assert(sp.at(0)->pos.fen() != before_str2);
				unmake_move(sp.at(0)->ev, sp.at(0)->pos, undo_actions2);
			}
			unmake_move(sp.at(0)->ev, sp.at(0)->pos, undo_actions1);
			my_assert(before == nnue_evaluate(sp.at(0)->ev, sp.at(0)->pos));
		}

		// generic position & promotion
		{
			sp.at(0)->pos = Position("8/5P1k/8/4B1K1/8/1B6/2N5/8 w - - 0 1");
			init_move(sp.at(0)->ev, sp.at(0)->pos);
			int before2 = nnue_evaluate(sp.at(0)->ev, sp.at(0)->pos);
			auto undo_actions1 = make_move(sp.at(0)->ev, sp.at(0)->pos, { constants::E5, constants::B8, Move::Type::NORMAL });
			my_assert(sp.at(0)->pos.fen() != before_str);
			auto undo_actions2 = make_move(sp.at(0)->ev, sp.at(0)->pos, { constants::F7, constants::F8, constants::ROOK, Move::Type::PROMOTION });
			my_assert(sp.at(0)->pos.fen() != before_str);
			unmake_move(sp.at(0)->ev, sp.at(0)->pos, undo_actions2);
			unmake_move(sp.at(0)->ev, sp.at(0)->pos, undo_actions1);
			my_assert(before2 == nnue_evaluate(sp.at(0)->ev, sp.at(0)->pos));
		}

		// promotion with capture
		{
			sp.at(0)->pos = Position("4b3/5P1k/8/6K1/8/1B6/2N5/8 w - - 0 1");
			init_move(sp.at(0)->ev, sp.at(0)->pos);
			int before2 = nnue_evaluate(sp.at(0)->ev, sp.at(0)->pos);
			auto undo_actions1 = make_move(sp.at(0)->ev, sp.at(0)->pos, { constants::F7, constants::E8, constants::ROOK, Move::Type::CAPTURE_PROMOTION });
			my_assert(sp.at(0)->pos.fen() != before_str);
			unmake_move(sp.at(0)->ev, sp.at(0)->pos, undo_actions1);
			my_assert(before2 == nnue_evaluate(sp.at(0)->ev, sp.at(0)->pos));
		}

		// castling
		{
			sp.at(0)->pos = Position("rnbqkbnr/p1p1p1pp/1p1p1p2/8/4P3/3B3N/PPPP1PPP/RNBQK2R w KQkq - 0 4");
			init_move(sp.at(0)->ev, sp.at(0)->pos);
			int before2 = nnue_evaluate(sp.at(0)->ev, sp.at(0)->pos);
			auto undo_actions1 = make_move(sp.at(0)->ev, sp.at(0)->pos, { constants::E1, constants::G1, Move::Type::CASTLING });
			my_assert(sp.at(0)->pos.fen() != before_str);
			unmake_move(sp.at(0)->ev, sp.at(0)->pos, undo_actions1);
			my_assert(before2 == nnue_evaluate(sp.at(0)->ev, sp.at(0)->pos));
		}

		// en-passant
		{
			sp.at(0)->pos = Position("rnbqkbnr/p1ppp1pp/1p3p2/4P3/8/3B3N/PPPP1PPP/RNBQK2R b KQkq - 0 1");
			init_move(sp.at(0)->ev, sp.at(0)->pos);
			int before2 = nnue_evaluate(sp.at(0)->ev, sp.at(0)->pos);
			auto undo_actions1 = make_move(sp.at(0)->ev, sp.at(0)->pos, { constants::D7, constants::D5, Move::Type::NORMAL });
			my_assert(sp.at(0)->pos.fen() != before_str);
			auto undo_actions2 = make_move(sp.at(0)->ev, sp.at(0)->pos, { constants::E5, constants::D6, Move::Type::ENPASSANT });
			my_assert(sp.at(0)->pos.fen() != before_str);
			unmake_move(sp.at(0)->ev, sp.at(0)->pos, undo_actions2);
			unmake_move(sp.at(0)->ev, sp.at(0)->pos, undo_actions1);
			my_assert(before2 == nnue_evaluate(sp.at(0)->ev, sp.at(0)->pos));
		}

		printf("OK\n");
	}

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
		Position p { entry.first };
		p.make_move(*Move::from(entry.second));
		my_assert(p.game_state() == Position::GameState::CHECKMATE);
	}

	printf("OK\n");

	// - underpromotions
	const std::vector<std::pair<std::string, std::string> > underpromotions {
            {"8/5P1k/8/4B1K1/8/1B6/2N5/8 w - - 0 1", "f7f8n"},
            {"8/2n5/1b6/8/4b1k1/8/5p1K/8 b - - 0 1", "f2f1n"},
	};

	for(auto & entry: underpromotions) {
		printf("Testing \"%s\" for underpromotions\n", entry.first.c_str());
		sp.at(0)->pos = Position { entry.first };
		my_assert(sp.at(0)->pos.fen() == entry.first);

		clear_flag(sp.at(0)->stop);
		memset(sp.at(0)->history, 0x00, history_malloc_size);
		Move best_move  { 0 };
		int            best_score { 0 };
		std::tie(best_move, best_score) = search_it(100, false, sp.at(0), -1, { }, false);
		
		my_assert(best_move == *Move::from(entry.second));

		printf("OK\n");
	}

	// - move sorting & generation
	{
		printf("move sorting & generation test\n");
		sp.at(0)->pos = Position { "rnbqkbnr/2p1p1pp/1p3p2/p2p4/Q1P1P3/8/PP1P1PPP/RNB1KBNR b KQkq - 0 1" };

		clear_flag(sp.at(0)->stop);
		memset(sp.at(0)->history, 0x00, history_malloc_size);

		MoveList move_list = sp.at(0)->pos.pseudo_legal_move_list();
		my_assert(move_list.size() == 7);
		sort_movelist_compare smc(*sp.at(0));
		move_list.sort([&smc](const Move move) { return smc.move_evaluater(move); });

		int prev_v = 32767;
		for(auto & m: move_list) {
			int cur_v = smc.move_evaluater(m);
			my_assert(cur_v <= prev_v);
			prev_v = cur_v;
		}

		printf("OK\n");
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
			tti.store(2, EXACT, 3, 4, *Move::from("e2e4"));
			my_assert(tti.lookup(0).has_value() == false);
			my_assert(tti.lookup(1).has_value() == false);
			my_assert(tti.lookup(2).has_value() == true);
			my_assert(tti.lookup(3).has_value() == false);
			auto record1 = tti.lookup(2);
			my_assert(record1.has_value());
			auto data1 = record1.value();
			my_assert(Move(data1.m) == *Move::from("e2e4"));
			my_assert(data1.depth == 3);
			my_assert(data1.score == 4);
			my_assert(data1.flags == EXACT);
		}

		printf("OK\n");
	}

	// bool is_insufficient_material_draw(const Position & pos)
	{
		printf("is_insufficient_material_draw test\n");

		// start position
		{
			Position p1 { constants::STARTPOS_FEN };
			my_assert(is_insufficient_material_draw(p1) == false);
		}

		// two kings
		{
			Position p1 { "8/8/8/2k5/8/5K2/8/8 w - - 0 1" };
			my_assert(is_insufficient_material_draw(p1) == true);
		}

		// A king + any(pawn, rook, queen) is sufficient.
		{
			std::vector<std::string> tests { "8/8/5p2/2k5/8/5K2/8/8 w - - 0 1", "8/8/5R2/2k5/8/5K2/8/8 w - - 0 1", "8/8/5Q2/2k5/8/5K2/8/8 w - - 0 1" };
			for(auto & test: tests) {
				// printf(" %s\n", test.c_str());
				Position p1 { test };
				my_assert(is_insufficient_material_draw(p1) == false);
			}
		}

		// A king and more than one other type of piece is sufficient (e.g. knight + bishop).
		{
			Position p1 { "8/8/5nb1/2k5/8/5K2/8/8 w - - 0 1" };
			my_assert(is_insufficient_material_draw(p1) == false);
		}

		// A king and two (or more) knights is sufficient.
		{
			Position p1 { "8/8/5nn1/2k5/8/5K2/8/8 w - - 0 1" };
			my_assert(is_insufficient_material_draw(p1) == false);
		}

		// King + knight against king + any(rook, bishop, knight, pawn) is sufficient.
		{
			std::vector<std::string> tests { "8/8/5nR1/2k5/8/5K2/8/8 w - - 0 1", "8/8/5nB1/2k5/8/5K2/8/8 w - - 0 1", "8/8/5nN1/2k5/8/5K2/8/8 w - - 0 1", "8/8/5nP1/2k5/8/5K2/8/8 w - - 0 1" };
			for(auto & test: tests) {
				// printf(" %s\n", test.c_str());
				Position p1 { test };
				my_assert(is_insufficient_material_draw(p1) == false);
			}
		}

		// King + bishop against king + any(knight, pawn) is sufficient.
		{
			std::vector<std::string> tests { "8/8/2b5/2k5/5N2/5K2/8/8 w - - 0 1", "8/8/2b5/2k5/5P2/5K2/8/8 w - - 0 1" };
			for(auto & test: tests) {
				// printf(" %s\n", test.c_str());
				Position p1 { test };
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
				Position p1 { test };
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
				Position p1 { test };
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
				Position p1 { test };
				my_assert(is_insufficient_material_draw(p1) == false);
			}
		}

		printf("OK\n");
	}

	// san
	std::vector<std::tuple<const std::string, const std::string, const std::string, int> > san_parsing_tests {
		{ "7r/3r1p1p/6p1/1p6/2B5/5PP1/1Q5P/1K1k4 b - - 0 38", "bxc4", "7r/3r1p1p/6p1/8/2p5/5PP1/1Q5P/1K1k4 w - - 0 39", -212 },
		{ "2n1r1n1/1p1k1p2/6pp/R2pP3/3P4/8/5PPP/2R3K1 b - - 0 30", "Nge7", "2n1r3/1p1knp2/6pp/R2pP3/3P4/8/5PPP/2R3K1 w - - 1 31", 270 },
		{ "8/5p2/1kn1r1n1/1p1pP3/6K1/8/4R3/5R2 b - - 9 60", "Ngxe5+", "8/5p2/1kn1r3/1p1pn3/6K1/8/4R3/5R2 w - - 0 61", 1218 },
		{ "r3k2r/pp1bnpbp/1q3np1/3p4/3N1P2/1PP1Q2P/P1B3P1/RNB1K2R b KQkq - 5 15", "Ng8", "r3k1nr/pp1bnpbp/1q4p1/3p4/3N1P2/1PP1Q2P/P1B3P1/RNB1K2R w KQkq - 6 16", 313 },
		{ libchess::constants::STARTPOS_FEN, "e4", "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1", 48 },  // -45 afterwards
	};

	{
		printf("SAN parsing test\n");

		for(auto & test: san_parsing_tests) {
			libchess::Position pos(std::get<0>(test));
			pos.make_move(SAN_to_move(std::get<1>(test), pos).value());
			my_assert(std::get<2>(test) == pos.fen());
		}

		printf("OK\n");
	}

	// NNUE eval (using san parsing data)
	{
		printf("NNUE evaluation test\n");

		for(auto & test: san_parsing_tests) {
			libchess::Position pos(std::get<0>(test));
			init_move(sp.at(0)->ev, pos);
			my_assert(nnue_evaluate(sp.at(0)->ev, pos) == std::get<3>(test));
		}

		printf("OK\n");
	}

	delete_threads();
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
		sp.at(0)->pos = positions.at(i).first;
		auto rc  = search_it(search_time, false, sp.at(0), -1, { }, false);

		bool hit = abs(rc.second) >= 9800;
		mates_found += hit;
	}

	printf("%d %.2f %zu\n", mates_found, mates_found * 100. / n, n);
}
#endif
