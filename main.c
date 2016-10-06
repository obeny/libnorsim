// config:
// NS_SIZE=65536 NS_ERASE_SIZE=256 NS_WEAK_PAGES="rnd 0,1;" NS_GRAVE_PAGES="rnd 3,1;"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <mtd/mtd-user.h>

int main(int argc, char* argv[])
{
	(void)argv;

	puts("running main");
	char buf[512*1024];
	int fd = open("/tmp/nor", O_RDWR);
	int read_bytes;
	int written_bytes;
	int ret;

	mtd_info_t mtd_info;
	erase_info_t ei;

	printf("fd=%d\n", fd);

	// memgetinfo
	ioctl(fd, MEMGETINFO, &mtd_info);
	printf("mtd_info.type=%d\n", mtd_info.type);
	printf("mtd_info.flags=%d\n", mtd_info.flags);
	printf("mtd_info.size=%d\n", mtd_info.size);
	printf("mtd_info.erasesize=%d\n", mtd_info.erasesize);
	printf("mtd_info.writesize=%d\n", mtd_info.writesize);
	printf("mtd_info.oobsize=%d\n", mtd_info.oobsize);

	// erasing - invalid ei
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

	// erasing weak page (cycles=1)
	ei.start = 0;
	ei.length = 256 * 1024;
	ret = ioctl(fd, MEMUNLOCK, &ei);
	printf("ret=%d\n", ret);
	ret = ioctl(fd, MEMERASE, &ei);
	printf("ret=%d\n", ret);

	// erasing worn page
	ret = ioctl(fd, MEMUNLOCK, &ei);
	printf("ret=%d\n", ret);
	ret = ioctl(fd, MEMERASE, &ei);
	printf("ret=%d\n", ret);

	// erasing without unlocking
	ret = ioctl(fd, MEMERASE, &ei);
	printf("ret=%d\n", ret);

	// read - worn page
	read_bytes = pread(fd, buf, 128, 0);
	printf("read_bytes=%d\n", read_bytes);
	
	// read grave page (cycles=1)
	read_bytes = pread(fd, buf, 128, 256 * 3 * 1024);
	printf("read_bytes=%d\n", read_bytes);

	// read graved page
	read_bytes = pread(fd, buf, 128, 256 * 3 * 1024);
	printf("read_bytes=%d\n", read_bytes);

	// read block exceeding erasepage boundary
	read_bytes = pread(fd, buf, 256 * 1024 * 2, 256 * 3 * 1024);
	printf("read_bytes=%d\n", read_bytes);

	// write
	written_bytes = pwrite(fd, buf, 128, 256 * 1024);
	printf("written_bytes=%d\n", written_bytes);

	// write worn page
	written_bytes = pwrite(fd, buf, 128, 0);
	printf("written_bytes=%d\n", written_bytes);

	// write block exceeding erasepage boundary
	written_bytes = pwrite(fd, buf, 256 * 1024 * 2, 0);
	printf("written_bytes=%d\n", written_bytes);

	// read
	ret = read(fd, buf, 1);
	printf("ret=%d\n", ret);

	// write
	ret = write(fd, buf, 1);
	printf("ret=%d\n", ret);

	// hang if needed :)
	if (argc > 1) {
		while(1)
			ioctl(fd, MEMUNLOCK, &ei);
	}

	close(fd);
}
