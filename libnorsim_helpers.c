#include <stdio.h>
#include <pthread.h>

#include "libnorsim.h"

extern pthread_mutex_t mutex;
extern st_page_t *page_info;
extern unsigned long erase_size;

unsigned long get_min(unsigned long g, unsigned long p)
{
	unsigned long t = (0 != p);
	return ((t<=g)?(t):(g));
}

unsigned long get_max(unsigned long g, unsigned long p)
{
	return ((p>g)?(p):(g));
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
