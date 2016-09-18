#ifndef __SYSCALLSCACHE_H__
#define __SYSCALLSCACHE_H__

#include <cstdio>

class Libnorsim;

class SyscallsCache {
	typedef int (*open_ptr_t)(const char *path, int oflag, ...);
	typedef int (*close_ptr_t)(int fd);
	typedef ssize_t (*pread_ptr_t)(int fd, void *buf, size_t count, off_t offset);
	typedef ssize_t (*pwrite_ptr_t)(int fd, const void *buf, size_t count, off_t offset);
	typedef ssize_t (*read_ptr_t)(int fd, void *buf, size_t count);
	typedef ssize_t (*write_ptr_t)(int fd, const void *buf, size_t count);
	typedef int (*ioctl_ptr_t)(int fd, unsigned long request, ...);

public:
	SyscallsCache(Libnorsim *libnorsom);

	bool isOk();

	int invokeOpen(const char *path, int oflag, ...);
	int invokeClose(int fd);
	size_t invokePread(int fd, void *buf, size_t count, off_t offset);
	size_t invokePwrite(int fd, const void *buf, size_t count, off_t offset);
	size_t invokeRead(int fd, void *buf, size_t count);
	size_t invokeWrite(int fd, const void *buf, size_t count);
	int invokeIoctl(int fd, unsigned long request, ...);

private:
	bool m_initialized;

	open_ptr_t m_openSC;
	close_ptr_t m_closeSC;
	pread_ptr_t m_preadSC;
	pwrite_ptr_t m_pwriteSC;
	read_ptr_t m_readSC;
	write_ptr_t m_writeSC;
	ioctl_ptr_t m_ioctlSC;

	Libnorsim *m_libnorsim;
};

#endif // __SYSCALSSCACHE_H__
