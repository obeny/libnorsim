// TODO:
// remove hardcoded bitflip limit
// net-socket comm
// implement read, write (any others?)
// refactoring

#include <cstdio>
#include <cstring>

#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#include <mtd/mtd-user.h>

#include "Libnorsim.h"
#include "LogFormatterLibnorsim.h"

#define STATS_FILL(t,a,op) t.a##_##op = get_##a(t.a##_##op,m_pageManager->getPage(i).op)

extern "C" void sig_handler(int signum);
extern "C" volatile sig_atomic_t report_requested;

unsigned long get_min(unsigned long g, unsigned long p);
unsigned long get_max(unsigned long g, unsigned long p);

__attribute__((constructor)) void libnorsim_constructor()
{
	printf("libnorsim, version: %s loaded\n", VERSION);
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
	if (!initPageBuffer())
		goto err;

	try {
		m_pageManager.reset(new PageManager(*this, m_size / m_eraseSize));
	} catch (std::exception &e) {
		m_logger->log(Loglevel::FATAL, "%s", false, e.what());
		goto err;
	}
	initPageFailures();

	if ((!m_pageManager->getWeakPageCount()) && (!m_pageManager->getGravePageCount()))
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
	try {
		m_syscallsCache.reset(new SyscallsCache());
	} catch (std::exception &e) {
		m_logger->log(Loglevel::FATAL, "Couldn't initialize \"%s\" syscall", false, e.what());
		return (false);
	}
	if (NULL == m_syscallsCache.get()) {
		m_logger->log(Loglevel::FATAL, "SyscallsCache init FAILED!");
		return (false);
	}
	m_logger->log(Loglevel::DEBUG, "SyscallsCache init OK");
	return (true);
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
		m_logger->log(Loglevel::FATAL, "Given flash size and cache file sizes differs (%lukB != %lukB)",
			false, m_size / 1024, cache_file_size / 1024);
		return (false);
	}

	char *env_erase_size = getenv(ENV_ERASE_SIZE);
	if (!env_erase_size) {
		m_logger->log(Loglevel::FATAL, "No erase_size given");
		return (false);
	}
	m_eraseSize = (unsigned)strtoul(env_erase_size, NULL, 10) * 1024;
	m_logger->log(Loglevel::INFO, "Set erase size: 0x%lX (%lukB)", false, m_eraseSize, m_eraseSize / 1024);

	return (true);
}

bool Libnorsim::initPageBuffer() {
	m_pageBuffer.reset(new char[m_eraseSize]);
	if (NULL == m_pageBuffer.get())
		return (false);
	return (true);
}

void Libnorsim::initPageFailures() {
	char *env_weak_pages = getenv(ENV_WEAK_PAGES);
	if (env_weak_pages)
		m_pageManager->parseWeakPagesEnv(env_weak_pages);
	else
		m_logger->log(Loglevel::WARNING, "No weak pages environment given, assuming no weak pages");

	char *env_grave_pages = getenv(ENV_GRAVE_PAGES);
	if (env_grave_pages)
		m_pageManager->parseGravePagesEnv(env_grave_pages);
	else
		m_logger->log(Loglevel::WARNING, "No grave pages environment given, assuming no grave pages");
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

	long remaining;
	for (unsigned i = 0; i < m_pageManager->getPageCount(); ++i) {
		st_page_t page = m_pageManager->getPage(i);
		switch (m_pageManager->getPage(i).type) {
			case E_PAGE_NORMAL:
				if (detailed && (page.reads || page.writes || page.erases)) {
					m_logger->log(Loglevel::ALWAYS, "\tPage %5u: N(reads=%lu, writes=%lu, erases=%lu)", false,
						i, page.reads, page.writes, page.erases
					);
				}
				break;
			case E_PAGE_WEAK:
				remaining = page.limit - page.erases;
				m_logger->log(Loglevel::ALWAYS, "\tPage %5u: W(limit=%u, remaining=%u, reads=%lu, writes=%lu, erases=%lu)", false,
					i,
					page.limit, (remaining > 0)?(remaining):(0),
					page.reads, page.writes, page.erases
				);
				break;
			case E_PAGE_GRAVE:
				remaining = page.limit - page.erases;
				m_logger->log(Loglevel::ALWAYS, "\tPage %5u: G(limit=%u, remaining=%u, reads=%lu, writes=%lu, erases=%lu)", false,
					i,
					page.limit, (remaining > 0)?(remaining):(0),
					page.reads, page.writes, page.erases
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
	for (unsigned i = 0; i < m_pageManager->getPageCount(); ++i) {
		switch (m_pageManager->getPage(i).type) {
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
	for (unsigned i = 0; i < m_pageManager->getPageCount(); ++i) {
		switch (m_pageManager->getPage(i).type) {
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
