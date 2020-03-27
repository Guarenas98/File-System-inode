#include <stdio.h>
#include "libfs.h"
//#include "libdisk.h"  //pa testear disco

//el main es el SO




void usage(char *prog)
{
	fprintf(stderr, "usage: %s <disk image file>\n", prog);
	exit(1);
}

int main(int argc, char *argv[])
{

	if (argc != 2) {
		usage(argv[0]);
	}
	char *path = argv[1];
	fs_boot(path);
	test_estructuras();
	fs_sync();
	
	return 0;
}

