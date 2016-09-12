#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <mtd/mtd-user.h>

int main(void)
{
	char buf[512*1024];
	int fd = open("/tmp/nor", O_RDWR);
	int read_bytes;
	int written_bytes;
	int ret;

	mtd_info_t mtd_info;
	erase_info_t ei;

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

	ioctl(fd, MEMGETINFO, &mtd_info);
	printf("mtd_info.type=%d\n", mtd_info.type);
	printf("mtd_info.flags=%d\n", mtd_info.flags);
	printf("mtd_info.size=%d\n", mtd_info.size);
	printf("mtd_info.erasesize=%d\n", mtd_info.erasesize);
	printf("mtd_info.writesize=%d\n", mtd_info.writesize);
	printf("mtd_info.oobsize=%d\n", mtd_info.oobsize);

	ei.start = 1;
	ei.length = 256 * 1024;
	ret = ioctl(fd, MEMUNLOCK, &ei);
	printf("ret=%d\n", ret);
	ret = ioctl(fd, MEMERASE, &ei);
	printf("ret=%d\n", ret);

	ei.start = 256 * 1024;
	ei.length = 1;
	ret = ioctl(fd, MEMUNLOCK, &ei);
	printf("ret=%d\n", ret);
	ret = ioctl(fd, MEMERASE, &ei);
	printf("ret=%d\n", ret);

	ei.start = 0;
	ei.length = 256 * 1024;
	ret = ioctl(fd, MEMUNLOCK, &ei);
	printf("ret=%d\n", ret);
	ret = ioctl(fd, MEMERASE, &ei);
	printf("ret=%d\n", ret);

	ret = ioctl(fd, MEMERASE, &ei);
	printf("ret=%d\n", ret);

	close(fd);
}
