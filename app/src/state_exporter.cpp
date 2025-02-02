#include <unistd.h>
#include <sys/shm.h>

#include "state_exporter.h"


state_exporter::state_exporter(const int hz):
	hz(hz)
{
	unsigned sid = shmget('Dog0', sizeof(_export_structure_), IPC_CREAT | 0600);
	pdata        = reinterpret_cast<_export_structure_ *>(shmat(sid, nullptr, 0));
	pthread_mutexattr_t mutex_attributes;
	pthread_mutexattr_init(&mutex_attributes);
	pthread_mutexattr_setpshared(&mutex_attributes, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&pdata->mutex, &mutex_attributes);

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
	pdata->revision = 0;
}

void state_exporter::clear()
{
	std::unique_lock<std::mutex> lck(lock);
	sp = nullptr;
	pdata->revision = 0;
}

void state_exporter::handler()
{
	while(!stop) {
		usleep(1000000 / hz);

		std::unique_lock<std::mutex> lck(lock);
		if (!sp)
			continue;

		if (pthread_mutex_trylock(&pdata->mutex) == 0) {
			pdata->counters = sp->cs.data;
			pdata->cur_move = sp->cur_move;
			pdata->revision++;
			pthread_mutex_unlock(&pdata->mutex);
		}
	}
}
