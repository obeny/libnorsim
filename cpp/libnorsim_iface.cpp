#include <cstdio>
#include <unistd.h>

#include <signal.h>

#include <sys/file.h>
#include <sys/ioctl.h>

#include <mtd/mtd-user.h>

#include "Libnorsim.h"
#include "Logger.h"

#define SYSCALL_PROLOGUE(syscall) \
	do { \
		instance.handleReportRequest(); \
		instance.getLogger().log(Loglevel::NOTE, "handling syscall: %s", false, syscall); \
	} while(0)

extern "C" {

volatile sig_atomic_t report_requested = 0;

static int internal_open(Libnorsim &libnorsim, const char *path, int oflag, va_list args);
static int internal_close(Libnorsim &libnorsim, int fd);
static int internal_ioctl(Libnorsim &libnorsim, int fd, unsigned long request, va_list args);

int open(const char *path, int oflag, ...) {
	Libnorsim &instance = Libnorsim::getInstance();
	std::lock_guard<std::mutex> lg(instance.getGlobalMutex());
	SYSCALL_PROLOGUE("open");
	int res;

	char *realpath_buf = realpath(path, NULL);
	va_list args;
	va_start(args, oflag);
	if (0 != strcmp(realpath_buf, instance.getCacheFile()))
		res = instance.getSyscallsCache().invokeOpen(path, oflag, args);
	else
		res = internal_open(instance, path, oflag, args);

	va_end(args);
	free(realpath_buf);
	realpath_buf = NULL;

	return (res);
}

int close(int fd) {
	Libnorsim &instance = Libnorsim::getInstance();
	std::lock_guard<std::mutex> lg(instance.getGlobalMutex());
	SYSCALL_PROLOGUE("close");
	int res;

	if (fd != instance.getCacheFileFd())
		res = instance.getSyscallsCache().invokeClose(fd);
	else
		res = internal_close(instance, fd);

	return (res);
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
	Libnorsim &instance = Libnorsim::getInstance();
	std::lock_guard<std::mutex> lg(instance.getGlobalMutex());
	SYSCALL_PROLOGUE("pread");
	return 0;
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
	Libnorsim &instance = Libnorsim::getInstance();
	std::lock_guard<std::mutex> lg(instance.getGlobalMutex());
	SYSCALL_PROLOGUE("pwrite");
	return 0;
}

ssize_t read(int fd, void *buf, size_t count) {
	// TODO: simplified version
	Libnorsim &instance = Libnorsim::getInstance();
	std::lock_guard<std::mutex> lg(instance.getGlobalMutex());
	SYSCALL_PROLOGUE("read");
	return (instance.getSyscallsCache().invokeRead(fd, buf, count));
}

ssize_t write(int fd, const void *buf, size_t count) {
	// TODO: simplified version
	Libnorsim &instance = Libnorsim::getInstance();
	std::lock_guard<std::mutex> lg(instance.getGlobalMutex());
	SYSCALL_PROLOGUE("write");
	return (instance.getSyscallsCache().invokeWrite(fd, buf, count));
}

int ioctl(int fd, unsigned long request, ...) {
	Libnorsim &instance = Libnorsim::getInstance();
	std::lock_guard<std::mutex> lg(instance.getGlobalMutex());
	SYSCALL_PROLOGUE("ioctl");
	int res;

	va_list args;
	va_start(args, request);

	if (fd != instance.getCacheFileFd())
		res = instance.getSyscallsCache().invokeIoctl(fd, request, args);
	else
		res = internal_ioctl(instance, fd, request, args);

	va_end(args);
	return (res);
}

void sig_handler(int signum)
{
	switch (signum) {
		case SIGUSR1: report_requested = SIGNAL_REPORT_SHORT; break;
		case SIGUSR2: report_requested = SIGNAL_REPORT_DETAILED; break;
		default: break;
	}
}

static int internal_open(Libnorsim &libnorsim, const char *path, int oflag, va_list args) {
	int errno_cpy;
	int ret;

	if (!libnorsim.isOpened()) {
		ret = libnorsim.getSyscallsCache().invokeOpen(path, oflag, args);
		if (ret < 0) {
			errno_cpy = errno;
			libnorsim.getLogger().log(Loglevel::FATAL, "Couldn't open cache file: %s, errno=%d", false, path, errno_cpy);
			goto err;
		}
		libnorsim.setCacheFileFd(ret);
		if (flock(libnorsim.getCacheFileFd(), LOCK_EX) < 0) {
			errno_cpy = errno;
			libnorsim.getLogger().log(Loglevel::FATAL, "Error while acquiring lock on cache file: %s, errno=%d", false, path, errno_cpy);
			goto err;
		}
		libnorsim.setOpened();
		libnorsim.getLogger().log(Loglevel::INFO, "Opened cache file: %s", false, path);
	} else {
		libnorsim.getLogger().log(Loglevel::ERROR, "Couldn't re-open cache file which is in use");
		goto err;
	}
	return (ret);

err:
	exit(-1);
	return (-1);
}

static int internal_close(Libnorsim &libnorsim, int fd) {
	int errno_cpy;
	int ret;

	if (libnorsim.isOpened()) {
		ret = libnorsim.getSyscallsCache().invokeClose(fd);
		if (ret < 0) {
			errno_cpy = errno;
			libnorsim.getLogger().log(Loglevel::FATAL, "Error while closing cache file: %s, errno=%d", false, libnorsim.getCacheFile(), errno_cpy);
			goto err;
		}
		libnorsim.setClosed();
		libnorsim.setCacheFileFd(-1);
		libnorsim.getLogger().log(Loglevel::INFO, "Closed cache file: %s", false, libnorsim.getCacheFile());
	} else {
		libnorsim.getLogger().log(Loglevel::ERROR, "Couldn't close not opened cache file");
		goto err;
	}
	return (0);

err:
	exit(-1);
	return (-1);
}

static int internal_ioctl_memgetinfo(Libnorsim &libnorsim, int fd, unsigned long request, va_list args) {

}

static int internal_ioctl_memunlock(Libnorsim &libnorsim, int fd, unsigned long request, va_list args) {

}

static int internal_ioctl_memerase(Libnorsim &libnorsim, int fd, unsigned long request, va_list args) {

}

static int internal_ioctl(Libnorsim &libnorsim, int fd, unsigned long request, va_list args) {
	switch (request) {
		case MEMGETINFO: return (internal_ioctl_memgetinfo(libnorsim, fd, request, args));
		case MEMUNLOCK: return (internal_ioctl_memunlock(libnorsim, fd, request, args));
		case MEMERASE: return (internal_ioctl_memerase(libnorsim, fd, request, args));
	}
}

} // extern "C"
