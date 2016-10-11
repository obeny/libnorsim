#ifndef __LIBNORSIM_H__
#define __LIBNORSIM_H__

#define ENV_LOG         "NS_LOG"
#define ENV_LOGLEVEL    "NS_LOGLEVEL"
#define ENV_CACHE_FILE  "NS_CACHE_FILE"
#define ENV_SIZE        "NS_SIZE"
#define ENV_ERASE_SIZE  "NS_ERASE_SIZE"
#define ENV_WEAK_PAGES  "NS_WEAK_PAGES"
#define ENV_GRAVE_PAGES "NS_GRAVE_PAGES"

#define PARSE_BEH_EIO "eio"
#define PARSE_BEH_RND "rnd"
#define PARSE_BEH_LEN 3

#define PARSE_NODE_DELIM   ';'
#define PARSE_PROP_DELIM   ','
#define PARSE_PREFIX_DELIM ' '

#define SIGNAL_REPORT_SHORT 1
#define SIGNAL_REPORT_DETAILED 2

#include <memory>
#include <mutex>

#include <mtd/mtd-user.h>

#include "PageManager.h"
#include "SyscallsCache.h"

class Logger;
class LogFormatter;

class Libnorsim
{
public:
	static Libnorsim & getInstance() {
		static Libnorsim instance;
		return instance;
	}

	SyscallsCache& getSyscallsCache() { return (*m_syscallsCache.get()); }
	PageManager& getPageManager() { return (*m_pageManager.get()); }
	Logger& getLogger() { return (*m_logger.get()); }

	bool isInitialized() { return (m_initialized); }

	std::mutex& getGlobalMutex() { return (m_mutex); }

	char* getCacheFile() { return (m_cacheFile.get()); }
	mtd_info_t* getMtdInfo() { return (&m_mtdInfo); }
	unsigned long getEraseSize() { return (m_eraseSize); }

	int getCacheFileFd() { return (m_cacheFileFd); }
	void setCacheFileFd(int fd) { m_cacheFileFd = fd; }

	char* getEraseBuffer() { return (m_eraseBuffer.get()); }

	bool isOpened() { return (m_opened); }
	void setOpened() { m_opened = true; }
	void setClosed() { m_opened = false; }

	void handleReportRequest();

private:
	Libnorsim();
	~Libnorsim();
	Libnorsim(const Libnorsim &);
	void operator=(Libnorsim const &);

	void initLogger();
	bool initSyscallsCache();
	bool initCacheFile();
	bool initSizes();
	bool initEraseBuffer();
	void initPageFailures();
	
	void initMtdInfo();

	void printUsage();

	void printPageReport(bool detailed = false);
	void printPageStatistics();

	bool m_initialized;
	bool m_opened;
	std::unique_ptr<LogFormatter> m_logFormatter;
	std::unique_ptr<Logger> m_logger;
	std::unique_ptr<SyscallsCache> m_syscallsCache;
	std::unique_ptr<PageManager> m_pageManager;
	std::mutex m_mutex;

	std::unique_ptr<char> m_cacheFile;
	std::unique_ptr<char> m_eraseBuffer;

	unsigned long m_size;
	unsigned long m_eraseSize;

	int m_cacheFileFd;

	mtd_info_t m_mtdInfo;

};

#endif // __LIBNORSIM_H__
