#include <atomic>
#include <mutex>
#include <thread>

#include "main.h"
#include "stats.h"


class state_exporter
{
private:
	std::atomic_bool stop { false   };
	std::thread     *th   { nullptr };
	std::mutex       lock;
	search_pars_t   *sp   { nullptr };
	const int        hz   { 25      };

public:
	struct _export_structure_ {
		std::mutex           lock;
		volatile int         revision;
		chess_stats::_data_  counters;
		uint32_t             cur_move;
	} *pdata { nullptr };

public:
	state_exporter(const int hz);
	virtual ~state_exporter();

	void set(search_pars_t *const sp);
	void clear();

	void handler();
};
