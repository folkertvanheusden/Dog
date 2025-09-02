#if defined(linux) || defined(__APPLE__)
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#if !defined(NDEBUG)
#if !defined(_WIN32) && !defined(ESP32) && !defined(__ANDROID__) && !defined(__APPLE__)
#include <valgrind/drd.h>
#include <valgrind/helgrind.h>
#endif
#endif

#include "state_exporter.h"


state_exporter::state_exporter(const int hz):
	hz(hz)
{
	bool new_segment = false;

	// try to open
	fd = shm_open("/Dog", O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
	if (fd == -1) {
		// did not exist
		if (errno != ENOENT) {
			fprintf(stderr, "Failed to access shared memory: %s\n", strerror(errno));
			return;
		}

		// create
		fd = shm_open("/Dog", O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
		if (fd == -1) {
			fprintf(stderr, "Failed to create shared memory: %s\n", strerror(errno));
			return;
		}

		ftruncate(fd, sizeof(_export_structure_));

		new_segment = true;
	}

	pdata = reinterpret_cast<_export_structure_ *>(mmap(nullptr, sizeof(_export_structure_), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0));
	if (pdata == reinterpret_cast<void *>(-1))
		pdata = nullptr;
	else if (new_segment && sysconf(_SC_THREAD_PROCESS_SHARED) >= 0) {
		pthread_mutexattr_t mutex_attributes { };
		pthread_mutexattr_init(&mutex_attributes);
		pthread_mutexattr_setpshared(&mutex_attributes, PTHREAD_PROCESS_SHARED);
		pthread_mutex_init(&pdata->mutex, &mutex_attributes);
	}

	if (pdata)
		th = new std::thread(&state_exporter::handler, this);
	else
		fprintf(stderr, "SHM unavailable: %s\n", strerror(errno));
}

state_exporter::~state_exporter()
{
	stop = true;

	if (th) {
		th->join();
		delete th;
	}

	if (pdata)
		munmap(pdata, 0);
	if (fd != -1)
		close(fd);
}

void state_exporter::set(search_pars_t *const sp)
{
	std::unique_lock<std::mutex> lck(lock);
	this->sp = sp;
	if (pdata)
		pdata->revision = 0;
}

void state_exporter::clear()
{
	std::unique_lock<std::mutex> lck(lock);
	sp = nullptr;
	if (pdata)
		pdata->revision = 0;
}

void state_exporter::handler()
{
#if !defined(NDEBUG)
#if !defined(_WIN32) && !defined(ESP32) && !defined(__ANDROID__) && !defined(__APPLE__)
	VALGRIND_HG_DISABLE_CHECKING(&sp->cs.data,  sizeof(sp->cs.data));
	VALGRIND_HG_DISABLE_CHECKING(&sp->cur_move, sizeof(sp->cur_move));
	DRD_IGNORE_VAR(sp->cs.data );
	DRD_IGNORE_VAR(sp->cur_move);
#endif
#endif

	while(!stop) {
		usleep(1000000 / hz);

		std::unique_lock<std::mutex> lck(lock);
		if (!sp)
			continue;

		int rc = pthread_mutex_trylock(&pdata->mutex);
		if (rc == 0) {
			static int cnt = 0;
			pdata->counters = sp->cs.data;
			pdata->cur_move = sp->cur_move;
			pdata->revision++;
			pthread_mutex_unlock(&pdata->mutex);
		}
		else if (rc == EOWNERDEAD) {
			pthread_mutex_consistent(&pdata->mutex);
		}
	}
}
#endif
