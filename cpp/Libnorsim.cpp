#include <cstdio>
#include <cstring>

#include <unistd.h>
#include <sys/stat.h>

#include "Libnorsim.h"
#include "LogFormatterLibnorsim.h"

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
	: m_initialized(false) {
	initLogger();
	if (!initSyscallsCache())
		return;
	if (!initCacheFile())
		return;
	if (!initSizes())
		return;
	if (!initPages())
		return;

	initWeakPages();
	initGravePages();

	m_logger->log(Loglevel::DEBUG, "Libnorsim init OK");
	m_initialized = true;
}

Libnorsim::~Libnorsim() {
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
	m_syscallsCache = new SyscallsCache(this);
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
	m_pageInfo.reset((st_page_t*)malloc(sizeof(st_page_t) * m_pages));
	if (!m_pageInfo) {
		m_logger->log(Loglevel::FATAL, "Couldn't allocate memory for page information");
		return (false);
	}
	memset(m_pageInfo.get(), 0x00, sizeof(st_page_t) * m_pages);
	return (true);
}

void Libnorsim::initWeakPages() {
	char *env_weak_pages = getenv(ENV_WEAK_PAGES);
	if (!env_weak_pages) {
		m_logger->log(Loglevel::WARNING, "No weak_pages given, assuming no weak pages");
		return;
	}

	parsePageType(env_weak_pages, ENV_WEAK_PAGES, "weak", &beh_weak, E_PAGE_WEAK);
}

void Libnorsim::initGravePages() {
	char *env_grave_pages = getenv(ENV_GRAVE_PAGES);
	if (!env_grave_pages) {
		m_logger->log(Loglevel::WARNING, "No grave_pages given, assuming no grave pages");
		return;
	}

	parsePageType(env_grave_pages, ENV_GRAVE_PAGES, "grave", &beh_grave, E_PAGE_GRAVE);
}

void Libnorsim::parsePageType(char* env, const char* const env_name, const char* const name,
		e_beh_t* const beh, const e_page_type_t type) {
	int res = 0;

	if (!env) {
		if (0 == strncmp(env, PARSE_BEH_EIO, PARSE_BEH_LEN)) {
			*beh = E_BEH_EIO;
			m_logger->log(Loglevel::INFO, "Set %s behavior: %d", name, PARSE_BEH_EIO);
			env = strchr(env, PARSE_PREFIX_DELIM) + 1;
		}
		else if (0 == strncmp(env, PARSE_BEH_RND, PARSE_BEH_LEN)) {
			*beh = E_BEH_RND;
			m_logger->log(Loglevel::INFO, "Set %s behavior: %d", name, PARSE_BEH_RND);
			env = strchr(env, PARSE_PREFIX_DELIM) + 1;
		} else {
			*beh = E_BEH_EIO;
			m_logger->log(Loglevel::INFO, "No %s pages behavior defined, assuming \"eioa\"", name);
		}
		if ((res = parsePageEnv(env, type)) < 0)
			m_logger->log(Loglevel::INFO, "Couldn't parse %s", env);
	}
	m_logger->log(Loglevel::INFO, "Set %s pages: %d", name, res);
	return;
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

	do {
		end_node = strchr(cur_node, PARSE_NODE_DELIM);
		if (NULL == end_node) break;
		end_prop = strchr(cur_node, PARSE_PROP_DELIM);
		if (NULL == end_prop) break;

		page = strtoul(cur_node, &end_prop, 10);
		cur_prop = ++end_prop;
		cycles = strtoul(cur_prop, NULL, 10);
		m_logger->log(Loglevel::DEBUG, "page=%lu\tcycles=%u%s", page, cycles);
		if (page > m_pages) {
			m_logger->log(Loglevel::ERROR, "trying to set non existing page (page=%lu > pages=%lu)", page, m_pages);
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