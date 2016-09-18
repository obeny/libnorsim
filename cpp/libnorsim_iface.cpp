#include <cstdio>

#include "Libnorsim.h"

extern "C" {
int open(const char *path, int oflag, ...)
{
	Libnorsim &instance = Libnorsim::getInstance();

	puts("open");
	return 0;
}

int close(int fd)
{
	// Libnorsim *instance = Libnorsim::getInstance();

	puts("close");
	return 0;
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset)
{
	// Libnorsim *instance = Libnorsim::getInstance();

	puts("pread");
	return 0;
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset)
{
	// Libnorsim *instance = Libnorsim::getInstance();

	puts("pwrite");
	return 0;
}

ssize_t read(int fd, void *buf, size_t count)
{
	// Libnorsim *instance = Libnorsim::getInstance();

	puts("read");
	return 0;
}

ssize_t write(int fd, const void *buf, size_t count)
{
	// Libnorsim *instance = Libnorsim::getInstance();

	puts("write");
	return 0;
}

int ioctl(int fd, unsigned long request, ...)
{
	// Libnorsim *instance = Libnorsim::getInstance();
	
	puts("ioctl");
	return 0;
}

} // extern "C"
