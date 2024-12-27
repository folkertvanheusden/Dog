#include <string>

#include <libchess/Position.h>

#include "eval.h"
#include "main.h"
#include "my_tuner.h"


#if defined(linux) || defined(_WIN32)
void tune(std::string file)
{
	auto normalized_results = libchess::NormalizedResult<libchess::Position>::parse_epd(file, [](const std::string& fen) { return *libchess::Position::from_fen(fen); });
	printf("%zu EPDs loaded\n", normalized_results.size());

	std::vector<libchess::TunableParameter> tunable_parameters = default_parameters.get_tunable_parameters();
	printf("%zu parameters\n", tunable_parameters.size());

	int16_t history[history_size] { 0 };
	libchess::Tuner<libchess::Position> tuner{normalized_results, tunable_parameters,
		[&history](std::vector<libchess::NormalizedResult<libchess::Position> > & positions, const std::vector<libchess::TunableParameter> & params) {
			eval_par cur(params);

			search_pars_t sp { &cur, false, history };
			sp.stop       = new end_t();
			sp.stop->flag = false;
			sp.cs         = new chess_stats_t();

#pragma omp parallel for
			for(auto &p: positions) {
				auto & pos = p.position();
				int score = qs(pos, -32767, 32767, 0, &sp, 0);
				if (pos.side_to_move() != libchess::constants::WHITE)
					score = -score;
				p.set_result(score);
			}

			delete sp.cs;
			delete sp.stop;
		}};


	uint64_t start_ts = esp_timer_get_time() / 1000;
	double start_error = tuner.error();
	tuner.tune();
	double end_error = tuner.error();
	uint64_t end_ts = esp_timer_get_time() / 1000;

	time_t start = start_ts / 1000;
	char *str = ctime(&start), *lf = strchr(str, '\n');

	if (lf)
		*lf = 0x00;

	printf("# error: %.18f (delta: %f) (%f%%), took: %fs, %s\n", end_error, end_error - start_error, sqrt(end_error) * 100.0, (end_ts - start_ts) / 1000.0, str);

	auto parameters = tuner.tunable_parameters();
	for(auto parameter : parameters)
		printf("%s=%d\n", parameter.name().c_str(), parameter.value());
	printf("#---\n");

	FILE *fh = fopen("tune.dat", "w");
	if (!fh)
		fprintf(stderr, "Failed to create tune.dat!\n");
	else {
		for(auto parameter : parameters)
			fprintf(fh, "%s=%d\n", parameter.name().c_str(), parameter.value());
		fclose(fh);
	}
}
#endif
