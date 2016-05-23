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
#include <signal.h>

//
// TODO:
//      - on client error, stop handling client instead of exiting completely
//

//
// Macros
//

// Return number of elements in static array
#define ARRAY_LENGTH(array) (sizeof(array)/sizeof(array[0]))
// Exit with an error message
#define ERROR(...)           \
	do {                     \
		perror(__VA_ARGS__); \
		goto end;            \
	} while (0)
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
// If there is an error, propagate it.
#define PROPAGATE(condition) if (!(condition)) goto end

//TODO: comment out
#define ENABLE_DEBUG_PRINTS

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

int listen_fd = -1;

//
// Function Declarations
//

bool register_signal_handlers();
void kill_signal_handler(int signum);
bool init(int port, int workers_count);
void uninit();
bool serve();
bool handle_request(int client_fd);
char* parse_request(char* request, int length, bool* is_post);
bool handle_post_request(int client_fd, const char* path);
bool handle_get_request(int client_fd, const char* path);
bool write_all(int fd, void* data, int size);

//
// Implementation
//

int main(int argc, char** argv)
{
	int return_value = EXIT_FAILURE;
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

	PROPAGATE(init(port, workers_count));

	PRINTF("Server started\n");
	while (TRUE)
	{
		PROPAGATE(serve());
	}

	return_value = EXIT_SUCCESS;
end:
	uninit();
	return return_value;
}

bool register_signal_handlers()
{
	bool success = FALSE;

	struct sigaction kill_action;
	kill_action.sa_handler = kill_signal_handler;
	if (sigemptyset(&kill_action.sa_mask) == -1) {
		ERROR("Error calling sigemptyset");
	}
	kill_action.sa_flags = 0;

	if (sigaction(SIGINT, &kill_action, NULL) == -1) {
		ERROR("Error, sigaction failed");
	}
	if (sigaction(SIGTERM, &kill_action, NULL) == -1) {
		ERROR("Error, sigaction failed");
	}

	success = TRUE;

end:
	return success;
}

void kill_signal_handler(int signum)
{
	uninit();
	exit(EXIT_SUCCESS);
}

bool init(int port, int workers_count)
{
	bool success = FALSE;

	assert(workers_count >= 1);
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	VERIFY(listen_fd != -1, "create socket failed");

	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(port);
	VERIFY(bind(listen_fd, (struct sockaddr*)&server_address, sizeof(server_address)) != -1, "bind failed");
	VERIFY(listen(listen_fd, workers_count) != -1, "listen failed");

	success = TRUE;
end:
	if (!success) {
		uninit();
	}
	return success;
}

void uninit()
{
	if (listen_fd != -1) {
		close(listen_fd);
		listen_fd = -1;
	}
}

bool serve()
{
	bool success = FALSE;

	PRINTF("wait for connection\n");
	int client_fd = accept(listen_fd, NULL, NULL);
	VERIFY(client_fd != -1, "accept failed");

	PROPAGATE(handle_request(client_fd));

	success = TRUE;
end:
	if (client_fd != -1 ) close(client_fd);
	return success;
}

bool handle_request(int client_fd)
{
	bool success = FALSE;
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

	success = TRUE;
end:
	if (buffer != NULL) free(buffer);
	return success;
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

bool handle_post_request(int client_fd, const char* path)
{
	return FALSE;
}

bool handle_get_request(int client_fd, const char* path)
{
	bool success = FALSE;
	struct stat path_stat;
	int fd = -1;
	char* buffer = NULL;
	PRINTF("Get (%s)\n", path);

	if (stat(path, &path_stat) == -1) {
		if (errno == ENOENT) {
			// TODO: send 404 not found
		} else {
			// TODO: return 500 internal error
		}
		goto end;
	}

	// TODO: check if is file / dir

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		// TODO: send 500 internal error
		goto end;
	}

	buffer = (char*)malloc(MEGA);
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

	success = TRUE;
end:
	if (buffer != NULL) free(buffer);
	if (fd != -1) close(fd);
	return success;
}

bool write_all(int fd, void* data, int size)
{
	bool success = FALSE;
	assert(size >= 0);
	while (size > 0) {
		int bytes_written = write(fd, data, size);
		VERIFY(bytes_written != -1, "write response failed");
		size -= bytes_written;
		assert(size >= 0);
	}
	success = TRUE;
end:
	return success;
}
