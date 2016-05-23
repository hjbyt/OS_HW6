#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <error.h>

//
// Macros
//

// Return number of elements in static array
#define ARRAY_LENGTH(array) (sizeof(array)/sizeof(array[0]))
// Exit with an error message
#define ERROR(...) error(EXIT_FAILURE, errno, __VA_ARGS__)
// Verify that a condition holds, else exit with an error.
#define VERIFY(condition, ...) if (!(condition)) ERROR(__VA_ARGS__)

//
// Constants
//

typedef int bool;
#define FALSE 0
#define TRUE 1

#define KILO 1024
#define MEGA (KILO*KILO)

#define DEFAULT_HTTP_PORT 80

//
// Structs
//

//
// Globals
//

//
// Function Declarations
//


//
// Implementation
//

int main(int argc, char** argv)
{
	if (argc != 3 && argc != 2) {
		printf("Usage: ./http_server <workers> [<port>]\n");
		return EXIT_FAILURE;
	}

	errno = 0;
	int workers_count = strtol(argv[1], NULL, 0);
	VERIFY(errno == 0 && workers_count >= 1, "Invallid argument given as <workers>");
	int port;
	if (argc == 3) {
		errno = 0;
		port = strtol(argv[2], NULL, 0);
		VERIFY(errno == 0 && port >= 0, "Invallid argument given as <port>");
	} else {
		port = DEFAULT_HTTP_PORT;
	}

	return EXIT_SUCCESS;
}

