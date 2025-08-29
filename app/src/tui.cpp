#include <cassert>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctype.h>
#if defined(ESP32)
#include <esp_chip_info.h>
#include <esp_http_client.h>
#include <esp_wifi.h>
#include <soc/rtc.h>
#endif
#include <thread>
#include <sys/stat.h>

#include <libchess/Position.h>

#include "book.h"
#include "eval.h"
#include "eval-stats.h"
#include "main.h"
#include "max-ascii.h"
#include "nnue.h"
#include "san.h"
#include "search.h"
#include "str.h"
#include "syzygy.h"
#include "tui.h"


#if defined(ESP32)
#define RECALL_FILE "/spiffs/recall.txt"
#else
#define RECALL_FILE ".dog-recall.txt"
#endif

typedef enum { C_INCREMENTAL, C_TOTAL } dog_clock_t;

bool        do_ping    = false;
dog_clock_t clock_type = C_TOTAL;
std::string wifi_ssid;
std::string wifi_psk;

#if defined(ESP32)
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
EventGroupHandle_t  s_wifi_event_group;
int                 s_retry_num        = 0;
constexpr const int max_retry          = 15;

void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
		esp_wifi_connect();
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < max_retry) {
			esp_wifi_connect();
			s_retry_num++;
			my_printf("retring to connect to the AP (%d)\n", max_retry - s_retry_num);
		}
		else {
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
			s_retry_num = 0;
		}
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		my_printf("IP: " IPSTR "\n", IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

esp_err_t _http_event_handler(esp_http_client_event_handle_t evt)
{
	if (evt->event_id == HTTP_EVENT_ON_DATA)
		memcpy(evt->user_data, evt->data, std::min(evt->data_len, 16));

	return ESP_OK;
}
#endif

std::string myformat(const char *const fmt, ...)
{
        char *buffer = nullptr;
        va_list ap;
        va_start(ap, fmt);
        int rc = vasprintf(&buffer, fmt, ap);
        va_end(ap);
	if (rc == -1)
                return fmt;

        std::string result = buffer;
        free(buffer);

        return result;
}

#if defined(ESP32)
std::string push_pgn(const std::string & pgn)
{
	uint64_t org_tt_size = tti.get_size();
	tti.set_size(0);

	my_printf("free heap size: %d, min_free_heap_size: %d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
	my_printf("Connecting to SSID %s...\n", wifi_ssid.c_str());

	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_t *p = esp_netif_create_default_wifi_sta();

	esp_netif_set_hostname(p, "Dog " DOG_VERSION);

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
				ESP_EVENT_ANY_ID,
				&event_handler,
				nullptr,
				&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
				IP_EVENT_STA_GOT_IP,
				&event_handler,
				nullptr,
				&instance_got_ip));
	wifi_config_t wifi_config { };
	strcpy((char *)wifi_config.sta.ssid,     wifi_ssid.c_str());
	strcpy((char *)wifi_config.sta.password, wifi_psk. c_str());

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
			WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
			pdFALSE,
			pdFALSE,
			portMAX_DELAY);

	if (bits & WIFI_CONNECTED_BIT)
		my_printf("Connected to %s\n", wifi_config.sta.ssid);
	else
		my_printf("Connection failed\n");

	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT,   IP_EVENT_STA_GOT_IP, instance_got_ip));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,    instance_any_id));

	vEventGroupDelete(s_wifi_event_group);
	// wifi is setup now

	char recv_buffer[16] { };
	esp_http_client_config_t http_config = {
		.url = "https://vanheusden.com/pgn/",
		.event_handler = _http_event_handler,
		.user_data     = recv_buffer,
	};
	esp_http_client_handle_t client = esp_http_client_init(&http_config);

	// POST
	esp_http_client_set_method(client, HTTP_METHOD_POST);
	esp_http_client_set_header(client, "Content-Type", "application/json");
	esp_http_client_set_post_field(client, pgn.data(), pgn.size());
	esp_err_t err = esp_http_client_perform(client);
	if (err == ESP_OK)
		my_printf("PGN succesfully (%d) submitted\n", esp_http_client_get_status_code(client));
	else {
		my_printf("HTTP POST request failed: %s\n", esp_err_to_name(err));
	}
	esp_http_client_cleanup(client);

	//
	my_printf("stop wifi\n");
	esp_wifi_disconnect();
	esp_wifi_stop();
	esp_netif_destroy_default_wifi(p);
	esp_event_loop_delete_default();

	tti.set_size(org_tt_size);

	if (strncmp(recv_buffer, "OK:", 3) == 0)
		return myformat("https://vanheusden.com/pgn/?%s", &recv_buffer[3]);

	return "";
}
#endif

bool store_position(const std::string & fen, const int total_dog_time)
{
	FILE *fh = fopen(RECALL_FILE, "w");
	if (fh) {
		fprintf(fh, "%s\n", fen.c_str());
		fprintf(fh, "%d\n", total_dog_time);
		fclose(fh);

		return true;
	}

	return false;
}

void to_uart(const char *const buffer, int buffer_len)
{
#if defined(WEMOS32) || defined(ESP32_S3_QTPY) || defined(ESP32_S3_XIAO)
        ESP_ERROR_CHECK(uart_wait_tx_done(uart_num, 100));
        uart_write_bytes(uart_num, buffer, buffer_len);
	if (buffer[buffer_len - 1] == '\n') {
		const char cr = '\r';
		uart_write_bytes(uart_num, &cr, 1);
	}
#endif
}

bool peek_for_ctrl_c()
{
#if defined(WEMOS32) || defined(ESP32_S3_QTPY) || defined(ESP32_S3_XIAO)
	char   buffer = 0;
	size_t length = 0;
	ESP_ERROR_CHECK(uart_get_buffered_data_len(uart_num, (size_t*)&length));
	if (length && uart_read_bytes(uart_num, &buffer, 1, 100))
		return buffer == 3;
#endif

	return false;
}

void my_printf(const char *const fmt, ...)
{
#if defined(ESP32)
        char *buffer = nullptr;
        va_list ap { };
        va_start(ap, fmt);
        auto buffer_len = vasprintf(&buffer, fmt, ap);
        va_end(ap);

        printf("%s", buffer);
	to_uart(buffer, buffer_len);

        free(buffer);
#else
        va_list ap { };
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
#endif
}

uint64_t do_perft(libchess::Position &pos, int depth)
{
	libchess::MoveList move_list = pos.legal_move_list();
	if (depth == 1)
		return move_list.size();

	uint64_t count     = 0;
	for(const libchess::Move & move: move_list) {
		pos.make_move(move);
		count += do_perft(pos, depth - 1);
		pos.unmake_move();
	}

	return count;
}

void perft(libchess::Position &pos, int depth)
{
	my_printf("Perft for fen: %s\n", pos.fen().c_str());

	for(int d=1; d<=depth; d++) {
		uint64_t t_start = esp_timer_get_time();
		uint64_t count   = do_perft(pos, d);
		uint64_t t_end   = esp_timer_get_time();
		double   t_diff  = std::max(uint64_t(1), t_end - t_start) / 1000000.;
		my_printf("%d: %" PRIu64 " (%.3f nps, %.2f seconds)\n", d, count, count / t_diff, t_diff);
	}
}

std::string format_move_and_score(const libchess::Move & m, int16_t score)
{
	return m.to_str() + myformat(" [%5.2f]", score / 100.);
}

void store_cursor_position()
{
	if (t == T_VT100) {
		my_printf("\x1b");
		my_printf("7");
	}
	else if (t == T_ANSI) {
		my_printf("\x1b[s");
	}
}

void restore_cursor_position()
{
	if (t == T_VT100) {
		my_printf("\x1b");
		my_printf("8");
	}
	else if (t == T_ANSI) {
		my_printf("\x1b[u");
	}
}

void display(const libchess::Position & p, const terminal_t t, const std::optional<std::vector<libchess::Move> > & moves, const std::vector<int16_t> & scores)
{
	std::vector<std::string> lines;

	if (t == T_ANSI) {
		std::string line = "\x1b[m\x1b[43;30m    ";
		for(int x=0; x<8; x++)
			line += "   ";
		line += " \x1b[m";
		lines.push_back(line);
	}

	for(int y=7; y>=0; y--) {
		std::string line;
		if (t == T_ANSI)
			line = "\x1b[43;30m " + std::to_string(y + 1) + " |";
		else if (t == T_VT100)
			line = " " + std::to_string(y + 1) + " \x1b(0\x78\x1b(B";
		else
			line = " " + std::to_string(y + 1) + " |";
		for(int x=0; x<8; x++) {
			const auto piece = p.piece_on(libchess::Square::from(libchess::File(x), libchess::Rank(y)).value());
			if (piece.has_value()) {
				int  c        = piece.value().type().to_char();
				bool is_white = piece.value().color() == libchess::constants::WHITE;
				if (t == T_ANSI) {
					if (is_white)
						line += "\x1b[30;47m ";
					else
						line += "\x1b[40;37m ";
				}
				else if (t == T_VT100) {
					if (is_white)
						line += "\x1b[1m ";
					else
						line += "\x1b[m ";
				}
				else {
					line += " ";
				}
				line += char(piece.value().color() == libchess::constants::WHITE ? toupper(c) : c) + std::string(" ");
				if (t == T_ANSI)
					line += "\x1b[43;30m";
				else if (t == T_VT100)
					line += "\x1b[m";
			}
			else {
				line += "   ";
			}
		}
		line += " \x1b[m";
		lines.push_back(line);
	}

	if (t == T_ANSI) {
		std::string line;
		line = "\x1b[43;30m   +";
		for(int x=0; x<8; x++)
			line += "---";
		line += " \x1b[m";
		lines.push_back(line);
		line = "\x1b[43;30m    ";
		for(int x=0; x<8; x++)
			line += std::string(" ") + char('A' + x) + " ";
		line += " \x1b[m";
		lines.push_back(std::move(line));
	}
	else if (t == T_VT100) {
		std::string line;
		line = "   \x1b(0\x6d";
		for(int x=0; x<8; x++)
			line += "\x71\x71\x71";
		line += "\x1b(B ";
		lines.push_back(line);
		line = "    ";
		for(int x=0; x<8; x++)
			line += std::string(" ") + char('A' + x) + " ";
		lines.push_back(std::move(line));
	}
	else {
		std::string line = "   +";
		for(int x=0; x<8; x++)
			line += "---";
		lines.push_back(line);
		line = "    ";
		for(int x=0; x<8; x++)
			line += std::string(" ") + char('A' + x) + " ";
		lines.push_back(std::move(line));
	}

	if (moves.has_value()) {
		auto & refm  = moves.value();
		size_t nrefm = refm.size();
		size_t skip  = 0;
		size_t half  = (nrefm + 1) / 2;
		if (half > lines.size())
			skip = (half - lines.size()) * 2;
		size_t line_nr = 0;
		for(size_t i=skip; i<nrefm; i += 2, line_nr++) {
			std::string add = "  " + std::to_string(i / 2 + 1) + ". " + format_move_and_score(refm.at(i + 0), scores.at(i + 0));
			if (nrefm - i >= 2)
				add += "  " + format_move_and_score(refm.at(i + 1), scores.at(i + 1));
			lines.at(line_nr) += add;
		}
	}

	for(auto & line: lines)
		my_printf("%s\n", line.c_str());

	if (p.game_state() != libchess::Position::GameState::IN_PROGRESS)
		my_printf("Game is finished\n");
	else {
		my_printf("Move number: %d, color: %s, half moves: %d, repetition count: %d\n", p.fullmoves(), p.side_to_move() == libchess::constants::WHITE ? "white":"black", p.halfmoves(), p.repeat_count());
		my_printf("%s\n", p.fen().c_str());
	}
}

// determine how many pieces cannot move because they are protecting an other piece
int get_complexity(const libchess::Position & pos, const libchess::Color & c)
{
	using namespace libchess;
	int  count     = 0;
	auto my_pieces = pos.color_bb(c);
	while(my_pieces) {
		Square my_sq = my_pieces.forward_bitscan();
		my_pieces.forward_popbit();

		Bitboard pinned_bb;
		Bitboard pinners_bb =
			((pos.piece_type_bb(constants::QUEEN) | pos.piece_type_bb(constants::ROOK))   & pos.color_bb(!c) & lookups::rook_attacks(my_sq)) |
			((pos.piece_type_bb(constants::QUEEN) | pos.piece_type_bb(constants::BISHOP)) & pos.color_bb(!c) & lookups::bishop_attacks(my_sq));
		while (pinners_bb) {
			Square sq = pinners_bb.forward_bitscan();
			pinners_bb.forward_popbit();
			Bitboard bb = lookups::intervening(sq, my_sq) & pos.occupancy_bb();
			if (bb.popcount() == 1)
				pinned_bb ^= bb & pos.color_bb(c);
		}

		count += pinned_bb.popcount();
	}

	return count;
}

int get_score(const libchess::Position & pos, const libchess::Move & m)
{
	Eval e(pos);

	libchess::Position work(pos);
	make_move(&e, work, m);

	return -nnue_evaluate(&e, work);
}

int get_score(const libchess::Position & pos, const libchess::Color & c)
{
	Eval e(pos);
	return -nnue_evaluate(&e, c);
}

void emit_pv(Eval *const nnue_eval, const libchess::Position & pos, const libchess::Move & best_move, const terminal_t t)
{
	std::vector<libchess::Move> pv = get_pv_from_tt(pos, best_move);
	auto start_color = pos.side_to_move();
	auto start_score = nnue_evaluate(nnue_eval, pos);

	if (t == T_ANSI || t == T_VT100) {
		if (t == T_ANSI)
			my_printf("\x1b[43;30mPV[%.2f]:\x1b[m\n", start_score / 100.);
		else
			my_printf("PV[%.2f (%c)]:\n", start_score / 100., start_color == libchess::constants::WHITE ? 'W' : 'B');

		int nr = 0;
		libchess::Position work(sp.at(0)->pos);
		for(auto & move: pv) {
			if (++nr == 5)
				my_printf("\n"), nr = 0;
			my_printf(" ");

			work.make_move(move);
			auto cur_color = work.side_to_move();
			int  cur_score = nnue_evaluate(nnue_eval, start_color);

			if ((start_color == cur_color && cur_score < start_score) || (start_color != cur_color && cur_score > start_score))
				my_printf("\x1b[40;31m%s\x1b[m", move.to_str().c_str());
			else if (start_score == cur_score)
				my_printf("%s", move.to_str().c_str());
			else
			{
				if (t == T_ANSI)
					my_printf("\x1b[40;32m%s\x1b[m", move.to_str().c_str());
				else
					my_printf("\x1b[1m%s\x1b[m", move.to_str().c_str());
			}
			if (work.side_to_move() != start_color)
				my_printf(" [%.2f] ", -cur_score / 100.);
			else
				my_printf(" [%.2f] ", cur_score / 100.);
		}
	}
	else {
		my_printf("PV:");
		for(auto & move: pv)
			my_printf(" %s", move.to_str().c_str());
	}
}

std::string perc(const unsigned total, const unsigned part)
{
	return myformat("%.2f%%", part * 100. / total);
}

void show_stats(const libchess::Position & pos, const chess_stats & cs, const bool verbose)
{
	my_printf("Nodes proc.   : %u\n", cs.data.nodes);
	my_printf("QS Nodes proc.: %u\n", cs.data.qnodes);
	my_printf("Standing pats : %u\n", cs.data.n_standing_pat);
	my_printf("Draws         : %u\n", cs.data.n_draws);
	my_printf("TT queries    : %u (total), %s (hits), %u (store), %s (invalid)\n",
			cs.data.tt_query,
			perc(cs.data.tt_query, cs.data.tt_hit).c_str(),
			cs.data.tt_store,
			perc(cs.data.tt_query, cs.data.tt_invalid).c_str());
	my_printf("QS TT queries : %u (total), %s (hits), %u (store)\n",
			cs.data.qtt_query,
			perc(cs.data.qtt_query, cs.data.qtt_hit).c_str(),
			cs.data.qtt_store);
	my_printf("TT cut-off    : %s (search), %s (qs)\n",
			perc(cs.data.tt_query,  cs.data.tt_cutoff ).c_str(),
			perc(cs.data.qtt_query, cs.data.qtt_cutoff).c_str());
	my_printf("Null moves    : %u\n", cs.data.n_null_move_hit);
	my_printf("LMR           : %u (total), %s (hits)\n",
			cs.data.n_lmr, perc(cs.data.n_lmr, cs.data.n_lmr_hit).c_str());
	my_printf("Static eval   : %u (total), %s (hits)\n",
			cs.data.n_static_eval, perc(cs.data.n_static_eval, cs.data.n_static_eval_hit).c_str());
	if (cs.data.nmc_nodes)
		my_printf("Avg. move c/o : %.2f\n", cs.data.n_moves_cutoff / double(cs.data.nmc_nodes));
	if (cs.data.nmc_qnodes)
                my_printf("Avg.qs c/o    : %.2f\n", cs.data.n_qmoves_cutoff / double(cs.data.nmc_qnodes));
	if (verbose) {
#if defined(ESP32)
		my_printf("RAM           : %u (min free), %u (largest free)\n", uint32_t(heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT)),
				heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
		my_printf("SOC           : ");
		esp_chip_info_t chip_info;
		esp_chip_info(&chip_info);
		switch (chip_info.model) {
			case CHIP_ESP32S3:  my_printf("ESP32-S3"); break;
			case CHIP_ESP32C3:  my_printf("ESP32-C3"); break;
			case CHIP_ESP32C2:  my_printf("ESP32-C2"); break;
			case CHIP_ESP32C6:  my_printf("ESP32-C6"); break;
			case CHIP_ESP32H2:  my_printf("ESP32-H2"); break;
			case CHIP_ESP32P4:  my_printf("ESP32-P4"); break;
			case CHIP_ESP32C5:  my_printf("ESP32-C5"); break;
			case CHIP_ESP32C61: my_printf("ESP32-C61"); break;
					    //case CHIP_ESP32H21: my_printf("ESP32-H21"); break;
			default: my_printf("UNKNOWN"); break;
		}
		rtc_cpu_freq_config_t conf;
		rtc_clk_cpu_freq_get_config(&conf);
		my_printf(" @ %u MHz with %u threads\n", unsigned(conf.freq_mhz), unsigned(sp.size()));
#endif
	}
	my_printf("Game phase    : %d (0...255)\n", game_phase(pos));
	auto mobility = count_mobility(pos);
	my_printf("Mobility      : %d/%d (w/b)\n", mobility.first, mobility.second);
	auto dev      = development(pos);
	my_printf("Development   : %d/%d (w/b)\n", dev.first, dev.second);
	int complexity_w = get_complexity(sp.at(0)->pos, libchess::constants::WHITE) * 100 / 32;
	int complexity_b = get_complexity(sp.at(0)->pos, libchess::constants::BLACK) * 100 / 32;
	my_printf("Pos.complexity: %d (white), %d (black)\n", complexity_w, complexity_b);
}

void show_movelist(const libchess::Position & pos)
{
	int nr = 0;
	auto moves = pos.legal_move_list();
	for(auto & move: moves) {
		if (nr != 0)
			my_printf(" ");
		my_printf("%s", move.to_str().c_str());
		if (++nr == 13) {
			my_printf("\n");
			nr = 0;
		}
	}
	my_printf("\n");
}

void tt_lookup()
{
	auto te = tti.lookup(sp.at(0)->pos.hash());
	if (te.has_value() == false)
		my_printf("None\n");
	else {
		const char *const flag_names[] = { "invalid", "exact", "lowerbound", "upperbound" };
		my_printf("Score: %.2f (%s)\n", te.value().score / 100., flag_names[te.value().flags]);
		my_printf("Depth: %d\n", te.value().depth);
		std::optional<libchess::Move> tt_move;
		if (te.value().M)
			tt_move = uint_to_libchessmove(te.value().M);
		if (tt_move.has_value() && sp.at(0)->pos.is_legal_move(tt_move.value()))
			my_printf("Move: %s\n", tt_move.value().to_str().c_str());
	}
}

void do_syzygy(const libchess::Position & pos)
{
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__) || defined(__APPLE__)
	if (with_syzygy) {
		std::optional<std::pair<libchess::Move, int> > s_root = probe_fathom_root(pos);
		if (s_root.has_value())
			my_printf("Syzygy move + score for current position: %.2f for %s\n", s_root.value().second / 100., s_root.value().first.to_str().c_str());
		else
			my_printf("No Syzygy move + score for current position.\n");

		std::optional<int> s_nonroot = probe_fathom_nonroot(pos);
		if (s_nonroot.has_value())
			my_printf("Syzygy score for current position: %.2f\n", s_nonroot.value() / 100.);
		else
			my_printf("No Syzygy score for current position.\n");
	}
	else
#endif
	{
		my_printf("No syzygy available\n");
	}
}

void press_any_key()
{
	my_printf("Press any key... ");

	char c = 0;
	if (is.get(c))
		my_printf("\n");
}

void compare_moves(const libchess::Position & pos, libchess::Move & m, int *const expected_move_count)
{
	auto tt_rc = tti.lookup(pos.hash());
	if (tt_rc.has_value() == false || tt_rc.value().M == 0)
		return;

	auto tt_move = libchess::Move(uint_to_libchessmove(tt_rc.value().M));
	if (tt_move != m) {
		int eval_me  = get_score(sp.at(0)->pos, tt_move);
		int eval_opp = get_score(sp.at(0)->pos, m);
		if (eval_opp > eval_me)
			my_printf("Very good!\n");
		else if (eval_opp < eval_me)
			my_printf("I would've moved %s (%.2f > %.2f)\n", tt_move.to_str().c_str(), eval_me / 100., eval_opp / 100.);
	}
	else {
		(*expected_move_count)++;
	}
}

void show_header(const terminal_t t)
{
	if (t != T_VT100 && t != T_ANSI)
		return;

	my_printf("\x1b[m\x1b[2J\x1b[1;7m\x1b[1;1H");
	for(int i=0; i<8; i++)
		my_printf("          ");
	my_printf("\x1b[1;1HHELLO, THIS IS DOG");
	my_printf("\x1b[m\x1b[2;1H");
}

terminal_t t              = T_ASCII;
bool       default_trace  = false;
int32_t    total_dog_time = 1000;  // milliseconds
bool       do_ponder      = false;

std::optional<std::string> get_cfg_dir()
{
	const char *home = std::getenv("HOME");
	if (!home) {
		fprintf(stderr, "$HOME not found\n");
		return { };
	}

	return std::string(home) + "/.config/Dog";
}

void write_settings()
{
#if defined(ESP32)
	FILE *fh = fopen("/spiffs/settings.dat", "w");
#else
	auto home = get_cfg_dir();
	if (home.has_value() == false)
		return;
#if defined(_WIN32)
	mkdir(home.value().c_str());
#else
	mkdir(home.value().c_str(), 0700);
#endif

	FILE *fh = fopen((home.value() + "/settings.dat").c_str(), "w");
#endif
	if (!fh) {
		fprintf(stderr, "Cannot write settings: %s\n", strerror(errno));
		return;
	}

	fprintf(fh, "%d\n", t);
	fprintf(fh, "%d\n", default_trace);
	fprintf(fh, "%lu\n", total_dog_time);
	fprintf(fh, "%d\n", do_ponder);
	fprintf(fh, "%d\n", clock_type);
	fprintf(fh, "%d\n", do_ping);
	fprintf(fh, "%s\n", wifi_ssid.c_str());
	fprintf(fh, "%s\n", wifi_psk .c_str());

	fclose(fh);
}

void load_settings()
{
#if defined(ESP32)
	FILE *fh = fopen("/spiffs/settings.dat", "r");
#else
	auto home = get_cfg_dir();
	if (home.has_value() == false)
		return;
	FILE *fh = fopen((home.value() + "/settings.dat").c_str(), "r");
#endif
	if (!fh)
		return;

	char buffer[16] { };
	fgets(buffer, sizeof buffer, fh);
	t              = terminal_t(atoi(buffer));
	fgets(buffer, sizeof buffer, fh);
	default_trace  = atoi(buffer);
	fgets(buffer, sizeof buffer, fh);
	total_dog_time = atoi(buffer);
	fgets(buffer, sizeof buffer, fh);
	do_ponder      = atoi(buffer);
	fgets(buffer, sizeof buffer, fh);
	clock_type     = dog_clock_t(atoi(buffer));
	fgets(buffer, sizeof buffer, fh);
	do_ping        = atoi(buffer);
	fgets(buffer, sizeof buffer, fh);
	char *lf = strchr(buffer, '\n');
	if (lf)
		*lf = 0x00;
	wifi_ssid      = buffer;
	fgets(buffer, sizeof buffer, fh);
	lf = strchr(buffer, '\n');
	if (lf)
		*lf = 0x00;
	wifi_psk       = buffer;

	fclose(fh);
}

static void help()
{
	my_printf("quit     stop the tui\n");
	my_printf("new      restart game\n");
	my_printf("player   select player (\"white\" or \"black\")\n");
	my_printf("time     set think time, in seconds\n");
	my_printf("clock    set clock type: \"incremental\" or \"total\"\n");
	my_printf("eval     show current evaluation score\n");
	my_printf("moves    show valid moves\n");
#if defined(ESP32)
	my_printf("cfgwifi  ssid|password\n");
	my_printf("submit   send current PGN to server, result is shown\n");
#else
	my_printf("syzygy   probe the syzygy ETB\n");
#endif
	my_printf("book     check for a move in the book\n");
	my_printf("hint     show a hint\n");
	my_printf("undo     take back last move\n");
	my_printf("auto     auto play until the end\n");
	my_printf("ponder   on/off\n");
	my_printf("trace    on/off\n");
	my_printf("terminal \"ansi\", \"vt100\" or \"text\"\n");
	my_printf("ping     beep on/off\n");
	my_printf("redraw   redraw screen\n");
	my_printf("stats    show statistics\n");
	my_printf("cstats   reset statistics\n");
	my_printf("fen      show a fen for the current position\n");
	my_printf("setfen   set the current position\n");
	my_printf("bench    run a benchmark: \"short\" or \"long\"\n");
	my_printf("perft x  run perft for depth x starting at current position\n");
	my_printf("recall   go to the latest position recorded\n");
	my_printf("...or enter a move (SAN/LAN)\n");
	my_printf("The score behind a move in the move-list is the absolute score.\n");
}

std::string my_getline(std::istream & is)
{
	my_printf("> ");

	std::string out;

	for(;;) {
		char c = 0;
		if (!is.get(c))
			break;

		if (c == 13 || c == 10) {
			if (out.empty() == false) {
				my_printf("\n\r> ");
				break;
			}
		}
		else if (c == 8 || c == 127) {
			if (out.empty() == false) {
				my_printf("\x08 \x08");
				out = out.substr(0, out.size() - 1);
			}
		}
		else if (c >= 32 && c < 127) {
			my_printf("%c", c);
			out += c;
		}
	}

	return out;
}

void tui()
{
	set_thread_name("TUI");

	load_settings();

	stop_ponder();

	trace_enabled = default_trace;
	
	std::optional<libchess::Color> player = sp.at(0)->pos.side_to_move();
	std::vector<int16_t>           scores;
	std::vector<libchess::Move>    moves_played;

#if defined(ESP32)
	auto *pb = new polyglot_book("/spiffs/dog-book.bin");
#else
	auto *pb = new polyglot_book("dog-book.bin");
#endif

	bool show_board = true;
	bool p_a_k      = false;
	bool first      = true;

	uint64_t human_think_start   = 0;
	uint64_t total_human_think   = 0;
	int      n_human_think       = 0;
	int32_t  initial_think_time  = 0;
	int32_t  human_score_sum     = 0;
	int      human_score_n       = 0;
	int32_t  dog_score_sum       = 0;
	int      dog_score_n         = 0;
	int      expected_move_count = 0;

	auto reset_state = [&]()
	{
		memset(sp.at(0)->history, 0x00, history_malloc_size);
		tti.reset();
		moves_played.clear();
		scores.clear();
		sp.at(0)->pos       = libchess::Position(libchess::constants::STARTPOS_FEN);
		total_dog_time      = initial_think_time;
		human_think_start   = 0;
		total_human_think   = 0;
		n_human_think       = 0;
		human_score_sum     = 0;
		human_score_n       = 0;
		dog_score_sum       = 0;
		dog_score_n         = 0;
		expected_move_count = 0;
		player              = libchess::constants::WHITE;

		for(auto & e: sp)
			e->nnue_eval->reset();
	};

	for(;;) {
		uint64_t start_position_count = sp.at(0)->cs.data.nodes + sp.at(0)->cs.data.qnodes;

		if (p_a_k && player.has_value()) {
			p_a_k = false;
			if (do_ponder) {
				std::string fen = sp.at(0)->pos.fen();
				start_ponder();
				press_any_key();
				stop_ponder();
				sp.at(0)->pos = libchess::Position(fen);
			}
			else {
				press_any_key();
			}
		}

		if (show_board) {
			if (player.has_value()) {
				show_header(t);

				if (t == T_ASCII) {
					my_printf("Human think time used: %.3f seconds\n", total_human_think / 1000000.);
					my_printf("Dog think time left: %.3f seconds\n", total_dog_time / 1000.);
					if (human_score_n)
						my_printf("Average score gain human: %.2f\n", human_score_sum * 100. / human_score_n);
					if (dog_score_n)
						my_printf("Average score gain dog: %.2f\n", dog_score_sum * 100. / dog_score_n);
				}
				else {
					my_printf("\x1b[2;63HHuman think time:");
					constexpr const uint32_t ms = 1000;
					constexpr const uint32_t us = ms * ms;
					my_printf("\x1b[3;65H%02d:%02d.%03d",
							total_human_think / (60 * us),
							(total_human_think / us) % 60,
							(total_human_think / ms) % ms);
					my_printf("\x1b[4;63HDog time left:");
					my_printf("\x1b[5;65H%02d:%02d.%03d",
							total_dog_time / (60 * ms),
							(total_dog_time / ms) % 60,
							total_dog_time % 1000);
					if (human_score_n || dog_score_n) {
						my_printf("\x1b[7;63HAvg.score gain:");
						if (human_score_n)
							my_printf("\x1b[8;65Hhuman: %.2f", human_score_sum / 100. / human_score_n);
						if (dog_score_n)
							my_printf("\x1b[9;65Hdog  : %.2f", dog_score_sum / 100. / dog_score_n);
					}
					my_printf("\x1b[2;1H");
				}
			}

			show_board = false;
			display(sp.at(0)->pos, t, moves_played, scores);

			if (sp.at(0)->pos.fullmoves() > 1)
				my_printf("%d of the move(s) you played were expected.\n", expected_move_count);
			if (sp.at(0)->pos.in_check()) {
				std::string result = "CHECK";
				switch(sp.at(0)->pos.game_state()) {
					case libchess::Position::GameState::CHECKMATE:
						result = "CHECKMATE";
						break;
					case libchess::Position::GameState::STALEMATE:
						result = "STALEMATE";
						break;
					case libchess::Position::GameState::THREEFOLD_REPETITION:
						result = "THREEFOLD REPETITION";
						break;
					case libchess::Position::GameState::FIFTY_MOVES:
						result = "FIFTY MOVES";
						break;
					case libchess::Position::GameState::IN_PROGRESS:
						break;
					default:
						result = "(unknown game state)";
						break;
				}
				if (t != T_ASCII)
					my_printf("\x1b[4m%s\x1b[m!\n", result.c_str());
				else
					my_printf("%s!\n", result.c_str());
			}

			if (t != T_ASCII) {
				store_cursor_position();
				my_printf("\x1b[15;69H / \\__");
				my_printf("\x1b[16;69H(    @\\____");
				my_printf("\x1b[17;69H /         O");
				my_printf("\x1b[18;69H/   (_____/");
				my_printf("\x1b[19;69H/_____/   U");
				restore_cursor_position();
			}
		}

		if (first) {
			first = false;
			my_printf("ponder: %s, bell: %s, wifi ssid: %s\n", do_ponder?"on":"off", do_ping?"on":"off", wifi_ssid.c_str());
		}

		if (peek_for_ctrl_c())
			player = sp.at(0)->pos.side_to_move();

		bool finished = sp.at(0)->pos.game_state() != libchess::Position::GameState::IN_PROGRESS;
		if ((player.has_value() && player.value() == sp.at(0)->pos.side_to_move()) || finished) {
			std::string fen;
			if (do_ponder) {
				fen = sp.at(0)->pos.fen();
				start_ponder();
			}

			human_think_start = esp_timer_get_time();

			std::string line = my_getline(is);

			if (fen.empty() == false) {
				stop_ponder();
				// because pondering does not reset the libchess::Position-object:
				sp.at(0)->pos = libchess::Position(fen);
			}

			if (do_ponder) {
				uint64_t end_position_count = sp.at(0)->cs.data.nodes + sp.at(0)->cs.data.qnodes;
				my_printf("While you were thinking, Dog considered %" PRIu64 " positions.\n", end_position_count - start_position_count);
			}

			auto parts = split(line, " ");
			if (parts[0] == "help")
				help();
			else if (parts[0] == "quit")
				break;
			else if (parts[0] == "version")
				hello();
			else if (parts[0] == "auto")
				player.reset();
#if defined(ESP32)
			else if (parts[0] == "cfgwifi") {
				if (parts.size() != 2)
					my_printf("Usage: cfgwifi ssid|password\n");
				else {
					auto parts_wifi = split(parts[1], "|");
					wifi_ssid = parts_wifi[0];
					wifi_psk  = parts_wifi[1];
					write_settings();
				}
			}
			else if (parts[0] == "submit" || parts[0] == "publish") {
				if (moves_played.empty())
					my_printf("No moves played yet\n");
				else if (wifi_ssid.empty())
					my_printf("WiFi is not configured yet (see cfgwifi)\n");
				else {
					std::string pgn = "[Event \"Computer chess event\"]\n"
							  "[Site \"-\"]\n"
							  "[Date \"-\"]\n"
							  "[Round \"-\"]\n";

					if (player.has_value()) {
						if (player.value() == libchess::constants::WHITE) {
							pgn += "[White \"Dog v" DOG_VERSION "\"]\n";
							pgn += "[Black \"?\"]\n";
						}
						else {
							pgn += "[White \"?\"]\n";
							pgn += "[Black \"Dog v" DOG_VERSION "\"]\n";
						}
					}
					else {
						pgn += "[White \"?\"]\n";
						pgn += "[Black \"?\"]\n";
					}
					std::string result = "?";
					auto game_state = sp.at(0)->pos.game_state();
					if (game_state != libchess::Position::GameState::IN_PROGRESS) {
						if (game_state != libchess::Position::GameState::CHECKMATE)
							result = "1/2-1/2";
						else if ((moves_played.size() & 1) == 0)  // black played last
							result = "0-1";
						else
							result = "1-0";
					}
					pgn += "[Result \"" + result + "\"]\n";
					pgn += "\n";

					auto current_color = libchess::constants::WHITE;
					int  move_nr       = 0;
					for(auto & move: moves_played) {
						if (current_color == libchess::constants::WHITE) {
							move_nr++;
							pgn += myformat("%d. %s ", move_nr, move.to_str().c_str());
							current_color = libchess::constants::BLACK;
						}
						else {
							pgn += move.to_str();
							if ((move_nr % 10) == 0)
								pgn += "\n";
							else
								pgn += " ";
							current_color = libchess::constants::WHITE;
						}
					}

					pgn += result + "\n\n";

					my_printf("\n");
					my_printf("\n");
					const std::string result_url = push_pgn(pgn);
					if (result_url.empty())
						my_printf(" > Submit failed!\n");
					else
						my_printf("PGN can be found at: %s\n", result_url.c_str());
					my_printf("\n");
				}
			}
#endif
			else if (parts[0] == "bench") {
				run_bench(parts.size() == 2 && parts[1] == "long");
			}
			else if (parts[0] == "perft") {
				perft(sp.at(0)->pos, parts.size() == 2 ? std::stoi(parts[1]) : 3);
			}
			else if (parts[0] == "new") {
				reset_state();
				show_board = true;
			}
			else if (parts[0] == "redraw")
				show_board = true;
			else if (parts[0] == "player" && parts.size() == 2) {
				if (parts[1] == "white" || parts[1] == "w")
					player = libchess::constants::WHITE;
				else
					player = libchess::constants::BLACK;
			}
			else if (parts[0] == "time" && parts.size() == 2) {
				try {
					total_dog_time = 0;

					auto time_parts = split(parts[1], ":");
					for(auto & time_part: time_parts) {
						total_dog_time *= 60;
						total_dog_time += std::stod(time_part);
					}
					total_dog_time *= 1000;

					initial_think_time = total_dog_time;
					write_settings();
				}
				catch(std::invalid_argument & ia) {
					my_printf("Please enter a value for time, not something else.\n");
					my_printf("Did you mean to enter \"clock %s\"?\n", parts[1].c_str());
				}
			}
			else if (parts[0] == "clock" && parts.size() == 2) {
				if (parts[1] == "incremental" || parts[1] == "total") {
					clock_type = parts[1] == "incremental" ? C_INCREMENTAL : C_TOTAL;
					my_printf("Clock type: %s\n", clock_type == C_INCREMENTAL ? "incremental" : "total");
				}
				else {
					my_printf("Parameter for \"clock\" must be \"incremental\" or \"total\".\n");
				}
			}
			else if (parts[0] == "moves")
				show_movelist(sp.at(0)->pos);
			else if (parts[0] == "board")
				show_board = true;
			else if (parts[0] == "stats")
				show_stats(sp.at(0)->pos, sp.at(0)->cs, parts.size() == 2 && parts[1] == "-v");
			else if (parts[0] == "cstats")
				sp.at(0)->cs.reset();
			else if (parts[0] == "fen")
				my_printf("Fen: %s\n", sp.at(0)->pos.fen().c_str());
			else if (parts[0] == "setfen" && parts.size() >= 2) {
				reset_state();
				std::string fen;
				for(size_t i=1; i<parts.size(); i++) {
					if (fen.empty() == false)
						fen += " ";
					fen += parts.at(i);
				}
				sp.at(0)->pos = libchess::Position(fen);
				my_printf("FEN set, hash: %" PRIx64 "\n", sp.at(0)->pos.hash());
				p_a_k      = true;
				show_board = true;
			}
			else if (parts[0] == "cls") {
				if (t != T_ASCII)
					my_printf("\x1b[2J");
			}
#if !defined(ESP32)
			else if (parts[0] == "syzygy")
				do_syzygy(sp.at(0)->pos);
#endif
			else if (parts[0] == "trace") {
				if (parts.size() == 2)
					trace_enabled = parts[1] == "on";
				else
					trace_enabled = !trace_enabled;
				default_trace = trace_enabled;
				write_settings();
				my_printf("Tracing is now %senabled\n", trace_enabled ? "":"not ");
			}
			else if (parts[0] == "ping") {
				if (parts.size() == 2)
					do_ping = parts[1] == "on";
				else
					do_ping = !do_ping;
				write_settings();
				my_printf("Beeping is now %senabled\n", do_ping ? "":"not ");
			}
			else if (parts[0] == "terminal" && parts.size() == 2) {
				if (parts[1] == "ansi")
					t = T_ANSI;
				else if (parts[1] == "vt100")
					t = T_VT100;
				else {
					t = T_ASCII;
					parts[1] = "text";
				}
				write_settings();
				my_printf("Terminal type is now: %s\n", parts[1].c_str());
			}
			else if (parts[0] == "ponder") {
				if (parts.size() == 2)
					do_ponder = parts[1] == "on";
				else
					do_ponder = !do_ponder;
				write_settings();
				my_printf("Pondering is now %senabled\n", do_ponder ? "":"not ");
				p_a_k      = true;
				show_board = true;
			}
			else if (parts[0] == "undo") {
				sp.at(0)->pos.unmake_move();  /// TODO
				player = sp.at(0)->pos.side_to_move();
				moves_played.pop_back();
				scores.pop_back();
			}
			else if (parts[0] == "eval") {
				int nnue_score = nnue_evaluate(sp.at(0)->nnue_eval, sp.at(0)->pos);
				my_printf("evaluation score: %.2f\n", nnue_score / 100.);
			}
			else if (parts[0] == "recall") {
				FILE *fh = fopen(RECALL_FILE, "r");
				if (fh) {
					char buffer[128] { };
					fgets(buffer, sizeof(buffer) - 1, fh);
					char *lf = strchr(buffer, '\n');
					if (lf)
						*lf = 0x00;
					my_printf("Use \"%s\"?\n", buffer);

					if (is.get() == 'y') {
						reset_state();

						sp.at(0)->pos = libchess::Position(buffer);
						fgets(buffer, sizeof(buffer) - 1, fh);
						total_dog_time = atoi(buffer);

						p_a_k      = true;
						show_board = true;
					}

					fclose(fh);
				}
				else {
					my_printf("No fen remembered\n");
					p_a_k = true;
				}
			}
			else if (parts[0] == "hint")
				tt_lookup();
			else if (parts[0] == "book") {
				auto move = pb->query(sp.at(0)->pos);
				if (move.has_value())
					my_printf("Book suggestion: %s\n", move.value().to_str().c_str());
				else
					my_printf("Book has no entry/entries for this position\n");
			}
			else if (parts[0] == "dog")
				print_max_ascii();
			else if (parts[0].size() >= 2) {
				auto move = libchess::Move::from(parts[0]);
				if (move.has_value())
					move = validate_move(move.value(), sp.at(0)->pos);
				if (move.has_value() == false)
					move = SAN_to_move(parts[0], sp.at(0)->pos);
				if (move.has_value() == true) {
					compare_moves(sp.at(0)->pos, move.value(), &expected_move_count);

					auto    now_playing  = sp.at(0)->pos.side_to_move();
					int16_t score_before = get_score(sp.at(0)->pos, now_playing);

					auto undo_actions = make_move(sp.at(0)->nnue_eval, sp.at(0)->pos, move.value());
					moves_played.push_back(move.value());
					scores.push_back(nnue_evaluate(sp.at(0)->nnue_eval, sp.at(0)->pos));

					int16_t score_after = get_score(sp.at(0)->pos, now_playing);
					human_score_sum += score_after - score_before;
					human_score_n++;

					uint64_t human_think_end = esp_timer_get_time();
					total_human_think += human_think_end - human_think_start;
					n_human_think++;

					store_position(sp.at(0)->pos.fen(), total_dog_time);

					show_board = true;
					p_a_k      = true;
				}
				else {
					my_printf("Not a valid move (or ambiguous) nor command.\n");
					my_printf("Enter \"help\" for the list of commands.\n");
				}
			}
		}
		else {
#if !defined(linux) && !defined(_WIN32) && !defined(__ANDROID__) && !defined(__APPLE__)
			stop_blink(led_red_timer, &led_red);
			start_blink(led_green_timer);
#endif
			auto    now_playing  = sp.at(0)->pos.side_to_move();
			int16_t score_before = get_score(sp.at(0)->pos, now_playing);

			auto move = pb->query(sp.at(0)->pos);
			assert(move.has_value() == false || sp.at(0)->pos.is_legal_move(move.value()));
			if (move.has_value() && sp.at(0)->pos.is_legal_move(move.value())) {
				my_printf("Book move: %s\n", move.value().to_str().c_str());

				auto undo_actions = make_move(sp.at(0)->nnue_eval, sp.at(0)->pos, move.value());
				moves_played.push_back(move.value());
				scores.push_back(nnue_evaluate(sp.at(0)->nnue_eval, sp.at(0)->pos));

				if (clock_type == C_TOTAL)
					total_dog_time -= 1;  // 1 millisecond
			}
			else {
				if (total_dog_time <= 0)
					my_printf("The flag fell for Dog, you won!\n");

				int32_t cur_think_time = 0;
				if (clock_type == C_INCREMENTAL)
					cur_think_time = total_dog_time;
				else {
					const int moves_to_go = 40 - sp.at(0)->pos.fullmoves();
					const int cur_n_moves = moves_to_go <= 0 ? 40 : moves_to_go;

	                                cur_think_time  = total_dog_time / (cur_n_moves + 7);

					const int limit_duration_min = total_dog_time / 15;
					if (cur_think_time > limit_duration_min)
						cur_think_time = limit_duration_min;

					total_dog_time -= cur_think_time;
				}

				my_printf("Thinking... (%.3f seconds)\n", cur_think_time / 1000.);
				libchess::Move best_move;
				int            best_score { 0 };
				clear_flag(sp.at(0)->stop);
				std::tie(best_move, best_score) = search_it(cur_think_time, true, sp.at(0), -1, { }, true);
				my_printf("Selected move: %s (score: %.2f)\n", best_move.to_str().c_str(), best_score / 100.);
				emit_pv(sp.at(0)->nnue_eval, sp.at(0)->pos, best_move, t);

				auto undo_actions = make_move(sp.at(0)->nnue_eval, sp.at(0)->pos, best_move);
				moves_played.push_back(best_move);
				scores.push_back(best_score);
			}

			store_position(sp.at(0)->pos.fen(), total_dog_time);

			int16_t score_after = get_score(sp.at(0)->pos, now_playing);
			dog_score_sum += score_after - score_before;
			dog_score_n++;

			my_printf("\n");

#if !defined(linux) && !defined(_WIN32) && !defined(__APPLE__)
			stop_blink(led_green_timer, &led_green);
#endif

			if (do_ping)
				my_printf("\07");  // bell

			p_a_k      = true;
			show_board = true;
		}
	}

	delete pb;

	trace_enabled = true;
}

void run_tui()
{
	// because of ESP32 stack
	auto th = new std::thread{tui};
	th->join();
	delete th;
}
