#include <unistd.h>
#include <sys/shm.h>

#include "state_exporter.h"


state_exporter::state_exporter(const int hz):
	hz(hz)
{
	unsigned sid = shmget('Dog0', sizeof(_export_structure_), IPC_CREAT | 0600);
	pdata        = reinterpret_cast<_export_structure_ *>(shmat(sid, nullptr, 0));

	th = new std::thread(&state_exporter::handler, this);
}

state_exporter::~state_exporter()
{
	stop = true;
	th->join();
	delete th;

	shmdt(pdata);
}

void state_exporter::set(search_pars_t *const sp)
{
	std::unique_lock<std::mutex> lck(lock);
	this->sp = sp;
}

void state_exporter::clear()
{
	std::unique_lock<std::mutex> lck(lock);
	sp = nullptr;
}

void state_exporter::handler()
{
	while(!stop) {
		usleep(1000000 / hz);

		std::unique_lock<std::mutex> lck(lock);
		if (!sp)
			continue;

		std::unique_lock<std::mutex> lck_data(pdata->lock);
		pdata->counters = sp->cs.data;
		pdata->cur_move = sp->cur_move;
		pdata->cv.notify_all();
	}
}
