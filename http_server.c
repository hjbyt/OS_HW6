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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//
// TODO:
// 		- cleanup properly on (fatal) errors (close connection etc)
//      - on client error, stop handling client instead of exiting completely
//

//
// Macros
//

//TODO: uncomment
#define ENABLE_DEBUG_PRINTS


// Return number of elements in static array
#define ARRAY_LENGTH(array) (sizeof(array)/sizeof(array[0]))
// Exit with an error message
#define ERROR(...) error(EXIT_FAILURE, errno, __VA_ARGS__)
// Verify that a condition holds, else exit with an error.
#define VERIFY(condition, ...) if (!(condition)) ERROR(__VA_ARGS__)
// Check that a condition holds, else exit with an error (without referencing errno)
#define CHECK(condition, msg)                    \
	do {                                         \
		if (!(condition)) {                      \
			fprintf(stderr, "Error, " msg "\n"); \
			exit(EXIT_FAILURE);                  \
		}                                        \
	} while (0)

#ifdef ENABLE_DEBUG_PRINTS
#define PRINTF printf
#else
#define PRINTF
#endif

//
// Constants
//

typedef int bool;
#define FALSE 0
#define TRUE 1

#define KILO 1024
#define MEGA (KILO*KILO)

#define DEFAULT_HTTP_PORT 80

#define REQUEST_MAX_SIZE 8192

#define HTTP_OK "HTTP/1.0 200 OK\n\n"

//
// Structs
//

//
// Globals
//

//
// Function Declarations
//

void handle_request(int client_fd);
char* parse_request(char* request, int length, bool* is_post);
void handle_post_request(int client_fd, const char* path);
void handle_get_request(int client_fd, const char* path);
void write_all(int fd, void* data, int size);

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

	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	VERIFY(listen_fd != -1, "create socket failed");

	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(port);
	VERIFY(bind(listen_fd, (struct sockaddr*)&server_address, sizeof(server_address)) != -1, "bind failed");
	VERIFY(listen(listen_fd, workers_count) != -1, "listen failed");

	PRINTF("Server started\n");
	while (TRUE)
	{
		//TODO: use select() ?
		PRINTF("wait for connection\n");
		int client_fd = accept(listen_fd, NULL, NULL);
		VERIFY(client_fd != -1, "accept failed");

		handle_request(client_fd);

		close(client_fd);
	}

	return EXIT_SUCCESS;
}

void handle_request(int client_fd)
{
	PRINTF("got request\n");
	char* buffer = (char*)malloc(REQUEST_MAX_SIZE);
	VERIFY(buffer != NULL, "malloc failed");

	//TODO: read with a while loop ?
	int bytes_read = read(client_fd, buffer, REQUEST_MAX_SIZE);
	VERIFY(bytes_read != -1, "read from client socket failed");
	CHECK(bytes_read != 0, "client disconnected unexpectedly");
	PRINTF("request: %s\n", buffer);

	bool is_post;
	char* path = parse_request(buffer, bytes_read, &is_post);
	CHECK(path != NULL, "invalid request");

	if (is_post) {
		handle_post_request(client_fd, path);
	} else {
		handle_get_request(client_fd, path);
	}

	free(buffer);
}

char* parse_request(char* request, int length, bool* is_post)
{
	assert(is_post != NULL);
	char* s = request;
	if (length >=  3 && strncmp("GET ", s, 4) == 0) {
		s += 4;
		*is_post = FALSE;
	} else if (length >= 4 && strncmp("POST ", s, 5) == 0) {
		s += 5;
		*is_post = FALSE;
	} else {
		return NULL;
	}

	return strtok(s, " ");
}

void handle_post_request(int client_fd, const char* path)
{

}

void handle_get_request(int client_fd, const char* path)
{
	PRINTF("Get (%s)\n", path);
	struct stat path_stat;
	if (stat(path, &path_stat) == -1) {
		if (errno == ENOENT) {
			// TODO: send 404 not found
		} else {
			// TODO: return 500 internal error
		}
		return;
	}

	// TODO: check if is file / dir

	int fd = open(path, O_RDONLY);
	if (fd == -1) {
		// TODO: send 500 internal error
		return;
	}

	char* buffer = (char*)malloc(MEGA);
	VERIFY(buffer != NULL, "malloc failed");
	int bytes_read = 0;
	PRINTF("Send file\n");
	write_all(client_fd, HTTP_OK, ARRAY_LENGTH(HTTP_OK) - 1);
	while (TRUE)
	{
		bytes_read = read(fd, buffer, MEGA);
		VERIFY(bytes_read != -1, "read requested file failed");
		if (bytes_read == 0) {
			break;
		}
		write_all(client_fd, buffer, bytes_read);
	}

	close(fd);
}

void write_all(int fd, void* data, int size)
{
	assert(size >= 0);
	while (size > 0) {
		int bytes_written = write(fd, data, size);
		VERIFY(bytes_written != -1, "write response failed");
		size -= bytes_written;
		assert(size >= 0);
	}
}
