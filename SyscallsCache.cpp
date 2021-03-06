#include <cstdio>
#include <stdexcept>

#include <dlfcn.h>

#include "SyscallsCache.h"

SyscallsCache::SyscallsCache()
{
	if (NULL == (m_syscalls.openSC = reinterpret_cast<Syscalls::open_ptr_t>(dlsym(RTLD_NEXT, "open"))))
		throw std::runtime_error("open");
	if (NULL == (m_syscalls.closeSC = reinterpret_cast<Syscalls::close_ptr_t>(dlsym(RTLD_NEXT, "close"))))
		throw std::runtime_error("close");
	if (NULL == (m_syscalls.preadSC = reinterpret_cast<Syscalls::pread_ptr_t>(dlsym(RTLD_NEXT, "pread"))))
		throw std::runtime_error("pread");
	if (NULL == (m_syscalls.pwriteSC = reinterpret_cast<Syscalls::pwrite_ptr_t>(dlsym(RTLD_NEXT, "pwrite"))))
		throw std::runtime_error("pwrite");
	if (NULL == (m_syscalls.readSC = reinterpret_cast<Syscalls::read_ptr_t>(dlsym(RTLD_NEXT, "read"))))
		throw std::runtime_error("read");
	if (NULL == (m_syscalls.writeSC = reinterpret_cast<Syscalls::write_ptr_t>(dlsym(RTLD_NEXT, "write"))))
		throw std::runtime_error("write");
	if (NULL == (m_syscalls.ioctlSC = reinterpret_cast<Syscalls::ioctl_ptr_t>(dlsym(RTLD_NEXT, "ioctl"))))
		throw std::runtime_error("ioctl");
}
