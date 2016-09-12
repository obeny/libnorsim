#ifndef __LIBNORSIM_H__
#define __LIBNORSIM_H__

#define ENV_LOGLEVEL    "NS_LOGLEVEL"
#define ENV_CACHE_FILE  "NS_CACHE_FILE"
#define ENV_SIZE        "NS_SIZE"
#define ENV_ERASE_SIZE  "NS_ERASE_SIZE"
#define ENV_WEAK_PAGES  "NS_WEAK_PAGES"
#define ENV_GRAVE_PAGES "NS_GRAVE_PAGES"
#define ENV_BIT_FLIPS   "NS_BIT_FLIPS"

#define ERR_INIT_FAILED     (-1)
#define ERR_NOT_INITIALIZED (-2)

#define PARSE_NODE_DELIM   ';'
#define PARSE_PROP_DELIM   ','
#define PARSE_PREFIX_DELIM ' '

#define PARSE_BEH_EIO "eio"
#define PARSE_BEH_RND "rnd"
#define PARSE_BEH_LEN 3

#define FREE(ptr) \
do { \
 if (ptr) { free(ptr); ptr = NULL; } \
} while (0)

#define PALL(func, ...) \
do { \
	if (func) \
		printf("%s(): ", __FUNCTION__); \
	printf(__VA_ARGS__); \
} while (0)

#define PDBG(...) \
do { \
	if (loglevel >= E_LOGLEVEL_DEBUG) { \
 		printf("DBG %s(): ", __FUNCTION__); \
		printf(__VA_ARGS__); \
	} \
} while (0)

#define PERR(...) \
do { \
	if (loglevel >= E_LOGLEVEL_ERROR) { \
 		printf("ERR %s(): ", __FUNCTION__); \
		printf(__VA_ARGS__); \
	} \
} while (0)

#define PINF(...) \
do { \
	if (loglevel >= E_LOGLEVEL_INFO) { \
 		printf("INF %s(): ", __FUNCTION__); \
		printf(__VA_ARGS__); \
	} \
} while (0)

#define get_min(a,b) ((b<a)?b:a)
#define get_max(a,b) ((b>a)?b:a)

#define STATS_FILL(t,a,op) t.a##_##op = get_##a(t.a##_##op,page_info[i].op)

typedef int (*open_ptr_t)(const char *path, int oflag, ...);
typedef int (*close_ptr_t)(int fd);

typedef ssize_t (*pread_ptr_t)(int fd, void *buf, size_t count, off_t offset);
typedef ssize_t (*pwrite_ptr_t)(int fd, const void *buf, size_t count, off_t offset);
typedef ssize_t (*read_ptr_t)(int fd, void *buf, size_t count);
typedef ssize_t (*write_ptr_t)(int fd, const void *buf, size_t count);

typedef int (*ioctl_ptr_t)(int fd, unsigned long request, ...);

typedef enum
{
	E_BEH_EIO = 0,
	E_BEH_RND
} e_beh_t;

typedef enum
{
	E_LOGLEVEL_ERROR = 1,
	E_LOGLEVEL_INFO = 2,
	E_LOGLEVEL_DEBUG = 3
} e_loglevel_t;

typedef enum
{
	E_PAGE_NORMAL = 0,
	E_PAGE_WEAK,
	E_PAGE_GRAVE,
	E_PAGE_BIT_FLIP
} e_page_type_t;

typedef enum
{
	E_SYSCALL_OPEN,
	E_SYSCALL_CLOSE,
	E_SYSCALL_PREAD,
	E_SYSCALL_PWRITE,
	E_SYSCALL_READ,
	E_SYSCALL_WRITE,
	E_SYSCALL_IOCTL
} e_syscall_t;

typedef struct
{
	e_page_type_t type;
	unsigned short cycles;
	unsigned short current_cycles;
	unsigned long reads;
	unsigned long writes;
	unsigned long erases;
} st_page_t;

typedef struct
{
	unsigned long min_reads;
	unsigned long max_reads;
	unsigned long min_writes;
	unsigned long max_writes;
	unsigned long min_erases;
	unsigned long max_erases;
} st_page_stats_t;

typedef struct {
	open_ptr_t _open;
	close_ptr_t _close;
	pread_ptr_t _pread;
	pwrite_ptr_t _pwrite;
	read_ptr_t _read;
	write_ptr_t _write;
	ioctl_ptr_t _ioctl;
} st_syscalls_t;

#endif // __LIBNORSIM_H__
