// TODO:
// normal pages report
// empty_string
// sighandler
// random
// improve exit
// bit-flips ??

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>

#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <mtd/mtd-user.h>

#include "libnorsim.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static char cache_file_realpath[PATH_MAX + 1];

static char empty_string[1];

static char *cache_file;
static unsigned loglevel = E_LOGLEVEL_INFO;
static unsigned long pages;
static unsigned long size;
static unsigned long erase_size;

static int initialized;
static int init_done;
static st_page_t *page_info;

static int cache_file_fd = -1;
static off_t cache_file_size;

static e_beh_t beh_weak;
static e_beh_t beh_grave;

static mtd_info_t mtd_info;

static st_syscalls_t real_syscalls;

static void norsim_init(void);
static void sig_handler_USR1(int signum);
static void usage(void);
static void report_pages(void);
static void report_stats(void);
static void shutdown(void);
static int parse_page_env(const char const *str, e_page_type_t type);

inline static unsigned get_page_by_offset(off_t offset);
inline static e_page_type_t get_page_type_by_index(unsigned index);

static void initialize(const e_syscall_t syscall)
{
	if (0 == init_done) {
		norsim_init();
		init_done = 1;
	}

	switch (syscall) {
		case E_SYSCALL_OPEN:
			if (!real_syscalls._open) {
				real_syscalls._open = dlsym(RTLD_NEXT, "open");
			}
			break;
		case E_SYSCALL_CLOSE:
			if (!real_syscalls._close) {
				real_syscalls._close = dlsym(RTLD_NEXT, "close");
			}
			break;
		case E_SYSCALL_PREAD:
			if (!real_syscalls._pread) {
				real_syscalls._pread = dlsym(RTLD_NEXT, "pread");
			}
			break;
		case E_SYSCALL_PWRITE:
			if (!real_syscalls._pwrite) {
				real_syscalls._pwrite = dlsym(RTLD_NEXT, "pwrite");
			}
			break;
		case E_SYSCALL_READ:
			if (!real_syscalls._read) {
				real_syscalls._read = dlsym(RTLD_NEXT, "read");
			}
			break;
		case E_SYSCALL_WRITE:
			if (!real_syscalls._write) {
				real_syscalls._write = dlsym(RTLD_NEXT, "write");
			}
			break;
		case E_SYSCALL_IOCTL:
			if (!real_syscalls._ioctl) {
				real_syscalls._ioctl = dlsym(RTLD_NEXT, "ioctl");
			}
			break;
	}
}

static void norsim_init(void)
{
	char *env_loglevel;
	char *env_size;
	char *env_erase_size;

	char *env_weak_pages;
	char *env_grave_pages;
	char *env_bit_flips;

	int res = 0;

	struct stat st;

	puts("Initializing norsim...");
	/*
	 * CHECK VARIABLES
	 */
	// LOGLEVEL
	if (NULL == (env_loglevel = getenv(ENV_LOGLEVEL)))
		PINF("No loglevel given, assuming INFO level\n");
	else
		loglevel = (unsigned)strtoul(env_loglevel, NULL, 10);
	PALL(1, "SET loglevel:\t\t%d\n", loglevel);
	// CACHE FILE
	if (NULL == (cache_file = getenv(ENV_CACHE_FILE))) {
		PERR("No cache_file given, interrupting!\n");
		goto err;
	}
	cache_file = realpath(cache_file, cache_file_realpath);
	PALL(1, "SET cache_file:\t\t%s\n", cache_file);
	// SIZES
	if (NULL == (env_size = getenv(ENV_SIZE))) {
		PERR("No size given, interrupting!\n");
		goto err;
	}
	else
		size = (unsigned)strtoul(env_size, NULL, 10) * 1024;
	PALL(1, "SET size:\t\t0x%04lx (%lukB)\n", size, size / 1024);
	if (NULL == (env_erase_size = getenv(ENV_ERASE_SIZE))) {
		PERR("No erase_size given, interrupting!\n");
		goto err;
	}
	else
		erase_size = (unsigned)strtoul(env_erase_size, NULL, 10) * 1024;
	PALL(1, "SET erase_size:\t\t0x%04lx (%lukB)\n", erase_size, erase_size / 1024);
	// WEAK PAGES
	if (NULL == (env_weak_pages = getenv(ENV_WEAK_PAGES))) {
		PINF("No weak_pages given, assuming no weak pages\n");
		env_weak_pages = empty_string;
	}
	// GRAVE PAGES
	if (NULL == (env_grave_pages = getenv(ENV_GRAVE_PAGES))) {
		PINF("No grave_pages given, assuming no grave pages\n");
		env_grave_pages = empty_string;
	}
	// BIT-FLIPS
	if (NULL == (env_bit_flips = getenv(ENV_BIT_FLIPS))) {
		PINF("No bit_flips given, assuming no bit-flips\n");
	}
	// NO FAULTS
	if (!env_weak_pages && !env_grave_pages && !env_bit_flips)
		PINF("No failure types defined, no faults will be forwareded to user program\n");
	// CALCULATE PAGES
	pages = size / erase_size;
	PALL(1, "SET pages:\t\t%lu\n", pages);
	/*
	 * VALIDATE PAGE SYNTAX
	 */
	page_info = (st_page_t*)malloc(sizeof(st_page_t) * pages);
	if (NULL == page_info) {
		PERR("Couldn't allocate memory for page_info!\n");
		goto err;
	}
	memset(page_info, 0x00, sizeof(st_page_t) * pages);

	// WEAK PAGES
	if (0 == strncmp(env_weak_pages, PARSE_BEH_EIO, PARSE_BEH_LEN))
	{
		beh_weak = E_BEH_EIO;
		PALL(1, "SET weak behavior:\t%s\n", PARSE_BEH_EIO);
		env_weak_pages = strchr(env_weak_pages, PARSE_PREFIX_DELIM) + 1;
	}
	else if (0 == strncmp(env_weak_pages, PARSE_BEH_RND, PARSE_BEH_LEN))
	{
		beh_weak = E_BEH_RND;
		PALL(1, "SET weak behavior:\t%s\n", PARSE_BEH_RND);
		env_weak_pages = strchr(env_weak_pages, PARSE_PREFIX_DELIM) + 1;
	}
	else {
		beh_weak = E_BEH_EIO;
		PINF("No weak pages behavior defined, assuming eio\n");
	}
	if ((res = parse_page_env(env_weak_pages, E_PAGE_WEAK)) < 0) {
		PERR("Couldn't parse " ENV_WEAK_PAGES "!\n");
		goto err;
	}
	PALL(1, "SET weak pages:\t\t%d\n", res);
	// GRAVE PAGES
	if (0 == strncmp(env_grave_pages, PARSE_BEH_EIO, PARSE_BEH_LEN))
	{
		beh_grave = E_BEH_EIO;
		PALL(1, "SET grave behavior:\t%s\n", PARSE_BEH_EIO);
		env_grave_pages = strchr(env_grave_pages, PARSE_PREFIX_DELIM) + 1;
	}
	else if (0 == strncmp(env_grave_pages, PARSE_BEH_RND, PARSE_BEH_LEN))
	{
		beh_grave = E_BEH_RND;
		PALL(1, "SET grave behavior:\t%s\n", PARSE_BEH_RND);
		env_grave_pages = strchr(env_grave_pages, PARSE_PREFIX_DELIM) + 1;
	}
	else {
		beh_grave = E_BEH_EIO;
		PINF("No grave pages behavior defined, assuming eio\n");
	}
	if ((res = parse_page_env(env_grave_pages, E_PAGE_GRAVE)) < 0) {
		PERR("Couldn't parse " ENV_GRAVE_PAGES "!\n");
		goto err;
	}
	PALL(1, "SET grave pages:\t%d\n", res);

	// INITIAL REPORT
	PALL(0, "\nSUMMARY:\n");
	report_pages();

	// CHECK CACHE FILE
	if (access(cache_file, F_OK) < 0) {
		PERR("Couldn't access cache_file: %s\n", cache_file);
		goto err;
	}
	stat(cache_file, &st);
	cache_file_size = st.st_size;
	if (size != (unsigned long)cache_file_size) {
		PERR("Given flash size and cache_file sizes differs (%lu != %lu)\n", size, (unsigned long)cache_file_size);
		goto err;
	}

	// SIGNAL HANDLING
	signal(SIGUSR1, sig_handler_USR1);
	// INITIALIZING MTD INFO
	mtd_info.type = MTD_NORFLASH;
	mtd_info.flags = MTD_CAP_NORFLASH;
	mtd_info.size = size;
	mtd_info.erasesize = erase_size;
	mtd_info.writesize = 1;
	mtd_info.oobsize = 0;

	// END
	puts("Init OK!");
	return;

err:
	usage();

	if (page_info)
		FREE(page_info);
	if (-1 != cache_file_fd)
		close(cache_file_fd);

	puts("Init Failed!");
	exit(ERR_INIT_FAILED);
}

__attribute__((destructor)) void norsim_finish(void)
{
	puts("Closing norsim...");
	PALL(0, "Report:\n");
	report_pages();
	PALL(0, "Stats:\n");
	report_stats();

	if (page_info)
		FREE(page_info);
	if (-1 != cache_file_fd)
		close(cache_file_fd);


	puts("Closing OK!");
}

static void sig_handler_USR1(int signum)
{
	time_t cur_time = time(NULL);
	struct tm *date_info = localtime(&cur_time);

	if (SIGUSR1 == signum) {
		if (!initialized) {
			PALL(0, "Not initialized, no info available\n");
			return;
		} else {
			PALL(0, "\n");
			PALL(0, asctime(date_info));
			PALL(0, "Report:\n");
			report_pages();
			PALL(0, "Stats:\n");
			report_stats();
		}
	}
}

static void usage(void)
{
	printf("\nversion: %s\n", VERSION);
	puts("usage: ");
	puts("following environment variables can be defined to set library behabior:");
	puts("\t" ENV_LOGLEVEL    ":\t0 - SILENCE, 1 - ERRORS, 2 - INFO, 3 - DEBUG");
	puts("\t" ENV_CACHE_FILE  ":\tpath to file which will be used as storage");
	puts("\t" ENV_SIZE        ":\tsize of flash device (decimal number in kBytes)");
	puts("\t" ENV_ERASE_SIZE  ":\tsize of erase page (decimal number in kBytes");
	puts("\t" ENV_WEAK_PAGES  ":\tpages marked as weak (see format description)");
	puts("\t" ENV_GRAVE_PAGES ":\tpages marked as grave (see format description)");
	puts("\t" ENV_BIT_FLIPS   ":\tpages marked to allow bit-flips (see format description)");
	puts("");
	puts("format used by weak, grave pages and bit-flips:");
	puts("\t(<page_number>,<cycles>;)+");
	puts("example:");
	puts("\t1,3;1024,10; - page 1 and 1024 will be marked as weak with 3 and 10 cycles accordingly");
	puts("");
	puts("failure types:");
	puts("weak:     page will store random data after given amount of cycles");
	puts("grave:    page can't be read after given amount of cycles, random data will be read");
	puts("bit-flip: allow given amount bit-flips while reading page");
}

static void report_pages(void)
{
	pthread_mutex_lock(&mutex);
	for (unsigned i = 0; i < pages; ++i) {
		if (E_PAGE_NORMAL != page_info[i].type) {
			PALL(0, "Page\t%u:\t", i);
			switch (page_info[i].type) {
				case E_PAGE_WEAK:
					PALL(0, "W(cycles=%u, remaining=%u, reads=%lu, writes=%lu, erases=%lu)\n",
						page_info[i].cycles,
						page_info[i].cycles - page_info[i].current_cycles,
						page_info[i].reads,
						page_info[i].writes,
						page_info[i].erases
					);
					break;
				case E_PAGE_GRAVE:
					PALL(0, "G(cycles=%u, remaining=%u, reads=%lu, writes=%lu, erases=%lu)\n",
						page_info[i].cycles,
						page_info[i].cycles - page_info[i].current_cycles,
						page_info[i].reads,
						page_info[i].writes,
						page_info[i].erases
					);
					break;
				default:
					break;
			}
		}
	}
	pthread_mutex_unlock(&mutex);
}

static void report_stats(void)
{
	st_page_stats_t weak, grave;
	memset (&weak, 0x00, sizeof(weak));
	memset (&grave, 0x00, sizeof(grave));

	pthread_mutex_lock(&mutex);
	for (unsigned i = 0; i < pages; ++i) {
		if (E_PAGE_NORMAL != page_info[i].type) {
			switch (page_info[i].type) {
				case E_PAGE_WEAK:
					STATS_FILL(weak,min,reads); STATS_FILL(weak,max,reads);
					STATS_FILL(weak,min,writes); STATS_FILL(weak,max,writes);
					STATS_FILL(weak,min,erases); STATS_FILL(weak,max,erases);
					break;
				case E_PAGE_GRAVE:
					STATS_FILL(grave,min,reads); STATS_FILL(grave,max,reads);
					STATS_FILL(grave,min,writes); STATS_FILL(grave,max,writes);
					STATS_FILL(grave,min,erases); STATS_FILL(grave,max,erases);
					break;
				default:
					break;
			}
		}
	}
	pthread_mutex_unlock(&mutex);

	PALL(0, "WEAK pages:\n");
	PALL(0, "\tmin reads:  %lu\n", weak.min_reads);
	PALL(0, "\tmax reads:  %lu\n", weak.max_reads);
	PALL(0, "\tmin writes: %lu\n", weak.min_writes);
	PALL(0, "\tmax writes: %lu\n", weak.max_writes);
	PALL(0, "\tmin erases: %lu\n", weak.min_erases);
	PALL(0, "\tmin erases: %lu\n", weak.max_erases);
	PALL(0, "GRAVE pages:\n");
	PALL(0, "\tmin reads:  %lu\n", grave.min_reads);
	PALL(0, "\tmax reads:  %lu\n", grave.max_reads);
	PALL(0, "\tmin writes: %lu\n", grave.min_writes);
	PALL(0, "\tmax writes: %lu\n", grave.max_writes);
	PALL(0, "\tmin erases: %lu\n", grave.min_erases);
	PALL(0, "\tmin erases: %lu\n", grave.max_erases);
}

static void shutdown(void)
{
	exit(ERR_NOT_INITIALIZED);
}

static int parse_page_env(const char const *str, e_page_type_t type)
{
	const char *cur_node = str;
	char *cur_prop;
	char *end_node;
	char *end_prop;
	int count = -1;
	unsigned cycles;
	unsigned long page;

	if (0 == str[0])
		return (0);

	do {
		end_node = strchr(cur_node, PARSE_NODE_DELIM);
		if (NULL == end_node) break;
		end_prop = strchr(cur_node, PARSE_PROP_DELIM);
		if (NULL == end_prop) break;

		page = strtoul(cur_node, &end_prop, 10);
		cur_prop = ++end_prop;
		cycles = strtoul(cur_prop, NULL, 10);
		PDBG("page=%lu,\tcycles=%u\n", page, cycles);
		if (page > pages) {
			PERR("trying to set non existing page (page=%lu > pages=%lu)\n", page, pages);
			return -1;
		}
		page_info[page].type = type;
		page_info[page].cycles = cycles;

		if (-1 == count)
			count = 0;
		++count;
		cur_node = ++end_node;
	} while (NULL != cur_node);

	return (count);
}

inline static unsigned get_page_by_offset(off_t offset)
{
	return (offset/erase_size);
}

inline static e_page_type_t get_page_type_by_index(unsigned index)
{
	e_page_type_t type;

	pthread_mutex_lock(&mutex);
	type = page_info[index].type;
	pthread_mutex_unlock(&mutex);

	return (type);
}

/*
 * WRAPPERS
 */
// ---------------------------------------
int open(const char *path, int oflag, ...)
{
	char realpath_buf[PATH_MAX + 1];
	int errno_cpy;
	int ret = -1;
	va_list args;

	initialize(E_SYSCALL_OPEN);
	realpath(path, realpath_buf);
	va_start(args, oflag);
	if (0 != strcmp(realpath_buf, cache_file))
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
	shutdown();
	return (-1);
}

// --------------
int close(int fd)
{
	int errno_cpy;
	int ret = -1;

	initialize(E_SYSCALL_CLOSE);
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
	shutdown();
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

	initialize(E_SYSCALL_PREAD);
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

	initialize(E_SYSCALL_PWRITE);
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
	initialize(E_SYSCALL_READ);
	return (real_syscalls._read(fd, buf, count));
}

// -------------------------------------------------
ssize_t write(int fd, const void *buf, size_t count)
{
	// TODO: simplified version
	initialize(E_SYSCALL_WRITE);
	return (real_syscalls._write(fd, buf, count));
}

// -------------------------------
int ioctl(int fd, unsigned long request, ...)
{
	int ret = -1;
	va_list args;

	initialize(E_SYSCALL_IOCTL);
	va_start(args, request);
	if (fd != cache_file_fd)
		ret = real_syscalls._ioctl(fd, request, args);
	else {
		switch (request) {
			case MEMGETINFO:
				ret = 0;
				mtd_info_t *mi = va_arg(args, mtd_info_t*);
				memcpy(mi, &mtd_info, sizeof(mtd_info));
				PDBG("got MEMGETINFO request\n");
				break;
			case MEMUNLOCK:
			case MEMERASE:
				if (MEMUNLOCK == request)
					PDBG("got MEMUNLOCK request\n");
				else if (MEMERASE == request)
					PDBG("got MEMERASE request\n");

				erase_info_t *ei = va_arg(args, erase_info_t*);
				if ((0 != (ei->start % erase_size) || 0 != (ei->length % erase_size))) {
					ret = -1;
					PINF("erase_info_t invalid, start=0x%04lx, length=0x%04lx\n", (unsigned long)ei->start, (unsigned long)ei->length);
				}
				else
					ret = 0;
			default:
				break;
		}
	}
	va_end(args);

	return (ret);
}
