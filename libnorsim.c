// TODO:
// cleanup init
// improve exit
// implement read, write (any others?)
// real failures (masks)
// refactoring

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include <dlfcn.h>
#include <limits.h>
#include <signal.h>

#include <sys/stat.h>

#include <mtd/mtd-user.h>

#include "libnorsim.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
mtd_info_t mtd_info;
st_page_t *page_info = NULL;
st_syscalls_t real_syscalls;
e_beh_t beh_weak;
e_beh_t beh_grave;

char *cache_file = NULL;
char *erase_buffer = NULL;
int initialized = 0;
int cache_file_fd = -1;
unsigned loglevel = E_LOGLEVEL_INFO;
unsigned long erase_size;

static char cache_file_realpath[PATH_MAX + 1];

static unsigned long pages;
static unsigned long size;

static int init_done;
static int report_requested;

static unsigned long cache_file_size;

static void norsim_init(void);
static void sig_handler_USR1(int signum);
static void usage(void);
static void report_pages(int detailed);
static void report_stats(void);
static int parse_page_env(const char * const str, e_page_type_t type);
static int parse_page_env_type(char* env_val, const char* const env_name,
	const char* const name, e_beh_t * const beh, const e_page_type_t type);
static void mtd_info_init(mtd_info_t *info);

void initialize(const e_syscall_t syscall)
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

__attribute__((noreturn)) void shutdown(void)
{
	exit(ERR_NOT_INITIALIZED);
}

static void norsim_init(void)
{
	char *env_loglevel;
	char *env_size;
	char *env_erase_size;

	char *env_weak_pages;
	char *env_grave_pages;

	struct stat st;

	puts("Initializing norsim...");
	memset(&real_syscalls, 0x00, sizeof(real_syscalls));
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
	if (NULL != realpath(cache_file, cache_file_realpath))
		cache_file = cache_file_realpath;
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
	}
	// GRAVE PAGES
	if (NULL == (env_grave_pages = getenv(ENV_GRAVE_PAGES))) {
		PINF("No grave_pages given, assuming no grave pages\n");
	}
	// NO FAULTS
	if (!env_weak_pages && !env_grave_pages)
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
	if (parse_page_env_type(env_weak_pages, ENV_WEAK_PAGES, "weak", &beh_weak, E_PAGE_WEAK) < 0)
		goto err;
	// GRAVE PAGES
	if (parse_page_env_type(env_grave_pages, ENV_GRAVE_PAGES, "grave", &beh_grave, E_PAGE_GRAVE) < 0)
		goto err;

	// INITIAL REPORT
	PALL(0, "\nSUMMARY:\n");
	report_pages(0);

	// CHECK CACHE FILE
	if (access(cache_file, F_OK) < 0) {
		PERR("Couldn't access cache_file: %s\n", cache_file);
		goto err;
	}
	stat(cache_file, &st);
	cache_file_size = st.st_size;
	if (size != cache_file_size) {
		PERR("Given flash size and cache_file sizes differs (%lu != %lu)\n", size, cache_file_size);
		goto err;
	}

	// SIGNAL HANDLING
	signal(SIGUSR1, sig_handler_USR1);
	// INITIALIZING MTD INFO
	mtd_info_init(&mtd_info);
	// INITIALIZE PAGE BUFFER
	erase_buffer = (char*)malloc(erase_size);
	memset(erase_buffer, 0xFF, erase_size);

	// END
	puts("Init OK!");
	return;

err:
	usage();
	puts("Init Failed!");
	exit(ERR_INIT_FAILED);
}

__attribute__((destructor)) void norsim_finish(void)
{
	puts("Closing norsim...");
	PALL(0, "Report:\n");
	report_pages(1);
	PALL(0, "Stats:\n");
	report_stats();

	if (erase_buffer)
		FREE(erase_buffer);
	if (page_info)
		FREE(page_info);
	if (-1 != cache_file_fd)
		close(cache_file_fd);

	puts("Closing OK!");
}

static void sig_handler_USR1(int signum)
{
	if (SIGUSR1 == signum)
		report_requested = 1;
}

static void usage(void)
{
	printf("\nversion: %s\n", VERSION);
	puts("usage: ");
	puts("following environment variables can be defined to set library behavior:");
	puts("\t" ENV_LOGLEVEL    ":\t0 - SILENCE, 1 - ERRORS, 2 - INFO, 3 - DEBUG");
	puts("\t" ENV_CACHE_FILE  ":\tpath to file which will be used as storage");
	puts("\t" ENV_SIZE        ":\tsize of flash device (decimal number in kBytes)");
	puts("\t" ENV_ERASE_SIZE  ":\tsize of erase page (decimal number in kBytes");
	puts("\t" ENV_WEAK_PAGES  ":\tpages marked as weak (see format description)");
	puts("\t" ENV_GRAVE_PAGES ":\tpages marked as grave (see format description)");
	puts("");
	puts("format used by weak and grave pages:");
	puts("\t(([rnd|eio])? <page_number>,<cycles>;)+");
	puts("example:");
	puts("\teio 1,3;1024,10; - page 1 and 1024 will be marked weak with 3 and 10 cycles accordingly and operations will result in \"eio\" error");
	puts("");
	puts("failure types:");
	puts("\teio:   page operation return -1");
	puts("\trnd:   read/write will return/store randomized data");
	puts("");
	puts("failure types:");
	puts("\tweak:  page will start failing during write operations after given amount of cycles");
	puts("\tgrave: page will start failing during read operations after given amount of cycles");
}

void report(void)
{
	time_t cur_time = time(NULL);
	struct tm *date_info = localtime(&cur_time);

	if (report_requested) {
		if (!initialized) {
			PALL(0, "Not initialized, no info available\n");
			return;
		} else {
			PALL(0, "\n");
			PALL(0, "%s", asctime(date_info));
			PALL(0, "Report:\n");
			report_pages(1);
			PALL(0, "Stats:\n");
			report_stats();
		}
	}
	report_requested = 0;
}

static void report_pages(int detailed)
{
	pthread_mutex_lock(&mutex);
	for (unsigned i = 0; i < pages; ++i) {
		switch (page_info[i].type) {
			case E_PAGE_NORMAL:
				if (detailed && (page_info[i].reads || page_info[i].writes || page_info[i].erases)) {
					PALL(0, "Page %5u: N(reads=%lu, writes=%lu, erases=%lu)\n",
						i,
						page_info[i].reads,
						page_info[i].writes,
						page_info[i].erases
					);
				}
				break;
			case E_PAGE_WEAK:
				PALL(0, "Page %5u: W(cycles=%u, remaining=%u, reads=%lu, writes=%lu, erases=%lu)\n",
					i,
					page_info[i].cycles,
					page_info[i].cycles - page_info[i].current_cycles,
					page_info[i].reads,
					page_info[i].writes,
					page_info[i].erases
				);
				break;
			case E_PAGE_GRAVE:
				PALL(0, "Page %5u: G(cycles=%u, remaining=%u, reads=%lu, writes=%lu, erases=%lu)\n",
					i,
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
	pthread_mutex_unlock(&mutex);
}

static void report_stats(void)
{
	st_page_stats_t normal, weak, grave;
	memset (&normal, 0x00, sizeof(normal));
	memset (&weak, 0x00, sizeof(weak));
	memset (&grave, 0x00, sizeof(grave));

	pthread_mutex_lock(&mutex);
	for (unsigned i = 0; i < pages; ++i) {
		switch (page_info[i].type) {
			case E_PAGE_NORMAL:
				STATS_FILL(normal,min,reads); STATS_FILL(normal,max,reads);
				STATS_FILL(normal,min,writes); STATS_FILL(normal,max,writes);
				STATS_FILL(normal,min,erases); STATS_FILL(normal,max,erases);
				break;
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
	pthread_mutex_unlock(&mutex);

	PALL(0, "NORMAL pages:\n");
	PALL(0, "\tmin reads:  %lu\n", normal.min_reads);
	PALL(0, "\tmax reads:  %lu\n", normal.max_reads);
	PALL(0, "\tmin writes: %lu\n", normal.min_writes);
	PALL(0, "\tmax writes: %lu\n", normal.max_writes);
	PALL(0, "\tmin erases: %lu\n", normal.min_erases);
	PALL(0, "\tmin erases: %lu\n", normal.max_erases);
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

static int parse_page_env(const char * const str, e_page_type_t type)
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

static int parse_page_env_type(char* env_val, const char* const env_name,
	const char* const name, e_beh_t * const beh, const e_page_type_t type)
{
	int res = 0;

	if (NULL != env_val) {
		if (0 == strncmp(env_val, PARSE_BEH_EIO, PARSE_BEH_LEN))
		{
			*beh = E_BEH_EIO;
			PALL(1, "SET %s behavior:\t%s\n", name, PARSE_BEH_EIO);
			env_val = strchr(env_val, PARSE_PREFIX_DELIM) + 1;
		}
		else if (0 == strncmp(env_val, PARSE_BEH_RND, PARSE_BEH_LEN))
		{
			*beh = E_BEH_RND;
			PALL(1, "SET %s behavior:\t%s\n", name, PARSE_BEH_RND);
			env_val = strchr(env_val, PARSE_PREFIX_DELIM) + 1;
		}
		else {
			*beh = E_BEH_EIO;
			PINF("No %s pages behavior defined, assuming eio\n", name);
		}
		if ((res = parse_page_env(env_val, type)) < 0) {
			PERR("Couldn't parse %s!\n", env_name);
		}
	}
	PALL(1, "SET %s pages:\t\t%d\n", name, res);
	return (res);
}

static void mtd_info_init(mtd_info_t *info)
{
	memset(info, 0x00, sizeof(mtd_info_t));
	info->type = MTD_NORFLASH;
	info->flags = MTD_CAP_NORFLASH;
	info->size = size;
	info->erasesize = erase_size;
	info->writesize = 1;
	info->oobsize = 0;
}

unsigned get_page_by_offset(off_t offset)
{
	return (offset/erase_size);
}

e_page_type_t get_page_type_by_index(unsigned index)
{
	e_page_type_t type;

	pthread_mutex_lock(&mutex);
	type = page_info[index].type;
	pthread_mutex_unlock(&mutex);

	return (type);
}
