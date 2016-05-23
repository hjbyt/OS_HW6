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

	return EXIT_SUCCESS;
}

