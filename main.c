#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{
	char buf[512*1024];
	int fd = open("/tmp/nor", O_RDWR);
	int read_bytes;
	int written_bytes;

	printf("fd=%d\n", fd);
	
	// read
	read_bytes = pread(fd, buf, 128, 0);
	printf("read_bytes=%d\n", read_bytes);
	
	read_bytes = pread(fd, buf, 128, 256 * 3 * 1024);
	printf("read_bytes=%d\n", read_bytes);

	read_bytes = pread(fd, buf, 128, 256 * 3 * 1024);
	printf("read_bytes=%d\n", read_bytes);

	read_bytes = pread(fd, buf, 256 * 1024 * 2, 256 * 3 * 1024);
	printf("read_bytes=%d\n", read_bytes);

	// write
	written_bytes = pwrite(fd, buf, 128, 256 * 1024);
	printf("written_bytes=%d\n", written_bytes);

	written_bytes = pwrite(fd, buf, 128, 0);
	printf("written_bytes=%d\n", written_bytes);

	written_bytes = pwrite(fd, buf, 128, 0);
	printf("written_bytes=%d\n", written_bytes);

	written_bytes = pwrite(fd, buf, 256 * 1024 * 2, 0);
	printf("written_bytes=%d\n", written_bytes);

	close(fd);
}
