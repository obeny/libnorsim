// TODO:
// net-socket comm
// implement read, write (any others?)
// real failures (masks)
// refactoring

#include <cstdio>
#include <cstring>

#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#include <mtd/mtd-user.h>

#include "Libnorsim.h"
#include "LogFormatterLibnorsim.h"

#define STATS_FILL(t,a,op) t.a##_##op = get_##a(t.a##_##op,m_pageInfo.get()[i].op)

extern "C" void sig_handler(int signum);
extern "C" volatile sig_atomic_t report_requested;

unsigned long get_min(unsigned long g, unsigned long p);
unsigned long get_max(unsigned long g, unsigned long p);

__attribute__((constructor)) void libnorsim_constructor()
{
	printf("libnorsim, version: %s initialized\n", VERSION);
	puts("waiting for \"open\" syscall to start...");
	fflush(stdout);
}

__attribute__((destructor)) void libnorsim_destructor()
{
	puts("closing libnorsim");
	fflush(stdout);
}

Libnorsim::Libnorsim() 
	: m_initialized(false), m_opened(false), m_cacheFileFd(-1) {
	report_requested = 0;

	initLogger();
	if (!initSyscallsCache())
		goto err;
	if (!initCacheFile())
		goto err;
	if (!initSizes())
		goto err;
	if (!initPages())
		goto err;
	if (!initEraseBuffer())
		goto err;

	initWeakPages();
	initGravePages();

	if ((!m_weakPages) && (!m_gravePages))
		m_logger->log(Loglevel::WARNING, "No failures defined, faults won't be forwarded to user program");

	m_logger->log(Loglevel::ALWAYS, "SUMMARY:");
	printPageReport();

	initMtdInfo();
	signal(SIGUSR1, sig_handler);
	signal(SIGUSR2, sig_handler);

	m_logger->log(Loglevel::DEBUG, "Libnorsim init OK");
	m_initialized = true;
	return;

err:
	printUsage();
	m_logger->log(Loglevel::DEBUG, "Libnorsim init FAILED!");
	exit(-1);
}

Libnorsim::~Libnorsim() {
	m_logger->log(Loglevel::ALWAYS, "Page report:");
	printPageReport(true);
	m_logger->log(Loglevel::ALWAYS, "Statistics:");
	printPageStatistics();
}

void Libnorsim::handleReportRequest() {
	if (report_requested) {
		if (!m_initialized) {
			m_logger->log(Loglevel::WARNING, "Not initialized, no report available");
		} else {
			time_t cur_time = time(NULL);
			struct tm *date_info = localtime(&cur_time);

			m_logger->log(Loglevel::ALWAYS, asctime(date_info), true);
			m_logger->log(Loglevel::ALWAYS, "Page report:");
			switch (report_requested) {
				case SIGNAL_REPORT_SHORT: printPageReport(false); break;
				case SIGNAL_REPORT_DETAILED: printPageReport(true); break;
				default: break;
			}

			m_logger->log(Loglevel::ALWAYS, "Statistics:");
			printPageStatistics();
		}
	}
	report_requested = 0;
}

void Libnorsim::initLogger() {
	char *env_log = getenv(ENV_LOG);
	if (!env_log){
		puts(ENV_LOG " not defined, assiming STDIO for logging...");
		m_logFormatter.reset(new LogFormatterLibnorsim(true));
		m_logger.reset(LoggerFactory::createLoggerStdio(m_logFormatter.get()));
	} else if (0 == strcmp(env_log, "stdio")) {
		puts("using STDIO for logging...");
		m_logFormatter.reset(new LogFormatterLibnorsim(true));
		m_logger.reset(LoggerFactory::createLoggerStdio(m_logFormatter.get()));
	} else {
		m_logFormatter.reset(new LogFormatterLibnorsim(false));
		m_logger.reset(LoggerFactory::createLoggerFile(m_logFormatter.get(), env_log));
		if (m_logger->isOk())
			printf("using %s file for logging...\n", env_log);
		else {
			printf("path %s is not valid, refusing logging to file, falling back to STDIO logging...\n", env_log);
			m_logFormatter.release();
			m_logFormatter.reset(new LogFormatterLibnorsim(true));
			m_logger.release();
			m_logger.reset(LoggerFactory::createLoggerStdio(m_logFormatter.get()));
		}
	}
	char *env_loglevel = getenv(ENV_LOGLEVEL);
	if (!env_loglevel) {
		puts("no loglevel given, assuming INFO level...");
		m_logger->setVerbosity(Loglevel::INFO);
	} else {
		unsigned long loglevel_set = strtoul(env_loglevel, NULL, 10);
		unsigned long loglevel_max = static_cast<unsigned long>(Loglevel::DEBUG);
		loglevel_set = (loglevel_set > loglevel_max)?(loglevel_max):(loglevel_set);
		Loglevel ll = static_cast<Loglevel>(loglevel_set);
		m_logger->setVerbosity(ll);
		printf("loglevel set to %d\n", static_cast<int>(ll));
	}
	m_logger->log(Loglevel::ALWAYS, "Logger ready!");
}

bool Libnorsim::initSyscallsCache() {
	m_syscallsCache.reset(new SyscallsCache(this));
	return (m_syscallsCache->isOk()); 
}

bool Libnorsim::initCacheFile() {
	char *env_cache_file = getenv(ENV_CACHE_FILE);
	if (!env_cache_file) {
		m_logger->log(Loglevel::FATAL, "No cache_file given");
		return (false);
	}
	m_cacheFile.reset(realpath(env_cache_file, NULL));
	if (!m_cacheFile) {
		m_logger->log(Loglevel::FATAL, "File \"%s\" (" ENV_CACHE_FILE ") do not exist", false, env_cache_file);
		return (false);
	}
	m_logger->log(Loglevel::INFO, "Set cache file: %s", false, m_cacheFile.get());
	
	if (access(m_cacheFile.get(), F_OK) < 0) {
		m_logger->log(Loglevel::FATAL, "Couldn't access file \"%s\"", false, m_cacheFile.get());
		return(false);
	}
	return (true);
}

bool Libnorsim::initSizes() {
	char *env_size = getenv(ENV_SIZE);
	if (!env_size) {
		m_logger->log(Loglevel::FATAL, "No size given");
		return (false);
	}
	m_size = static_cast<unsigned>(strtoul(env_size, NULL, 10) * 1024);
	m_logger->log(Loglevel::INFO, "Set size: 0x%lX (%lukB)", false, m_size, m_size / 1024);

	struct stat st;
	stat(m_cacheFile.get(), &st);
	unsigned long cache_file_size = st.st_size;
	if (m_size != cache_file_size) {
		m_logger->log(Loglevel::FATAL, "Given flash size and cache_file sizes differs (%lukB != %lukB)",
			false, m_size / 1024, cache_file_size / 1024);
		return (false);
	}

	char *env_erase_size = getenv(ENV_ERASE_SIZE);
	if (!env_erase_size) {
		m_logger->log(Loglevel::FATAL, "No erase_size given");
		return (false);
	}
	m_eraseSize = (unsigned)strtoul(env_erase_size, NULL, 10) * 1024;
	m_logger->log(Loglevel::INFO, "Set erase_size: 0x%lX (%lukB)", false, m_eraseSize, m_eraseSize / 1024);

	return (true);
}

bool Libnorsim::initPages() {
	m_pages = m_size / m_eraseSize;
	m_logger->log(Loglevel::INFO, "Set page count: %lu", false, m_pages);
	m_pageInfo.reset(new st_page_t[m_pages]);
	if (!m_pageInfo) {
		m_logger->log(Loglevel::FATAL, "Couldn't allocate memory for page information");
		return (false);
	}
	memset(m_pageInfo.get(), 0x00, sizeof(st_page_t) * m_pages);
	return (true);
}

bool Libnorsim::initEraseBuffer() {
	m_eraseBuffer.reset(new char[m_eraseSize]);
	return (NULL != m_eraseBuffer.get());
}

void Libnorsim::initWeakPages() {
	char *env_weak_pages = getenv(ENV_WEAK_PAGES);
	if (!env_weak_pages) {
		m_logger->log(Loglevel::WARNING, "No weak_pages given, assuming no weak pages");
		return;
	}

	m_weakPages = parsePageType(env_weak_pages, "weak", &m_behaviorWeak, E_PAGE_WEAK);
}

void Libnorsim::initGravePages() {
	char *env_grave_pages = getenv(ENV_GRAVE_PAGES);
	if (!env_grave_pages) {
		m_logger->log(Loglevel::WARNING, "No grave_pages given, assuming no grave pages");
		return;
	}

	m_gravePages = parsePageType(env_grave_pages, "grave", &m_behaviorGrave, E_PAGE_GRAVE);
}

void Libnorsim::initMtdInfo() {
	memset(&m_mtdInfo, 0x00, sizeof(mtd_info_t));
	m_mtdInfo.type = MTD_NORFLASH;
	m_mtdInfo.flags = MTD_CAP_NORFLASH;
	m_mtdInfo.size = m_size;
	m_mtdInfo.erasesize = m_eraseSize;
	m_mtdInfo.writesize = 1;
	m_mtdInfo.oobsize = 0;
}

void Libnorsim::printUsage() {
	printf("\nversion: %s\n", VERSION);
	puts("usage: ");
	puts("following environment variables can be defined to set library behavior:");
	puts("\t" ENV_LOGLEVEL    ":\t0 - SILENCE, 1 - ERRORS, 2 - INFO, 3 - DEBUG");
	puts("\t" ENV_LOG         ":\tstdio - log to console, <filepath> - log to file");
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

void Libnorsim::printPageReport(bool detailed)
{
	std::lock_guard<std::mutex> lg(m_mutex);

	for (unsigned i = 0; i < m_pages; ++i) {
		switch (m_pageInfo.get()[i].type) {
			case E_PAGE_NORMAL:
				if (detailed && (m_pageInfo.get()[i].reads || m_pageInfo.get()[i].writes || m_pageInfo.get()[i].erases)) {
					m_logger->log(Loglevel::ALWAYS, "\tPage %5u: N(reads=%lu, writes=%lu, erases=%lu)", false,
						i,
						m_pageInfo.get()[i].reads,
						m_pageInfo.get()[i].writes,
						m_pageInfo.get()[i].erases
					);
				}
				break;
			case E_PAGE_WEAK:
				m_logger->log(Loglevel::ALWAYS, "\tPage %5u: W(cycles=%u, remaining=%u, reads=%lu, writes=%lu, erases=%lu)", false,
					i,
					m_pageInfo.get()[i].cycles,
					m_pageInfo.get()[i].cycles - m_pageInfo.get()[i].current_cycles,
					m_pageInfo.get()[i].reads,
					m_pageInfo.get()[i].writes,
					m_pageInfo.get()[i].erases
				);
				break;
			case E_PAGE_GRAVE:
				m_logger->log(Loglevel::ALWAYS, "\tPage %5u: G(cycles=%u, remaining=%u, reads=%lu, writes=%lu, erases=%lu)", false,
					i,
					m_pageInfo.get()[i].cycles,
					m_pageInfo.get()[i].cycles - m_pageInfo.get()[i].current_cycles,
					m_pageInfo.get()[i].reads,
					m_pageInfo.get()[i].writes,
					m_pageInfo.get()[i].erases
				);
				break;
			default:
				break;
		}
	}
}

void Libnorsim::printPageStatistics() {
	st_page_stats_t normal, weak, grave;
	memset (&normal, 0x00, sizeof(normal));
	memset (&weak, 0x00, sizeof(weak));
	memset (&grave, 0x00, sizeof(grave));

	m_mutex.lock();
	for (unsigned i = 0; i < m_pages; ++i) {
		switch (m_pageInfo.get()[i].type) {
			case E_PAGE_NORMAL:
				STATS_FILL(normal,max,reads); STATS_FILL(normal,max,writes); STATS_FILL(normal,max,erases);
				break;
			case E_PAGE_WEAK:
				STATS_FILL(weak,max,reads); STATS_FILL(weak,max,writes); STATS_FILL(weak,max,erases);
				break;
			case E_PAGE_GRAVE:
				STATS_FILL(grave,max,reads); STATS_FILL(grave,max,writes); STATS_FILL(grave,max,erases);
				break;
			default:
				break;
		}
	}
	normal.min_reads = normal.max_reads; normal.min_writes = normal.max_writes; normal.min_erases = normal.max_erases;
	weak.min_reads = weak.max_reads; weak.min_writes = weak.max_writes; weak.min_erases = weak.max_erases;
	grave.min_reads = grave.max_reads; grave.min_writes = grave.max_writes; grave.min_erases = grave.max_erases;
	for (unsigned i = 0; i < m_pages; ++i) {
		switch (m_pageInfo.get()[i].type) {
			case E_PAGE_NORMAL:
				STATS_FILL(normal,min,reads); STATS_FILL(normal,min,writes); STATS_FILL(normal,min,erases);
				break;
			case E_PAGE_WEAK:
				STATS_FILL(weak,min,reads); STATS_FILL(weak,min,writes); STATS_FILL(weak,min,erases);
				break;
			case E_PAGE_GRAVE:
				STATS_FILL(grave,min,reads); STATS_FILL(grave,min,writes); STATS_FILL(grave,min,erases);
				break;
			default:
				break;
		}
	}
	m_mutex.unlock();

	m_logger->log(Loglevel::ALWAYS, "\tNORMAL pages:");
	m_logger->log(Loglevel::ALWAYS, "\t\tmin reads:  %lu", false, normal.min_reads);
	m_logger->log(Loglevel::ALWAYS, "\t\tmax reads:  %lu", false, normal.max_reads);
	m_logger->log(Loglevel::ALWAYS, "\t\tmin writes: %lu", false, normal.min_writes);
	m_logger->log(Loglevel::ALWAYS, "\t\tmax writes: %lu", false, normal.max_writes);
	m_logger->log(Loglevel::ALWAYS, "\t\tmin erases: %lu", false, normal.min_erases);
	m_logger->log(Loglevel::ALWAYS, "\t\tmax erases: %lu", false, normal.max_erases);

	m_logger->log(Loglevel::ALWAYS, "\tWEAK pages:");
	m_logger->log(Loglevel::ALWAYS, "\t\tmin reads:  %lu", false, weak.min_reads);
	m_logger->log(Loglevel::ALWAYS, "\t\tmax reads:  %lu", false, weak.max_reads);
	m_logger->log(Loglevel::ALWAYS, "\t\tmin writes: %lu", false, weak.min_writes);
	m_logger->log(Loglevel::ALWAYS, "\t\tmax writes: %lu", false, weak.max_writes);
	m_logger->log(Loglevel::ALWAYS, "\t\tmin erases: %lu", false, weak.min_erases);
	m_logger->log(Loglevel::ALWAYS, "\t\tmax erases: %lu", false, weak.max_erases);

	m_logger->log(Loglevel::ALWAYS, "\tGRAVE pages:");
	m_logger->log(Loglevel::ALWAYS, "\t\tmin reads:  %lu", false, grave.min_reads);
	m_logger->log(Loglevel::ALWAYS, "\t\tmax reads:  %lu", false, grave.max_reads);
	m_logger->log(Loglevel::ALWAYS, "\t\tmin writes: %lu", false, grave.min_writes);
	m_logger->log(Loglevel::ALWAYS, "\t\tmax writes: %lu", false, grave.max_writes);
	m_logger->log(Loglevel::ALWAYS, "\t\tmin erases: %lu", false, grave.min_erases);
	m_logger->log(Loglevel::ALWAYS, "\t\tmax erases: %lu", false, grave.max_erases);
}

int Libnorsim::parsePageType(char* env, const char* const name,	e_beh_t* const beh, const e_page_type_t type) {
	int res = 0;

	if (0 == strncmp(env, PARSE_BEH_EIO, PARSE_BEH_LEN)) {
		*beh = E_BEH_EIO;
		m_logger->log(Loglevel::INFO, "Set \"%s pages\" behavior: %s", false, name, PARSE_BEH_EIO);
		env = strchr(env, PARSE_PREFIX_DELIM) + 1;
	}
	else if (0 == strncmp(env, PARSE_BEH_RND, PARSE_BEH_LEN)) {
		*beh = E_BEH_RND;
		m_logger->log(Loglevel::INFO, "Set \"%s pages\" behavior: %s", false, name, PARSE_BEH_RND);
		env = strchr(env, PARSE_PREFIX_DELIM) + 1;
	} else {
		*beh = E_BEH_EIO;
		m_logger->log(Loglevel::INFO, "No \"%s pages\" behavior defined, assuming \"eio\"", false, name);
	}

	if ((res = parsePageEnv(env, type)) < 0) {
		m_logger->log(Loglevel::INFO, "Couldn't parse: \"%s\"", false, env);
		return (res);
	}
	m_logger->log(Loglevel::INFO, "Set \"%s pages\": %d", false, name, res);
	return (res);
}

int Libnorsim::parsePageEnv(const char * const str, e_page_type_t type) {
	const char *cur_node = str;
	char *cur_prop;
	char *end_node;
	char *end_prop;
	int count = -1;
	unsigned cycles;
	unsigned long page;

	if (0 == str[0])
		return (0);

	char type_sign = (type == E_PAGE_WEAK)?('W'):('G');
	do {
		end_node = (char*)(strchr(cur_node, PARSE_NODE_DELIM));
		if (NULL == end_node) break;
		end_prop = (char*)strchr(cur_node, PARSE_PROP_DELIM);
		if (NULL == end_prop) break;

		page = strtoul(cur_node, &end_prop, 10);
		cur_prop = ++end_prop;
		cycles = strtoul(cur_prop, NULL, 10);
		m_logger->log(Loglevel::DEBUG, "\t(%c)\tpage=%lu\tcycles=%u", false,
			type_sign, page, cycles
		);
		if (page > m_pages) {
			m_logger->log(Loglevel::ERROR, "\t(%c)\ttrying to set non existing page (page=%lu > pages=%lu)", false,
				type_sign, page, m_pages
			);
			return -1;
		}
		m_pageInfo.get()[page].type = type;
		m_pageInfo.get()[page].cycles = cycles;

		if (-1 == count)
			count = 0;
		++count;
		cur_node = ++end_node;
	} while (NULL != cur_node);

	return (count);
}
