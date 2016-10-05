unsigned long get_min(unsigned long g, unsigned long p)
{
	unsigned long t = (0 != p);
	return ((t<=g)?(t):(g));
}

unsigned long get_max(unsigned long g, unsigned long p)
{
	return ((p>g)?(p):(g));
}
