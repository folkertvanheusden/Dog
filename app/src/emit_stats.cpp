#include <pthread.h>
#include <string>
#include <unistd.h>
#include <sys/shm.h>

#include "state_exporter.h"


void emit_statistics(state_exporter::_export_structure_ *const counts, const std::string & header)
{
	printf(" * %s *\n", header.c_str());

	for(;;) {
		pthread_mutex_lock(&counts->mutex);
		if (counts->revision)
			break;
		pthread_mutex_unlock(&counts->mutex);
		usleep(101000);
	}

	printf("Search nodes: %u, qs nodes: %u, ratio: %.3f\n", counts->counters.nodes, counts->counters.qnodes, double(counts->counters.qnodes)/counts->counters.nodes);
	printf("draws: %.2f%% (%u), standing pat: %.2f%% (%u)\n", counts->counters.n_draws * 100. / counts->counters.nodes, counts->counters.n_draws, counts->counters.n_standing_pat * 100. / counts->counters.qnodes, counts->counters.n_standing_pat);
	printf("%u tt query, %u ttstore, %.2f%% hit, query/store factor: %.2f\n", counts->counters.tt_query, counts->counters.tt_store, counts->counters.tt_hit * 100. / counts->counters.tt_query, counts->counters.tt_query / double(counts->counters.tt_store));
	printf("Syzygy queries: %u, hits: %.2f%%\n", counts->counters.syzygy_queries, counts->counters.syzygy_query_hits * 100. / counts->counters.syzygy_queries);
	printf("Average beta-cutoff index: %.2f, QS beta-cutoff index: %.2f\n", counts->counters.n_moves_cutoff / double(counts->counters.nmc_nodes), counts->counters.n_qmoves_cutoff / double(counts->counters.nmc_qnodes));
	printf("QS early stop: %.2f%% (%u)\n", counts->counters.n_qs_early_stop * 100. / counts->counters.qnodes, counts->counters.n_qs_early_stop);
	printf("Null move cutoff: %.2f%% (%u out of %u)\n", counts->counters.n_null_move_hit * 100. / counts->counters.n_null_move, counts->counters.n_null_move_hit, counts->counters.n_null_move);
	printf("late-move-reduction cutoff: %.2f%% (%u out of %u)\n", counts->counters.n_lmr_hit * 100.0 / counts->counters.n_lmr, counts->counters.n_lmr_hit, counts->counters.n_lmr);
	printf("static evaluation cutoff: %.2f%% (%u out of %u)\n", counts->counters.n_static_eval_hit * 100. / counts->counters.n_static_eval, counts->counters.n_static_eval_hit, counts->counters.n_static_eval);
	printf("average alpha/beta aspiration window distance: %.2f/%.2f\n", counts->counters.alpha_distance / double(counts->counters.n_alpha_distances), counts->counters.beta_distance / double(counts->counters.n_beta_distances));

	pthread_mutex_unlock(&counts->mutex);
}

state_exporter::_export_structure_ *open_shm()
{
	unsigned sid = shmget('Dog0', sizeof(state_exporter::_export_structure_), 0);
	return reinterpret_cast<state_exporter::_export_structure_ *>(shmat(sid, nullptr, 0));
}

void close_shm(state_exporter::_export_structure_ *p)
{
	shmdt(p);
}

int main(int argc, char *argv[])
{
	auto *p = open_shm();
	if (p == reinterpret_cast<void *>(-1)) {
		fprintf(stderr, "Failed to open Dog shared memory segment\n");
		return 1;
	}

	emit_statistics(p, "Statistics");

	close_shm(p);

	return 0;
}
