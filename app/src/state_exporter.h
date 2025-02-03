#include <atomic>
#include <mutex>
#include <pthread.h>
#include <thread>

#include "main.h"
#include "stats.h"


class state_exporter
{
private:
	std::atomic_bool stop { false   };
	std::thread     *th   { nullptr };
	std::mutex       lock;  // protecting 'sp'
	search_pars_t   *sp   { nullptr };
	const int        hz   { 25      };
	int              fd   { -1      };

public:
	struct _export_structure_ {
		pthread_mutex_t      mutex;
		int                  revision;
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
