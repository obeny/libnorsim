#include <cstring>

#include "PageManager.h"
#include "Libnorsim.h"
#include "Logger.h"

PageManager::PageManager(Libnorsim &libnorsim, const unsigned pageCount)
 : m_pageCount(pageCount), m_libnorsim(libnorsim) {
	m_libnorsim.getLogger().log(Loglevel::INFO, "Set page count: %lu", false, m_pageCount);
	m_pages.reset(new st_page_t[m_pageCount]);
	if (!m_pages)
		throw std::runtime_error("Couldn't allocate memory for page information structures");
	for (unsigned i = 0; i < m_pageCount; ++i) {
		m_pages[i].reads = 0;
		m_pages[i].writes = 0;
		m_pages[i].erases = 0;
		m_pages[i].limit = 0;
		m_pages[i].type = E_PAGE_NORMAL;
		m_pages[i].unlocked = false;
	}
}

void PageManager::parseWeakPagesEnv(const char *env) {
	m_weakPages = parsePageType(env, "weak", &m_behaviorWeak, E_PAGE_WEAK);
}

void PageManager::parseGravePagesEnv(const char *env) {
	m_gravePages = parsePageType(env, "grave", &m_behaviorGrave, E_PAGE_GRAVE);
}

void PageManager::setBitMask(const unsigned index, char *buffer) {
	std::set<std::tuple<unsigned, unsigned>>::iterator it;
	for (it = m_pages[index].deadBits.begin(); it != m_pages[index].deadBits.end(); ++it) {
		buffer[std::get<0>(*it)] = ~(1 << std::get<1>(*it));
	}
}

void PageManager::mergeBitMasks(const unsigned long offset, const unsigned long count, char *dst, const char *src) {
	char *head = &dst[offset];
	char *end = &head[count];
	char tmp;
	for (; head < end; ++head) {
		tmp = *head & *src;
		*head = tmp;
	}
}

void PageManager::setPageType(const unsigned index, const e_page_type_t type, const unsigned short limit) {
	getPage(index).type = type;
	getPage(index).limit = limit;
}

void PageManager::setPageDeadBits(const unsigned index) {
	long rnd, bit;
	for (int i = 0; i < PAGE_BITFLIP_LIMIT; ++i) {
		rnd = rand() % m_libnorsim.getEraseSize();
		bit = rnd % 8;
		m_pages[index].deadBits.insert(std::make_tuple(rnd, bit));
	}
	m_libnorsim.getLogger().log(Loglevel::DEBUG, "\tPage deadbits:");
	std::set<std::tuple<unsigned, unsigned>>::iterator it;
	for (it = m_pages[index].deadBits.begin(); it != m_pages[index].deadBits.end(); ++it) {
		m_libnorsim.getLogger().log(Loglevel::DEBUG, "\t\tbyte=%ld, bit=%d", false, std::get<0>(*it), std::get<1>(*it));
	}
}

int PageManager::parsePageType(const char *env, const char * const name, e_beh_t * const beh, const e_page_type_t type) {
	int res = 0;

	if (0 == strncmp(env, PARSE_BEH_EIO, PARSE_BEH_LEN)) {
		*beh = E_BEH_EIO;
		m_libnorsim.getLogger().log(Loglevel::INFO, "Set \"%s pages\" behavior: %s", false, name, PARSE_BEH_EIO);
		env = strchr(env, PARSE_PREFIX_DELIM) + 1;
	}
	else if (0 == strncmp(env, PARSE_BEH_RND, PARSE_BEH_LEN)) {
		*beh = E_BEH_RND;
		m_libnorsim.getLogger().log(Loglevel::INFO, "Set \"%s pages\" behavior: %s", false, name, PARSE_BEH_RND);
		env = strchr(env, PARSE_PREFIX_DELIM) + 1;
	} else {
		*beh = E_BEH_EIO;
		m_libnorsim.getLogger().log(Loglevel::INFO, "No \"%s pages\" behavior defined, assuming \"eio\"", false, name);
	}

	if ((res = parsePageEnv(env, type)) < 0) {
		m_libnorsim.getLogger().log(Loglevel::WARNING, "Couldn't parse: \"%s\"", false, env);
		return (res);
	}
	m_libnorsim.getLogger().log(Loglevel::INFO, "Set \"%s pages\": %d", false, name, res);
	return (res);
}

int PageManager::parsePageEnv(const char * const str, const e_page_type_t type) {
	const char *cur_node = str;
	char *cur_prop;
	char *end_node;
	char *end_prop;
	int count = -1;
	unsigned limit;
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
		limit = strtoul(cur_prop, NULL, 10);
		m_libnorsim.getLogger().log(Loglevel::DEBUG, "\t(%c)\tpage=%lu\tlimit=%u", false,
			type_sign, page, limit
		);
		if (page > m_pageCount) {
			m_libnorsim.getLogger().log(Loglevel::ERROR, "\t(%c)\ttrying to set non existing page (page=%lu > pages=%lu)", false,
				type_sign, page, m_pageCount
			);
			return -1;
		}
		setPageType(page, type, limit);
		setPageDeadBits(page);

		if (-1 == count)
			count = 0;
		++count;
		cur_node = ++end_node;
	} while (NULL != cur_node);

	return (count);
}

