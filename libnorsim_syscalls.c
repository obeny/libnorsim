#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <dlfcn.h>
#include <errno.h>
#include <limits.h>

#include <sys/ioctl.h>
#include <sys/file.h>

#include <mtd/mtd-user.h>

#include "libnorsim.h"

extern pthread_mutex_t mutex;
extern mtd_info_t mtd_info;
extern st_page_t *page_info;
extern st_syscalls_t real_syscalls;
extern e_beh_t beh_weak;
extern e_beh_t beh_grave;

extern char *cache_file;
extern char *erase_buffer;
extern int initialized;
extern int cache_file_fd;
extern unsigned loglevel;
extern unsigned long erase_size;

static int syscall_init_done;

static void initialize_syscall(const e_syscall_t syscall)
{
	if (0 == syscall_init_done) {
		norsim_init();
		syscall_init_done = 1;
	}

	switch (syscall) {
		case E_SYSCALL_OPEN:
			if (!real_syscalls._open)
				real_syscalls._open = dlsym(RTLD_NEXT, "open");
			break;
		case E_SYSCALL_CLOSE:
			if (!real_syscalls._close)
				real_syscalls._close = dlsym(RTLD_NEXT, "close");
			break;
		case E_SYSCALL_PREAD:
			if (!real_syscalls._pread)
				real_syscalls._pread = dlsym(RTLD_NEXT, "pread");
			break;
		case E_SYSCALL_PWRITE:
			if (!real_syscalls._pwrite)
				real_syscalls._pwrite = dlsym(RTLD_NEXT, "pwrite");
			break;
		case E_SYSCALL_READ:
			if (!real_syscalls._read)
				real_syscalls._read = dlsym(RTLD_NEXT, "read");
			break;
		case E_SYSCALL_WRITE:
			if (!real_syscalls._write)
				real_syscalls._write = dlsym(RTLD_NEXT, "write");
			break;
		case E_SYSCALL_IOCTL:
			if (!real_syscalls._ioctl)
				real_syscalls._ioctl = dlsym(RTLD_NEXT, "ioctl");
			break;
	}
}

// ---------------------------------------
int open(const char *path, int oflag, ...)
{
	char realpath_buf[PATH_MAX + 1];
	int errno_cpy;
	int ret = -1;
	va_list args;

	report();
	initialize_syscall(E_SYSCALL_OPEN);
	va_start(args, oflag);
	if ((NULL != realpath(path, realpath_buf)) && (0 != strcmp(realpath_buf, cache_file)))
		ret = real_syscalls._open(path, oflag, args);
	else {
		if (!initialized) {
			pthread_mutex_lock(&mutex);
			ret = real_syscalls._open(path, oflag, args);
			if (ret < 0) {
				errno_cpy = errno;
				PERR("Error while opening cache_file: %s, errno=%d\n", cache_file, errno_cpy);
				goto err;
			}
			cache_file_fd = ret;
			if (flock(cache_file_fd, LOCK_EX) < 0) {
				errno_cpy = errno;
				PERR("Error while acquiring lock on cache_file: %s, errno=%d\n", cache_file, errno_cpy);
				goto err;
			}
			PALL(1, "Opened cache_file: %s\n", cache_file);
			initialized = 1;
			pthread_mutex_unlock(&mutex);
		} else {
			PERR("Couldn't re-open cache_file in use: %s\n", cache_file);
			goto err;
		}
	}
	va_end(args);
	return (ret);

err:
	va_end(args);
	pthread_mutex_unlock(&mutex);
	norsim_shutdown();
	return (-1);
}

// --------------
int close(int fd)
{
	int errno_cpy;
	int ret = -1;

	report();
	initialize_syscall(E_SYSCALL_CLOSE);
	if (fd != cache_file_fd)
		ret = real_syscalls._close(fd);
	else {
		if (initialized) {
			pthread_mutex_lock(&mutex);
			ret = real_syscalls._close(fd);
			if (ret < 0) {
				errno_cpy = errno;
				PERR("Error while closing cache_file: %s, errno=%d\n", cache_file, errno_cpy);
				goto err;
			}
			PALL(1, "Closed cache_file: %s\n", cache_file);
			initialized = 0;
			cache_file_fd = -1;
			pthread_mutex_unlock(&mutex);
		} else {
			PERR("Couldn't close uninitialized cache_file: %s\n", cache_file);
			goto err;
		}
	}
	return (ret);

err:
	pthread_mutex_unlock(&mutex);
	norsim_shutdown();
	return (-1);
}

// ---------------------------------------------------------
ssize_t pread(int fd, void *buf, size_t count, off_t offset)
{
	int ret = -1;
	int rnd;
	char rnd_byte;
	unsigned long index;
	unsigned long index_in;

	report();
	initialize_syscall(E_SYSCALL_PREAD);
	if (fd != cache_file_fd)
		ret = real_syscalls._pread(fd, buf, count, offset);
	else {
		index = get_page_by_offset(offset);
		switch (get_page_type_by_index(index)) {
			case E_PAGE_GRAVE:
				pthread_mutex_lock(&mutex);
				page_info[index].reads++;
				if (page_info[index].current_cycles < page_info[index].cycles) {
					page_info[index].current_cycles++;
					pthread_mutex_unlock(&mutex);
					ret = real_syscalls._pread(fd, buf, count, offset);
				} else {
					pthread_mutex_unlock(&mutex);
					if (E_BEH_EIO == beh_grave) {
						PDBG("EIO at page: %lu\n", index);
						ret = -1;
					} else {
						index_in = offset - index * erase_size;
						if ((index_in + count) > erase_size) {
							PDBG("read block exceeds eraseblock size\n");
							return (-1);
						}
						ret = real_syscalls._pread(fd, buf, count, offset);
						rnd = rand() % count;
						rnd_byte = ((char*)buf)[rnd] ^ rnd;
						PDBG("RND at page: %lu[%lu], expected: 0x%02X, is: 0x%02X\n", index, index_in + rnd, ((char*)buf)[rnd], rnd_byte & 0xFF);
						((char*)buf)[rnd] = rnd_byte & 0xFF;
					}
				}
				break;
			case E_PAGE_NORMAL:
			case E_PAGE_WEAK:
				pthread_mutex_lock(&mutex);
				page_info[index].reads++;
				pthread_mutex_unlock(&mutex);
			default:
				ret = real_syscalls._pread(fd, buf, count, offset);
		}
	}
	return (ret);
}

// ----------------------------------------------------------------
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset)
{
	int ret = -1;
	int rnd;
	char rnd_byte;
	unsigned long index;
	unsigned long index_in;

	report();
	initialize_syscall(E_SYSCALL_PWRITE);
	if (fd != cache_file_fd)
		ret = real_syscalls._pwrite(fd, buf, count, offset);
	else {
		index = get_page_by_offset(offset);
		switch (get_page_type_by_index(index)) {
			case E_PAGE_WEAK:
				pthread_mutex_lock(&mutex);
				page_info[index].writes++;
				if (page_info[index].current_cycles < page_info[index].cycles) {
					page_info[index].current_cycles++;
					pthread_mutex_unlock(&mutex);
					ret = real_syscalls._pwrite(fd, buf, count, offset);
				} else {
					pthread_mutex_unlock(&mutex);
					if (E_BEH_EIO == beh_weak) {
						PDBG("EIO at page: %lu\n", index);
						ret = -1;
					} else {
						index_in = offset - index * erase_size;
						if ((index_in + count) > erase_size) {
							PDBG("write block exceeds eraseblock size\n");
							return (-1);
						}
						rnd = rand() % count;
						rnd_byte = ((const char*)buf)[rnd] ^ rnd;
						PDBG("RND at page: %lu[%lu], expected: 0x%02X, is: 0x%02X\n", index, index_in + rnd, ((const char*)buf)[rnd], rnd_byte & 0xFF);
						((char*)buf)[rnd] = (char)(rnd_byte & 0xFF);
						ret = real_syscalls._pwrite(fd, buf, count, offset);
					}
				}
				break;
			case E_PAGE_NORMAL:
			case E_PAGE_GRAVE:
				pthread_mutex_lock(&mutex);
				page_info[index].writes++;
				pthread_mutex_unlock(&mutex);
			default:
				ret = real_syscalls._pwrite(fd, buf, count, offset);
		}
	}
	return (ret);
}

// ------------------------------------------
ssize_t read(int fd, void *buf, size_t count)
{
	// TODO: simplified version
	report();
	initialize_syscall(E_SYSCALL_READ);
	return (real_syscalls._read(fd, buf, count));
}

// -------------------------------------------------
ssize_t write(int fd, const void *buf, size_t count)
{
	// TODO: simplified version
	report();
	initialize_syscall(E_SYSCALL_WRITE);
	return (real_syscalls._write(fd, buf, count));
}

// ------------------------------------------
int ioctl(int fd, unsigned long request, ...)
{
	int ret = -1;
	int rnd;
	unsigned long index;
	va_list args;
	mtd_info_t *mi;
	erase_info_t *ei;

	report();
	initialize_syscall(E_SYSCALL_IOCTL);
	va_start(args, request);
	if (fd != cache_file_fd)
		ret = real_syscalls._ioctl(fd, request, args);
	else {
		switch (request) {
			case MEMGETINFO:
				ret = 0;
				mi = va_arg(args, mtd_info_t*);
				memcpy(mi, &mtd_info, sizeof(mtd_info));
				PDBG("got MEMGETINFO request\n");
				break;
			case MEMUNLOCK:
				ret = 0;				
				PDBG("got MEMUNLOCK request\n");
				ei = va_arg(args, erase_info_t*);
				if ((0 != (ei->start % erase_size) || 0 != (ei->length % erase_size))) {
					PINF("erase_info_t invalid, start=0x%04lx, length=0x%04lx\n", (unsigned long)ei->start, (unsigned long)ei->length);
					ret = -1;
					break;
				}
				index = get_page_by_offset(ei->start);
				pthread_mutex_lock(&mutex);
				page_info[index].unlocked = 1;
				pthread_mutex_unlock(&mutex);
				ret = 0;
				break;
			case MEMERASE:
				PDBG("got MEMERASE request\n");
				ei = va_arg(args, erase_info_t*);
				if ((0 != (ei->start % erase_size) || 0 != (ei->length % erase_size))) {
					PINF("erase_info_t invalid, start=0x%04lx, length=0x%04lx\n", (unsigned long)ei->start, (unsigned long)ei->length);
					ret = -1;
					break;
				}
				index = get_page_by_offset(ei->start);
				pthread_mutex_lock(&mutex);
				if (page_info[index].unlocked) {
					if (page_info[index].current_cycles < page_info[index].cycles) {
						page_info[index].current_cycles++;
						pthread_mutex_unlock(&mutex);
						ret = real_syscalls._pwrite(cache_file_fd, erase_buffer, ei->length, ei->start);
						break;
					}
					page_info[index].erases++;
					if (E_BEH_EIO == beh_weak) {
						PDBG("EIO at page: %lu\n", index);
						ret = -1;
					} else {
						rnd = rand() % ei->length;
						PDBG("RND at page: %lu[%d] = 0x%02X\n", index, rnd, (~rnd) & 0xFF);
						((char*)erase_buffer)[rnd] = ~rnd;
						ret = real_syscalls._pwrite(cache_file_fd, erase_buffer, ei->length, ei->start);
						((char*)erase_buffer)[rnd] = 0xFF;
					}
					page_info[index].unlocked = 0;
					pthread_mutex_unlock(&mutex);
				} else {
					PINF("page %ld locked, refusing erase\n", index);
					ret = -1;
					break;
				}
				ret = 0;
				break;
			default:
				break;
		}
	}
	va_end(args);

	return (ret);
}
