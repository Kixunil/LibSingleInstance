#include "singleinstance.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <poll.h>
#include <sys/select.h>
#include <semaphore.h>

#include <string>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <map>
#include <list>

#include <cstdio>

#define PIPENAME "singleinstance_pipe"
#define AUTO_THROW(STR) throw runtime_error(string((STR)) + ": " + strerror(errno))

using namespace std;

typedef map<uint32_t, pair<uint32_t, vector<char> > > pidbufmap;

struct SingleInstanceHandle {
	int lfd, pfd;
	string pipePath;
	pidbufmap buffers;
	list<vector<char *> > pending;
	int ctrlFifo[2];
};

struct SingleInstanceAutoHandle {
	SingleInstanceHandle *handle;
	pthread_t thread;
	SingleInstanceCallback callback;
	void *userData;
};

static void sendBuf(int fd, const char *buf, uint32_t len) {
	static pid_t pid = getpid();
	
	char sndbuf[512];
	memset(sndbuf, 0, 512);
	*(uint32_t *)sndbuf = pid;
	*((uint32_t *)(sndbuf + sizeof(uint32_t))) = len;

	uint32_t sndlen = len < 512 - 2*sizeof(uint32_t)?len:512 - 2*sizeof(uint32_t);
	memcpy(sndbuf + 2*sizeof(uint32_t), buf, sndlen);
	write(fd, sndbuf, 512);
	len -= sndlen;
	buf += sndlen;

	while(len) {
		sndlen = len < 512 - sizeof(uint32_t)?len:512 - sizeof(uint32_t);
		memcpy(sndbuf + sizeof(uint32_t), buf, sndlen);
		write(fd, sndbuf, 512);
		len -= sndlen;
		buf += sndlen;
	}
}

static void sendArgs(int fd, int argc, char **argv) {
	vector<char> buf;
	while(argc) {
		uint32_t arglen = strlen(*argv) + 1;
		buf.reserve(buf.size() + arglen + sizeof(uint32_t));
		for(size_t i = 0; i < sizeof(uint32_t); ++i) buf.push_back(((char *)&arglen)[i]);
		for(uint32_t i = 0; i < arglen; ++i) buf.push_back((*argv)[i]);
		--argc;
		++argv;
	}

	sendBuf(fd, &buf[0], buf.size());
}

vector<char *> parseArgs(const vector<char> &data) {
	vector<char *> args;
	size_t i = 0;
	while(i < data.size()) {
		uint32_t len = *(uint32_t *)&data[i];
		i += sizeof(uint32_t);
		char *buf = new char[len];
		memcpy(buf, &data[i], len);
		args.push_back(buf);
		i += len;
	}
	return args;
}

bool checkFull(SingleInstanceHandle *handle, pidbufmap::iterator &iter) {
	if(iter->second.first != iter->second.second.size())  return false;
	handle->pending.push_back(parseArgs(iter->second.second));
	handle->buffers.erase(iter);
	return true;
}

int recvIter(SingleInstanceHandle *handle, int wait) {
#ifdef LIBSINGLEINSTANCE_USE_POLL
	struct pollfd pfd[2];
	pfd[0].fd = handle->pfd;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;
	pfd[1].fd = handle->ctrlFifo[0];
	pfd[1].events = POLLIN;
	pfd[1].revents = 0;
	int pret = poll(&pfd, 2, wait?-1:0);
	if(pret && !pfd[0].revents && !pfd[1].revents) fprintf(stderr, "If you see this message, something is wrong with your system.\n");
	if(pfd[1].revents & POLLIN) {
		char c;
		read(handle->ctrlFifo[0], &c, 1);
		return -1;
	}
	if(pret <= 0 || !(pfd[0].revents & POLLIN)) return 0; // ignore errors
#else
	struct timeval tv = { 0, 0 };
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(handle->pfd, &fds);
	FD_SET(handle->ctrlFifo[0], &fds);
	int sret = select((handle->pfd > handle->ctrlFifo[0]?handle->pfd:handle->ctrlFifo[0]) + 1, &fds, NULL, NULL, wait?NULL:&tv);
	if(sret && !FD_ISSET(handle->pfd, &fds) && !FD_ISSET(handle->ctrlFifo[0], &fds)) fprintf(stderr, "If you see this message, something is wrong with your system.\n");
	if(FD_ISSET(handle->ctrlFifo[0], &fds)) {
		char c;
		read(handle->ctrlFifo[0], &c, 1);
		return -1;
	}
	if(sret <= 0 || !FD_ISSET(handle->pfd, &fds)) return 0;
#endif

	uint32_t pid;
	ssize_t ppret = read(handle->pfd, &pid, sizeof(uint32_t));
	if(ppret != sizeof(uint32_t)) {
		close(handle->pfd);
		int newpipe = open(handle->pipePath.c_str(), O_RDWR); // Ingore improbable race condition (fifo removed)
		handle->pfd = newpipe;
		return 1;
	}

	pair<uint32_t, vector<char> > tmp;
	pair<pidbufmap::iterator, bool> ins(handle->buffers.insert(pidbufmap::value_type(pid, tmp)));

	uint32_t toRead = 512 - sizeof(uint32_t);

	if(ins.second) {
		uint32_t len;
		read(handle->pfd, &len, sizeof(uint32_t));
		ins.first->second.first = 0;
		ins.first->second.second.resize(len);
		toRead -= sizeof(uint32_t);
	}

	uint32_t remaining = ins.first->second.second.size() - ins.first->second.first;
	uint32_t useful = toRead, trash = 0;
	if(remaining < toRead) {
		trash = toRead - remaining;
		useful = remaining;
	}
	ssize_t ret = read(handle->pfd, &ins.first->second.second[ins.first->second.first], useful);
	if(ret != useful)
		return 0;

	char trashbuf[512];
	if(trash) read(handle->pfd, trashbuf, trash);

	ins.first->second.first += ret;

	return !checkFull(handle, ins.first);
}

typedef struct SingleInstanceHandle SingleInstanceHandle;

struct SingleInstanceHandle *singleInstanceCreate(const char *appName, int argc, char **argv, char const **error) {
	static string serror;
	if(error) *error = NULL;
	int fd = -1, pfd = -1;
	try {
		string baseDir;
		const char *tmp = getenv("HOME");
		if(!tmp) {
			tmp = getenv("USER");
			if(!tmp) return NULL;
			baseDir = "/home/";
			baseDir += tmp;
		} else {
			baseDir = tmp;
		}
		baseDir += "/.";
		baseDir += appName;

		// Try to create directory and ignore errors (if it already exists, it's OK, if not, error will show up later
		mkdir(baseDir.c_str(), 0755);

		// Ensure that fifo exists
		if(mkfifo((baseDir + "/" PIPENAME).c_str(), 0600) < 0) {
			if(errno != EEXIST)
				AUTO_THROW("mkfifo");

			errno = 0;
			struct stat st;
			if(stat((baseDir + "/" PIPENAME).c_str(), &st) < 0)
				AUTO_THROW("stat");

			if(!S_ISFIFO(st.st_mode)) {
				errno = EEXIST;
				AUTO_THROW("mkfifo");
			}
		}

		int fd = open((baseDir + "/lock").c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
		if(fd < 0) {
			AUTO_THROW("open");
		}

		if(lockf(fd, F_TLOCK, 0) < 0) {
			if(errno == EACCES || errno == EAGAIN) {
				// Another instance already running
				pfd = open((baseDir + "/" PIPENAME).c_str(), O_WRONLY); 
				sendArgs(pfd, argc, argv);
				close(pfd);
				return NULL;
			} else {
				AUTO_THROW("open");
			}
		}

		pfd = open((baseDir + "/" PIPENAME).c_str(), O_RDWR); // Ingore improbable race condition (fifo removed)

		struct SingleInstanceHandle *handle = new SingleInstanceHandle(); // Todo exception handling

		handle->lfd = fd;
		handle->pfd = pfd;
		handle->pipePath = baseDir + "/" PIPENAME;
		pipe(handle->ctrlFifo);

		return handle;
	} catch(const exception &e) {
		if(fd > -1) close(fd);
		if(pfd > -1) close(pfd);
		serror = e.what();
		if(error) *error = serror.c_str();
		return NULL;
	}
}

void singleInstanceDestroy(struct SingleInstanceHandle *handle) {
	singleInstancePop(handle);

	close(handle->lfd);
	close(handle->pfd);
	close(handle->ctrlFifo[0]);
	close(handle->ctrlFifo[1]);

	delete handle;
}

int singleInstanceCheck(struct SingleInstanceHandle *handle, int *argc, char ***argv, int wait) {
	if(handle->pending.size()) {
		*argc = handle->pending.front().size();
		*argv = &handle->pending.front()[0];
		return 1;
	}
	int ret;
	do {
		ret = recvIter(handle, wait);
	} while(ret != -1 && (ret || (wait && !handle->pending.size())));
	if(handle->pending.size()) {
		*argc = handle->pending.front().size();
		*argv = &handle->pending.front()[0];
		return 1;
	}
	return 0;
}

void singleInstancePop(struct SingleInstanceHandle *handle) {
	if(!handle->pending.size()) return;
	
	for(size_t i = 0; i < handle->pending.front().size(); ++i) delete[] handle->pending.front()[i];
	handle->pending.pop_front();
}

void singleInstanceStopWait(struct SingleInstanceHandle *handle) {
	char c = 0;
	write(handle->ctrlFifo[1], &c, 1);
}

typedef struct SingleInstanceAutoHandle SingleInstanceAutoHandle;

static void *autoCheckRun(void *arg) {
	SingleInstanceAutoHandle *handle = (SingleInstanceAutoHandle *)arg;
	int argc;
	char **argv;
	while(singleInstanceCheck(handle->handle, &argc, &argv, 1)) {
		handle->callback(handle->userData, argc, argv);
		singleInstancePop(handle->handle);
	}
	return NULL;
}

struct SingleInstanceAutoHandle *singleInstanceAutoCreate(const char *appName, int argc, char **argv, char const **error, SingleInstanceCallback callback, void *userData) {
	SingleInstanceHandle *handle = singleInstanceCreate(appName, argc, argv, error);
	if(!handle) return NULL;
	struct SingleInstanceAutoHandle *ret = new SingleInstanceAutoHandle();
	ret->handle = handle;
	ret->callback = callback;
	ret->userData = userData;
	int err;
	if((err = pthread_create(&ret->thread, NULL, &autoCheckRun, ret))) {
		singleInstanceDestroy(handle);
		delete ret;
		if(error) *error = strerror(err);
		return NULL;
	}
	return ret;
}

void singleInstanceAutoDestroy(struct SingleInstanceAutoHandle *handle) {
	singleInstanceStopWait(handle->handle);
	pthread_join(handle->thread, NULL);
	singleInstanceDestroy(handle->handle);
	delete handle;
}
