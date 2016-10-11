#ifndef __PAGEMANAGER_H__
#define __PAGEMANAGER_H__

#define PAGE_BITFLIP_LIMIT 4

#include <memory>
#include <set>
#include <tuple>

enum e_beh_t {
	E_BEH_EIO = 0,
	E_BEH_RND
} ;

enum e_page_type_t {
	E_PAGE_NORMAL = 0b0000,
	E_PAGE_WEAK   = 0b0001,
	E_PAGE_GRAVE  = 0b0010
};

struct st_page_t {
	e_page_type_t type;
	unsigned short limit;
	unsigned long reads;
	unsigned long writes;
	unsigned long erases;
	std::set<std::tuple<unsigned, unsigned>> deadBits;
	bool unlocked;
};

struct st_page_stats_t {
	unsigned long min_reads;
	unsigned long max_reads;
	unsigned long min_writes;
	unsigned long max_writes;
	unsigned long min_erases;
	unsigned long max_erases;
};

class Libnorsim;

class PageManager {
public:
	PageManager(Libnorsim &libnorsim, const unsigned pageCount);

	unsigned getPageCount() { return (m_pageCount); }
	int getWeakPageCount() { return (m_weakPages); }
	int getGravePageCount() { return (m_gravePages); }

	e_beh_t getWeakPageBehavior() { return (m_behaviorWeak); }
	e_beh_t getGravePageBehavior() { return (m_behaviorGrave); }

	st_page_t& getPage(const unsigned index) { return (m_pages.get()[index]); }

	void parseWeakPagesEnv(const char *env);
	void parseGravePagesEnv(const char *env);

	void setBitMask(const unsigned index, char *buffer);
	void mergeBitMasks(const unsigned long offset, const unsigned long count, char *dst, const char *src);

private:
	void setPageType(const unsigned index, const e_page_type_t type, const unsigned short limit);
	void setPageDeadBits(const unsigned index);

	int parsePageType(const char *env, const char * const name, e_beh_t * const beh, const e_page_type_t type);
	int parsePageEnv(const char * const str, const e_page_type_t type);
	
	int m_weakPages;
	int m_gravePages;
	unsigned m_pageCount;

	e_beh_t m_behaviorWeak;
	e_beh_t m_behaviorGrave;

	std::unique_ptr<st_page_t[]> m_pages;

	Libnorsim &m_libnorsim;
};

#endif // __PAGEMANAGER_H__