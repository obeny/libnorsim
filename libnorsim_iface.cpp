#include <cstring>

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
static int internal_pread(Libnorsim &libnorsim, int fd, void *buf, size_t count, off_t offset);
static int internal_pwrite(Libnorsim &libnorsim, int fd, const void *buf, size_t count, off_t offset);
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
	int res;

	if (fd != instance.getCacheFileFd())
		res = instance.getSyscallsCache().invokePread(fd, buf, count, offset);
	else
		res = internal_pread(instance, fd, buf, count, offset);

	return (res);
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
	Libnorsim &instance = Libnorsim::getInstance();
	std::lock_guard<std::mutex> lg(instance.getGlobalMutex());
	SYSCALL_PROLOGUE("pwrite");
	int res;

	if (fd != instance.getCacheFileFd())
		res = instance.getSyscallsCache().invokePwrite(fd, buf, count, offset);
	else
		res = internal_pwrite(instance, fd, buf, count, offset);

	return (res);
}

ssize_t read(int fd, void *buf, size_t count) {
	// TODO: simplified version
	Libnorsim &instance = Libnorsim::getInstance();
	std::lock_guard<std::mutex> lg(instance.getGlobalMutex());
	SYSCALL_PROLOGUE("read");
	instance.getLogger().log(Loglevel::NOTE, "TODO: stub bypassing to real read function");
	return (instance.getSyscallsCache().invokeRead(fd, buf, count));
}

ssize_t write(int fd, const void *buf, size_t count) {
	// TODO: simplified version
	Libnorsim &instance = Libnorsim::getInstance();
	std::lock_guard<std::mutex> lg(instance.getGlobalMutex());
	SYSCALL_PROLOGUE("write");
	instance.getLogger().log(Loglevel::NOTE, "TODO: stub bypassing to real write function");
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

static int internal_pread(Libnorsim &libnorsim, int fd, void *buf, size_t count, off_t offset) {
	int ret = 0;
	unsigned index = offset / libnorsim.getEraseSize();

	libnorsim.getPageInfo()[index].reads++;
	if (E_PAGE_GRAVE == libnorsim.getPageInfo()[index].type) {
		if (libnorsim.getPageInfo()[index].current_cycles < libnorsim.getPageInfo()[index].cycles) {
			libnorsim.getPageInfo()[index].current_cycles++;
			return (libnorsim.getSyscallsCache().invokePread(fd, buf, count, offset));
		} else {
			if (E_BEH_EIO == libnorsim.getGravePageBehavior()) {
				libnorsim.getLogger().log(Loglevel::DEBUG, "EIO error at page: %lu", false, index);
				return (-1);
			} else {
				unsigned index_in = offset - index * libnorsim.getEraseSize();
				if ((index_in + count) > libnorsim.getEraseSize()) {
					libnorsim.getLogger().log(Loglevel::WARNING, "Read block exceeds eraseblock boundary");
				}
				ret = libnorsim.getSyscallsCache().invokePread(fd, buf, count, offset);
				unsigned long rnd = rand() % count;
				char rnd_byte = ((char*)buf)[rnd] ^ rnd;
				libnorsim.getLogger().log(Loglevel::DEBUG, "RND error at page: %lu[%lu], expected: 0x%02X, is 0x%02X", false, index, index_in + rnd, ((char*)buf)[rnd], rnd_byte);
				((char*)buf)[rnd] = rnd_byte;
			}
		}
	} else {
		return (libnorsim.getSyscallsCache().invokePread(fd, buf, count, offset));
	}

	return (ret);
}

static int internal_pwrite(Libnorsim &libnorsim, int fd, const void *buf, size_t count, off_t offset) {
	int ret = 0;
	unsigned index = offset / libnorsim.getEraseSize();
	
	libnorsim.getPageInfo()[index].writes++;
	if (E_PAGE_WEAK == libnorsim.getPageInfo()[index].type) {
		if (libnorsim.getPageInfo()[index].current_cycles < libnorsim.getPageInfo()[index].cycles) {
			libnorsim.getPageInfo()[index].current_cycles++;
			return (libnorsim.getSyscallsCache().invokePwrite(fd, buf, count, offset));
		} else {
			if (E_BEH_EIO == libnorsim.getWeakPageBehavior()) {
				libnorsim.getLogger().log(Loglevel::DEBUG, "EIO error at page: %lu", false, index);
				return (-1);
			} else {
				unsigned index_in = offset - index * libnorsim.getEraseSize();
				if ((index_in + count) > libnorsim.getEraseSize()) {
					libnorsim.getLogger().log(Loglevel::WARNING, "Write block exceeds eraseblock boundary");
					return (-1);
				}
				unsigned long rnd = rand() % count;
				char rnd_byte = ((const char*)buf)[rnd] ^ rnd;
				libnorsim.getLogger().log(Loglevel::DEBUG, "RND error at page: %lu[%lu], expected: 0x%02X, is 0x%02X", false, index, index_in + rnd, ((const char*)buf)[rnd], rnd_byte);
				((char*)buf)[rnd] = rnd_byte;
				ret = libnorsim.getSyscallsCache().invokePwrite(fd, buf, count, offset);
			}
		}
	} else {
		return (libnorsim.getSyscallsCache().invokePwrite(fd, buf, count, offset));
	}

	return (ret);
}

static int internal_ioctl_memgetinfo(Libnorsim &libnorsim, va_list args) {
	mtd_info_t *mi = va_arg(args, mtd_info_t*);
	libnorsim.getLogger().log(Loglevel::DEBUG, "Got MEMGETINFO request");
	memcpy(mi, libnorsim.getMtdInfo(), sizeof(mtd_info_t));
	return (0);
}

static int internal_ioctl_memunlock(Libnorsim &libnorsim, va_list args) {
	erase_info_t *ei = va_arg(args, erase_info_t*);
	unsigned index = (ei->start) / libnorsim.getEraseSize();
	libnorsim.getLogger().log(Loglevel::DEBUG, "Got MEMUNLOCK request at page: %d", false, index);

	if ((0 != (ei->start % libnorsim.getEraseSize()) || 0 != (ei->length % libnorsim.getEraseSize()))) {
		libnorsim.getLogger().log(Loglevel::WARNING, "Invalid erase_info_t, start=0x%04lX, length=0x%04lX",
			false, (unsigned long)ei->start, (unsigned long)ei->length);
		return (-1);
	}

	libnorsim.getPageInfo()[(ei->start) / libnorsim.getEraseSize()].unlocked = 1;
	
	return (0);
}

static int internal_ioctl_memerase(Libnorsim &libnorsim, int fd, va_list args) {
	int ret = 0;
	erase_info_t *ei = va_arg(args, erase_info_t*);
	unsigned index = (ei->start) / libnorsim.getEraseSize();
	libnorsim.getLogger().log(Loglevel::DEBUG, "Got MEMERASE request at page: %d", false, index);
	
	if ((0 != (ei->start % libnorsim.getEraseSize()) || 0 != (ei->length % libnorsim.getEraseSize()))) {
		libnorsim.getLogger().log(Loglevel::WARNING, "Invalid erase_info_t, start=0x%04lX, length=0x%04lX",
			false, (unsigned long)ei->start, (unsigned long)ei->length);
		return (-1);
	}

	if (libnorsim.getPageInfo()[index].unlocked) {
		libnorsim.getPageInfo()[index].erases++;
		if (libnorsim.getPageInfo()[index].current_cycles < libnorsim.getPageInfo()[index].cycles) {
			libnorsim.getPageInfo()[index].current_cycles++;
			libnorsim.getPageInfo()[index].unlocked = 0;
			return (libnorsim.getSyscallsCache().invokePwrite(
				libnorsim.getCacheFileFd(), libnorsim.getEraseBuffer(), ei->length, ei->start));
		}
		if (E_BEH_EIO == libnorsim.getWeakPageBehavior()) {
			libnorsim.getLogger().log(Loglevel::DEBUG, "EIO error at page: %lu", false, index);
			libnorsim.getPageInfo()[index].unlocked = 0;
			return (-1);
		} else {
			unsigned rnd = rand() % ei->length;
			libnorsim.getLogger().log(Loglevel::DEBUG, "RND error at page: %lu[%d] = 0x%02X", false, index, rnd, (~rnd) & 0xFF);
			libnorsim.getEraseBuffer()[rnd] = ~rnd;
			ret = libnorsim.getSyscallsCache().invokePwrite(fd, libnorsim.getEraseBuffer(), ei->length, ei->start);
			libnorsim.getEraseBuffer()[rnd] = 0xFF;
		}
		libnorsim.getPageInfo()[index].unlocked = 0;
	} else {
		libnorsim.getLogger().log(Loglevel::WARNING, "Page %ld locked, rejecting erase request", false, index);
		return (-1);
	}

	return (ret);
}

static int internal_ioctl(Libnorsim &libnorsim, int fd, unsigned long request, va_list args) {
	switch (request) {
		case MEMGETINFO: return (internal_ioctl_memgetinfo(libnorsim, args));
		case MEMUNLOCK: return (internal_ioctl_memunlock(libnorsim, args));
		case MEMERASE: return (internal_ioctl_memerase(libnorsim, fd, args));
	}
	return (-1);
}

} // extern "C"
