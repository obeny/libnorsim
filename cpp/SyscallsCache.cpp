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