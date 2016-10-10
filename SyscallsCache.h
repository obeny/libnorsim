#ifndef __SYSCALLSCACHE_H__
#define __SYSCALLSCACHE_H__

#include <errno.h>
#include <sys/stat.h>

class SyscallsCache {
	class Syscalls {
	public:
		typedef int (*open_ptr_t)(const char *path, int oflag, ...);
		typedef int (*close_ptr_t)(int fd);
		typedef ssize_t (*pread_ptr_t)(int fd, void *buf, size_t count, off_t offset);
		typedef ssize_t (*pwrite_ptr_t)(int fd, const void *buf, size_t count, off_t offset);
		typedef ssize_t (*read_ptr_t)(int fd, void *buf, size_t count);
		typedef ssize_t (*write_ptr_t)(int fd, const void *buf, size_t count);
		typedef int (*ioctl_ptr_t)(int fd, unsigned long request, ...);

		Syscalls() {}

		int getLastErrno() { return (m_lastErrno); }

		template<typename T, typename... Args>
		auto invoke(T syscallPtr, Args... args) {
			auto ret = syscallPtr(args...);
			m_lastErrno = errno;
			return (ret);
		}

		open_ptr_t openSC;
		close_ptr_t closeSC;
		pread_ptr_t preadSC;
		pwrite_ptr_t pwriteSC;
		read_ptr_t readSC;
		write_ptr_t writeSC;
		ioctl_ptr_t ioctlSC;

	private:
		int m_lastErrno;
	};

public:
	SyscallsCache();

	Syscalls& getSyscalls() { return (m_syscalls); }

private:
	Syscalls m_syscalls;

public:
	int invokeOpen(const char *path, int oflag, mode_t mode)
		{ return (m_syscalls.invoke<Syscalls::open_ptr_t>(m_syscalls.openSC, path, oflag, mode)); }
	int invokeClose(int fd)
		{ return (m_syscalls.invoke<Syscalls::close_ptr_t>(m_syscalls.closeSC, fd)); }
	size_t invokePread(int fd, void *buf, size_t count, off_t offset)
		{ return (m_syscalls.invoke<Syscalls::pread_ptr_t>(m_syscalls.preadSC, fd, buf, count, offset)); }
	size_t invokePwrite(int fd, const void *buf, size_t count, off_t offset)
		{ return (m_syscalls.invoke<Syscalls::pwrite_ptr_t>(m_syscalls.pwriteSC, fd, buf, count, offset)); }
	size_t invokeRead(int fd, void *buf, size_t count)
		{ return (m_syscalls.invoke<Syscalls::read_ptr_t>(m_syscalls.readSC, fd, buf, count)); }
	size_t invokeWrite(int fd, const void *buf, size_t count)
		{ return (m_syscalls.invoke<Syscalls::write_ptr_t>(m_syscalls.writeSC, fd, buf, count)); }
	int invokeIoctl(int fd, unsigned long request, va_list args)
		{ return (m_syscalls.invoke<Syscalls::ioctl_ptr_t>(m_syscalls.ioctlSC, fd, request, args)); }
};

#endif // __SYSCALSSCACHE_H__
