
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int
main (int argc, char * argv[])
{
	const char * fdstr = getenv("MIR_SOCKET");
	if (!fdstr) {
		fprintf(stderr, "No MIR_SOCKET defined\n");
		return 1;
	}

	int fdnum = 0;
	sscanf(fdstr, "fd://%d", &fdnum);
	if (fdnum == 0) {
		fprintf(stderr, "Unable to get FD number\n");
		return 1;
	}

	char inchar;
	while (read(fdnum, &inchar, 1) == 1)
		fwrite(&inchar, 1, 1, stdout);

	close(fdnum);
	return 0;
}
