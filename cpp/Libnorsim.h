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

#include <memory>

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

class Logger;
class LogFormatter;

class Libnorsim
{
public:
	static Libnorsim & getInstance() {
		static Libnorsim instance;
		return instance;
	}

	SyscallsCache & getSyscallsCache();
	Logger& getLogger() { return *(m_logger.get()); }

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
	void initWeakPages();
	void initGravePages();

	void parsePageType(char* env, const char* const env_name, const char* const name,
		e_beh_t* const beh, const e_page_type_t type);
	int parsePageEnv(const char * const str, e_page_type_t type);

	bool m_initialized;
	std::unique_ptr<LogFormatter> m_logFormatter;
	std::unique_ptr<Logger> m_logger;
	SyscallsCache *m_syscallsCache;

	std::unique_ptr<char> m_cacheFile;
	std::unique_ptr<st_page_t> m_pageInfo;
	unsigned m_pages;
	unsigned long m_size;
	unsigned long m_eraseSize;

	e_beh_t beh_weak;
	e_beh_t beh_grave;
};

#endif // __LIBNORSIM_H__
