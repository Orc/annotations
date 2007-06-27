#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "mapfile.h"


char *
mapfd(int fd, long *size)
{
    struct stat info;
    char *map;

    if (fstat(fd, &info) == -1)
	return 0;

    map = mmap(0,info.st_size,PROT_READ,MAP_SHARED,fd,0L);
    close(fd);

    if (map != (void*)-1) {
	*size = info.st_size;
	return map;
    }
    return 0;
}


char *
mapfile(char *f, long *size)
{
    int fd = open(f, O_RDONLY);
    char *map;

    if (fd == -1)
	return 0;

    map = mapfd(fd, size);
    close(fd);
    return map;
}
