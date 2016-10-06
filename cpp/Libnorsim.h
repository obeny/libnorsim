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

#include "SyscallsCache.h"

typedef enum
{
	E_BEH_EIO = 0,
	E_BEH_RND
} e_beh_t;

typedef enum
{
	E_PAGE_NORMAL = 0b0000,
	E_PAGE_WEAK   = 0b0001,
	E_PAGE_GRAVE  = 0b0010
} e_page_type_t;

typedef struct
{
	e_page_type_t type;
	unsigned short cycles;
	unsigned short current_cycles;
	unsigned long reads;
	unsigned long writes;
	unsigned long erases;
	unsigned unlocked;
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

class Logger;
class LogFormatter;

class Libnorsim
{
public:
	static Libnorsim & getInstance() {
		static Libnorsim instance;
		return instance;
	}

	SyscallsCache & getSyscallsCache() { return (*m_syscallsCache.get()); }
	Logger& getLogger() { return (*m_logger.get()); }

	bool isInitialized() { return (m_initialized); }

	std::mutex & getGlobalMutex() { return (m_mutex); }

	char *getCacheFile() { return (m_cacheFile.get()); }
	mtd_info_t *getMtdInfo() { return (&m_mtdInfo); }
	st_page_t *getPageInfo() { return (m_pageInfo.get()); }
	unsigned long getEraseSize() { return (m_eraseSize); }
	e_beh_t getWeakPageBehavior() { return (m_behaviorWeak); }
	e_beh_t getGravePageBehavior() { return (m_behaviorGrave); }

	int getCacheFileFd() { return (m_cacheFileFd); }
	void setCacheFileFd(int fd) { m_cacheFileFd = fd; }

	char *getEraseBuffer() { return (m_eraseBuffer.get()); }

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
	bool initPages();
	bool initEraseBuffer();
	
	void initWeakPages();
	void initGravePages();
	void initMtdInfo();

	void printUsage();

	void printPageReport(bool detailed = false);
	void printPageStatistics();

	int parsePageType(char* env, const char* const env_name, const char* const name,
		e_beh_t* const beh, const e_page_type_t type);
	int parsePageEnv(const char * const str, e_page_type_t type);

	bool m_initialized;
	bool m_opened;
	std::unique_ptr<LogFormatter> m_logFormatter;
	std::unique_ptr<Logger> m_logger;
	std::unique_ptr<SyscallsCache> m_syscallsCache;
	std::mutex m_mutex;

	std::unique_ptr<char> m_cacheFile;
	std::unique_ptr<char> m_eraseBuffer;
	std::unique_ptr<st_page_t> m_pageInfo;
	unsigned m_pages;
	unsigned long m_size;
	unsigned long m_eraseSize;

	int m_weakPages;
	int m_gravePages;

	int m_cacheFileFd;

	mtd_info_t m_mtdInfo;

	e_beh_t m_behaviorWeak;
	e_beh_t m_behaviorGrave;
};

#endif // __LIBNORSIM_H__
