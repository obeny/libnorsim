// config:
// NS_SIZE=65536 NS_ERASE_SIZE=256 NS_WEAK_PAGES="rnd 0,1;" NS_GRAVE_PAGES="rnd 3,1;"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <mtd/mtd-user.h>

int main(int argc, char* argv[])
{
	(void)argv;
	setenv("NS_SIZE", "65536", 1);
	setenv("NS_ERASE_SIZE", "256", 1);
	setenv("NS_WEAK_PAGES", "eio 0,2;", 1);
	setenv("NS_GRAVE_PAGES", "eio 3,1;", 1);
	setenv("NS_CACHE_FILE", "/tmp/nor", 1);

	puts("running main");
	char buf[512*1024];
	int fd;
	int ret;

	fd = open("non_existing_file", O_RDWR);
	printf("non_existing_file fd=%d\n", fd);

	unlink("/tmp/nor_tmp");
	fd = open("/tmp/nor_tmp", O_RDWR | O_CREAT | O_APPEND | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	printf("nor_tmp fd=%d\n", fd);
	ret = close(fd);

	fd = open("/tmp/nor", O_RDWR);
	printf("cache_file fd=%d\n", fd);
	ret = close(fd);
	printf("ret=%d\n", ret);
	fd = open("/tmp/nor", O_RDWR);
	printf("cache_file fd=%d\n", fd);

	int read_bytes;
	int written_bytes;

	mtd_info_t mtd_info;
	erase_info_t ei;

	// memgetinfo
	ioctl(fd, MEMGETINFO, &mtd_info);
	printf("mtd_info.type=%d\n", mtd_info.type);
	printf("mtd_info.flags=%d\n", mtd_info.flags);
	printf("mtd_info.size=%d\n", mtd_info.size);
	printf("mtd_info.erasesize=%d\n", mtd_info.erasesize);
	printf("mtd_info.writesize=%d\n", mtd_info.writesize);
	printf("mtd_info.oobsize=%d\n", mtd_info.oobsize);

	// erasing - invalid ei (start)
	printf("erase - invalid ei (start)\n");
	ei.start = 1;
	ei.length = 256 * 1024;
	ret = ioctl(fd, MEMUNLOCK, &ei);
	printf("ioctl MEMUNLOCK ret=%d\n", ret);
	ret = ioctl(fd, MEMERASE, &ei);
	printf("ioctl MEMERASE ret=%d\n", ret);

	// erasing - invalid ei (length)
	printf("erase - invalid ei (length)\n");
	ei.start = 256 * 1024;
	ei.length = 1;
	ret = ioctl(fd, MEMUNLOCK, &ei);
	printf("ioctl MEMUNLOCK ret=%d\n", ret);
	ret = ioctl(fd, MEMERASE, &ei);
	printf("ioctl MEMERASE ret=%d\n", ret);

	// erasing without unlocking
	ei.start = 0;
	ei.length = 256 * 1024;
	printf("erase - without unlocking\n");
	ret = ioctl(fd, MEMERASE, &ei);
	printf("ioctl MEMERASE ret=%d\n", ret);

	// erasing with unlocking
	printf("erase - with unlocking (cycles=1/2)\n");
	ret = ioctl(fd, MEMUNLOCK, &ei);
	printf("ioctl MEMUNLOCK ret=%d\n", ret);
	ret = ioctl(fd, MEMERASE, &ei);
	printf("ioctl MEMERASE ret=%d\n", ret);

	// writing to weak page
	printf("writing to weak page (cycles=1/2)\n");
	memset(buf, 0xA5, 256 * 1024);
	written_bytes = pwrite(fd, buf, ei.length, ei.start);
	printf("written ret=%d zeros\n", written_bytes);
	read_bytes = pread(fd, buf, 256*1024, ei.start);
	printf("read ret=%d zeros\n", read_bytes);
	for (long i = 0; i < (256 * 1024); ++i){
		if ((char)0xA5!= buf[i])
			printf("unexpected value found, should be 0xA5\n");
	}

	// erasing weak page
	printf("erase weak page (cycles=2/2)\n");
	ret = ioctl(fd, MEMUNLOCK, &ei);
	printf("ioctl MEMUNLOCK ret=%d\n", ret);
	ret = ioctl(fd, MEMERASE, &ei);
	printf("ioctl MEMERASE ret=%d\n", ret);

	// erasing worn page
	printf("erase worn page (cycles=3/2)\n");
	ret = ioctl(fd, MEMUNLOCK, &ei);
	printf("ioctl MEMUNLOCK ret=%d\n", ret);
	ret = ioctl(fd, MEMERASE, &ei);
	printf("ioctl MEMERASE ret=%d\n", ret);

	// writing worn page
	printf("writing worn page (cycles=3/2)\n");
	written_bytes = pwrite(fd, buf, ei.length, ei.start);
	printf("written ret=%d bytes\n", written_bytes);
	read_bytes = pread(fd, buf, 256*1024, ei.start);
	printf("read ret=%d bytes\n", read_bytes);
	for (long i = 0; i < (256 * 1024); ++i){
		if ((char)0xFF != buf[i])
			printf("unexpected value found at offset=%ld: 0x%02X, should be: 0xFF\n", i, buf[i] & 0xFF);
	}

	// write block exceeding erasepage boundary
	printf("write block exceeding erasepage boundary\n");
	written_bytes = pwrite(fd, buf, 256 * 1024 * 2, 256 * 1024);
	printf("written_bytes=%d\n", written_bytes);

	// read block exceeding erasepage boundary
	printf("read block exceeding erasepage boundary\n");
	read_bytes = pread(fd, buf, 256 * 1024 * 2, 256 * 1024);
	printf("read_bytes=%d\n", read_bytes);

	// read grave page
	printf("read grave page (cycles=1/1)\n");
	read_bytes = pread(fd, buf, 128, 256 * 3 * 1024);
	printf("read_bytes=%d\n", read_bytes);

	// read graved page
	printf("read graved page (cycles=2/1)\n");
	read_bytes = pread(fd, buf, 128, 256 * 3 * 1024);
	printf("read_bytes=%d\n", read_bytes);

	// read
	ret = read(fd, buf, 1);
	printf("read ret=%d\n", ret);

	// write
	ret = write(fd, buf, 1);
	printf("write ret=%d\n", ret);

	// hang if needed :)
	if (argc > 1) {
		while(1)
			ioctl(fd, MEMUNLOCK, &ei);
	}

	close(fd);
}
