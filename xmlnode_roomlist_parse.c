#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

int main(int argc, char **argv) 
{
	int fd;
	struct stat *statbuf;
	void *str;
	size_t size;

	/* open 'rooms.xml' and map it to memory
	 * to treat it as a char array 
	 */
	fd = open("rooms.xml", O_RDONLY, O_NONBLOCK);
	fstat(fd, statbuf);
	size = statbuf->st_size;
	str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

	/* do something interesting with our 
	 * xml string
	 */
	printf("%s\n%s\n", "rooms.xml", str);

	/* clean up
	 */
	munmap(str, size);
	close(fd);
	return 0;
}
