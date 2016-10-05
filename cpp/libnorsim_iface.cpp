#include <cstdio>

#include <signal.h>

#include "Libnorsim.h"
#include "Logger.h"

#define SYSCALL_PROLOGUE(syscall) \
	do { \
		instance.handleReportRequest(); \
		instance.getLogger().log(Loglevel::NOTE, "handling syscall: %s", false, syscall); \
	} while(0)

extern "C" {

volatile sig_atomic_t report_requested = 0;

int open(const char *path, int oflag, ...) {
	Libnorsim &instance = Libnorsim::getInstance();
	std::lock_guard<std::mutex> lg(instance.getGlobalMutex());
	SYSCALL_PROLOGUE("open");
	return 0;
}

int close(int fd) {
	Libnorsim &instance = Libnorsim::getInstance();
	std::lock_guard<std::mutex> lg(instance.getGlobalMutex());
	SYSCALL_PROLOGUE("close");
	return 0;
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
	return 0;
}

void sig_handler(int signum)
{
	switch (signum) {
		case SIGUSR1: report_requested = SIGNAL_REPORT_SHORT; break;
		case SIGUSR2: report_requested = SIGNAL_REPORT_DETAILED; break;
		default: break;
	}
}

} // extern "C"
