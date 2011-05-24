/* system */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* libpurple */
#include <xmlnode.h>

int main(int argc, char **argv) 
{
	int fd;
	struct stat statbuf;
	void *mem = NULL;
	size_t size;
	xmlnode *node = NULL;
	int status;

	/* low-level file i/o junk:
	 * open 'rooms.xml' and map it to memory
	 * to treat it as a char array 
	 */
	fd = open("rooms.xml", O_RDONLY, O_NONBLOCK);
	status = fstat(fd, &statbuf);
	size = statbuf.st_size;
	mem = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

	/* do something interesting with our 
	 * xml string
	 */
	printf("%s\n%s\n", "rooms.xml", (char *)mem);

	/* no matter what I do, I see these assertion warnings
	 * for every node in the xml string
         * ** (process:19998): CRITICAL **: purple_dbus_register_pointer: assertion map_node_id failed
         * or similar
         * I've tried using a string instead of a mmap'ed file
         * I've tried using a short xml string like "<node/>"
         * I've tried using 'xmlnode_from_file()'
         */
	node = xmlnode_from_str((char *)mem, size);

	if (node) {
		printf("%s\n", node->name);
	}

	/* clean up
	 */
	munmap(mem, size);
	close(fd);
	return 0;
}
