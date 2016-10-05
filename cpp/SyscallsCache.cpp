#include "SyscallsCache.h"

#include "Libnorsim.h"
#include "Logger.h"

#include <dlfcn.h>

SyscallsCache::SyscallsCache(Libnorsim *libnorsim)
	: m_initialized(true), m_libnorsim(libnorsim)
{
	if (NULL == (m_openSC = reinterpret_cast<open_ptr_t>(dlsym(RTLD_NEXT, "open")))) {
		m_libnorsim->getLogger().log(Loglevel::ERROR, "Couldn't initialize \"open\" syscall");
		m_initialized = false;
	}
	if (NULL == (m_closeSC = reinterpret_cast<close_ptr_t>(dlsym(RTLD_NEXT, "close")))) {
		m_libnorsim->getLogger().log(Loglevel::ERROR, "Couldn't initialize \"close\" syscall");
		m_initialized = false;
	}
	if (NULL == (m_preadSC = reinterpret_cast<pread_ptr_t>(dlsym(RTLD_NEXT, "pread")))) {
		m_libnorsim->getLogger().log(Loglevel::ERROR, "Couldn't initialize \"pread\" syscall");
		m_initialized = false;
	}
	if (NULL == (m_pwriteSC = reinterpret_cast<pwrite_ptr_t>(dlsym(RTLD_NEXT, "pwrite")))) {
		m_libnorsim->getLogger().log(Loglevel::ERROR, "Couldn't initialize \"pwrite\" syscall");
		m_initialized = false;
	}
	if (NULL == (m_readSC = reinterpret_cast<read_ptr_t>(dlsym(RTLD_NEXT, "read")))) {
		m_libnorsim->getLogger().log(Loglevel::ERROR, "Couldn't initialize \"read\" syscall");
		m_initialized = false;
	}
	if (NULL == (m_writeSC = reinterpret_cast<write_ptr_t>(dlsym(RTLD_NEXT, "write")))) {
		m_libnorsim->getLogger().log(Loglevel::ERROR, "Couldn't initialize \"write\" syscall");
		m_initialized = false;
	}
	if (NULL == (m_ioctlSC = reinterpret_cast<ioctl_ptr_t>(dlsym(RTLD_NEXT, "ioctl")))) {
		m_libnorsim->getLogger().log(Loglevel::ERROR, "Couldn't initialize \"ioctl\" syscall");
		m_initialized = false;
	}

	if (!m_initialized)
		m_libnorsim->getLogger().log(Loglevel::FATAL, "SyscallsCache is not fully initialized");
	else
		m_libnorsim->getLogger().log(Loglevel::DEBUG, "SyscallsCache init OK");
}

bool SyscallsCache::isOk() {
	return (m_initialized);
}

int SyscallsCache::invokeOpen(const char *path, int oflag, ...) {
	va_list args;
	va_start(args, oflag);
	return (m_openSC(path, oflag, args));
}

int SyscallsCache::invokeClose(int fd) {
	return (m_closeSC(fd));
}

size_t SyscallsCache::invokePread(int fd, void *buf, size_t count, off_t offset){
	return (m_preadSC(fd, buf, count, offset));
}

size_t SyscallsCache::invokePwrite(int fd, const void *buf, size_t count, off_t offset) {
	return (m_pwriteSC(fd, buf, count, offset));
}

size_t SyscallsCache::invokeRead(int fd, void *buf, size_t count) {
	return (m_readSC(fd, buf, count));
}

size_t SyscallsCache::invokeWrite(int fd, const void *buf, size_t count) {
	return (m_writeSC(fd, buf, count));
}

int SyscallsCache::invokeIoctl(int fd, unsigned long request, ...) {
	va_list args;
	va_start(args, request);
	return (m_ioctlSC(fd, request, args));
}
