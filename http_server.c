//
// TODO: document, explain...
//

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
#include <stddef.h>
#include <error.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

//
// TODO: - change the special errors (403, etc) to 500 ?
//       - implement multiple threads
//       - return HTML body in all responses?
//       - what if a 200 OK is sent, but then there is an error reading the file / direcory?
//         maybe we should prepare the body in advance, and send all at once.
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

//TODO: comment out?
#define ENABLE_DEBUG_PRINTS

#ifdef ENABLE_DEBUG_PRINTS
#define PRINTF printf
#else
#define PRINTF
#endif

//TODO: remove / ifdef DEBUG ?
#define WARN_UNUSED __attribute__((warn_unused_result))

//
// Constants
//

typedef int bool;
#define FALSE 0
#define TRUE 1

#define KILO 1024
#define MEGA (KILO*KILO)

#define DEFAULT_HTTP_PORT 80

#define REQUEST_MAX_LENGTH 1023
#define STATUS_LINE_MAX_LENGTH 1024


typedef enum
{
	PARSE_SUCCESS,
	PARSE_BAD_REQUEST,
	PARSE_UNSUPPORTED_REQUEST,
} ParseResult ;

const char* SUPPORTED_HTTP_METHODS[] = {
		"GET", "POST"
};

const char* UNSUPPORTED_HTTP_METHODS[] = {
		"OPTIONS", "HEAD", "PUT", "DELETE", "TRACE", "CONNECT"
};

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

bool register_signal_handlers() WARN_UNUSED;
void kill_signal_handler(int signum);
bool init(int port, int workers_count) WARN_UNUSED;
void uninit();
bool serve() WARN_UNUSED;
bool handle_request(int client_fd) WARN_UNUSED;
ParseResult parse_request(char* request, char** path);
bool is_string_in_array(const char* string, const char** string_array, int array_length);
bool handle_get_request(int client_fd, const char* path) WARN_UNUSED;
bool send_file(int client_fd, const char* file_path) WARN_UNUSED;
bool send_dir_list(int client_fd, const char* dir_path) WARN_UNUSED;
bool send_response(int fd, int status, const char* reason, char* body) WARN_UNUSED;
bool send_status_line(int fd, int status, const char* reason) WARN_UNUSED;
bool write_all(int fd, const void* data, int size) WARN_UNUSED;
size_t dirent_buf_size(DIR * dirp);

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
	//TODO: remove setsockopt
	VERIFY(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == 0, "setsockopt failed");
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
	int client_fd = -1;

	PRINTF("wait for connection\n");
	client_fd = accept(listen_fd, NULL, NULL);
	VERIFY(client_fd != -1, "accept failed");

	if (!handle_request(client_fd)) {
		PROPAGATE(send_response(client_fd, 500, "Internal Server Error", "Internal Server Error"));
	}

	success = TRUE;
end:
	if (client_fd != -1) close(client_fd);
	return success;
}

bool handle_request(int client_fd)
{
	bool success = FALSE;
	char* buffer = NULL;

	buffer = (char*)malloc(REQUEST_MAX_LENGTH + 1);
	VERIFY(buffer != NULL, "malloc failed");

	int bytes_read = read(client_fd, buffer, REQUEST_MAX_LENGTH);
	VERIFY(bytes_read != -1, "read from client socket failed");
	if (bytes_read == REQUEST_MAX_LENGTH) {
		PROPAGATE(send_response(client_fd, 414, "Request-URI Too Large", "Request to long"));
		success = TRUE;
		goto end;
	}
	buffer[bytes_read] = '\0';
	PRINTF("request: %s", buffer);

	char* path;
	ParseResult parse_result = parse_request(buffer, &path);
	switch (parse_result)
	{
	case PARSE_SUCCESS:
		break;
	case PARSE_BAD_REQUEST:
		PROPAGATE(send_response(client_fd, 400, "Bad Request", "Bad Request"));
		success = TRUE;
		goto end;
	case PARSE_UNSUPPORTED_REQUEST:
		PROPAGATE(
				send_response(client_fd, 501, "Not Implemented",
						"Request method not implemented"));
		success = TRUE;
		goto end;
	default:
		assert(FALSE);
	}

	//Note: as the instructions specified, post requests are handled just like get requests.
	PROPAGATE(handle_get_request(client_fd, path));

	success = TRUE;
end:
	if (buffer != NULL) free(buffer);
	return success;
}

ParseResult parse_request(char* request, char** path)
{
	char* s = request;

	// Parse request method
	char* method_end = strchr(s, ' ');
	if (method_end == NULL) {
		return PARSE_BAD_REQUEST;
	}
	*method_end = '\0';
	char* method = s;
	s = method_end + 1;

	// Check method
	if (is_string_in_array(method, UNSUPPORTED_HTTP_METHODS,
			ARRAY_LENGTH(UNSUPPORTED_HTTP_METHODS))) {
		return PARSE_UNSUPPORTED_REQUEST;
	}
	if (!is_string_in_array(method, SUPPORTED_HTTP_METHODS,
			ARRAY_LENGTH(SUPPORTED_HTTP_METHODS))) {
		return PARSE_BAD_REQUEST;
	}

	// Parse requested path
	char* path_end = strchr(s, ' ');
	if (path_end == NULL) {
		return PARSE_BAD_REQUEST;
	}
	*path_end = '\0';
	*path = s;
	s = path_end + 1;

	return PARSE_SUCCESS;
}

bool is_string_in_array(const char* string, const char** string_array, int array_length)
{
	for (int i = 0; i < array_length; ++i)
	{
		if (strcmp(string, string_array[i]) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

bool handle_get_request(int client_fd, const char* path)
{
	bool success = FALSE;
	struct stat path_stat;
	PRINTF("Get (%s)\n", path);

	if (stat(path, &path_stat) == -1) {
		if (errno == ENOENT || errno == ENOTDIR) {
			//TODO: return simple HTML body?
			PROPAGATE(send_response(client_fd, 404, "Not Found", "Requested file/directory not found."));
			success = TRUE;
		} else if (errno == EACCES) {
			PROPAGATE(send_response(client_fd, 403, "Forbidden", "Access denied."));
			success = TRUE;
		} else {
			perror("stat reqeusted file failed");
		}
		goto end;
	}

	if (S_ISDIR(path_stat.st_mode)) {
		PROPAGATE(send_dir_list(client_fd, path));
	} else {
		//TODO: is this the right thing to do?
		//      should check S_ISREG(path_stat.st_mode)?
		PROPAGATE(send_file(client_fd, path));
	}

	success = TRUE;
end:
	return success;
}

bool send_file(int client_fd, const char* file_path)
{
	bool success = FALSE;
	int fd = -1;
	char* buffer = NULL;
	PRINTF("Get (%s)\n", file_path);

	fd = open(file_path, O_RDONLY);
	if (fd == -1) {
		if (errno == ENOENT || errno == ENOTDIR) {
			PROPAGATE(send_response(client_fd, 404, "Not Found", "Requested file/directory not found."));
			success = TRUE;
		} else if (errno == EACCES) {
			PROPAGATE(send_response(client_fd, 403, "Forbidden", "Access denied."));
			success = TRUE;
		} else {
			perror("open requested file failed");
		}
		goto end;
	}
	VERIFY(fd != -1, "open requested file failed");

	buffer = (char*)malloc(MEGA);
	VERIFY(buffer != NULL, "malloc failed");
	int bytes_read = 0;
	PRINTF("Send file\n");
	PROPAGATE(send_status_line(client_fd, 200, "OK"));
	while (TRUE)
	{
		bytes_read = read(fd, buffer, MEGA);
		VERIFY(bytes_read != -1, "read requested file failed");
		if (bytes_read == 0) {
			break;
		}
		PROPAGATE(write_all(client_fd, buffer, bytes_read));
	}

	success = TRUE;
end:
	if (buffer != NULL) free(buffer);
	if (fd != -1) close(fd);
	return success;
}

//TODO: send HTML body (?)
//TODO: send in MEGA chunks, and not line by line (?)
bool send_dir_list(int client_fd, const char* dir_path)
{
	bool success = FALSE;
	DIR *dir = NULL;
	struct dirent* buffer = NULL;

	dir = opendir(dir_path);
	if (dir == NULL) {
		if (errno == ENOENT) {
			PROPAGATE(send_response(client_fd, 404, "Not Found", "Requested file/directory not found."));
			success = TRUE;
		} else if (errno == EACCES) {
			PROPAGATE(send_response(client_fd, 403, "Forbidden", "Access denied."));
			success = TRUE;
		} else {
			perror("opendir failed");
		}
		goto end;
	}

	size_t buffer_size = dirent_buf_size(dir);
	VERIFY(buffer_size != -1, "can't determine dirent_size of readdir_r buffer");
	buffer = (struct dirent*)malloc(buffer_size);
	VERIFY(buffer != NULL, "malloc failed");

	PRINTF("Send dir list\n");
	PROPAGATE(send_status_line(client_fd, 200, "OK"));
	struct dirent* enrty;
	int readdir_error = 0;
	while ((readdir_error = readdir_r(dir, buffer, &enrty)) == 0 && enrty != NULL)
	{
		if (strcmp(enrty->d_name, ".") == 0
				|| strcmp(enrty->d_name, "..") == 0) {
			continue;
		}
		PROPAGATE(write_all(client_fd, enrty->d_name, strlen(enrty->d_name)));
		//TODO: XXX
		const char* new_line = "\n";
		PROPAGATE(write_all(client_fd, new_line, strlen(new_line)));
	}
	if (readdir_error != 0)
	{
		errno = readdir_error;
		VERIFY(FALSE, "readdir failed");
	}

	success = TRUE;
end:
	if (buffer != NULL) free(buffer);
	if (dir != NULL) closedir(dir);
	return success;
}

bool send_response(int fd, int status, const char* reason, char* body)
{
	bool success = FALSE;

	PRINTF("Send response (status=%d, reason=%s)\n", status, reason);
	PROPAGATE(send_status_line(fd, status, reason));
	PROPAGATE(write_all(fd, body, strlen(body)));

	success = TRUE;
end:
	return success;
}

bool send_status_line(int fd, int status, const char* reason)
{
	bool success = FALSE;
	char* status_line = NULL;
	assert(reason != NULL);
	assert(strlen(reason) <= 500);
	assert(status >= 0 && status <= 10000);

	status_line = (char*)malloc(STATUS_LINE_MAX_LENGTH);
	VERIFY(snprintf(status_line, STATUS_LINE_MAX_LENGTH - 1, "HTTP/1.1 %d %s\r\n\r\n", status, reason) > 0,
			"snprintf failed");
	PROPAGATE(write_all(fd, status_line, strlen(status_line)));

	success = TRUE;
end:
	if (status_line != NULL) free(status_line);
	return success;
}

bool write_all(int fd, const void* data, int size)
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

// Taken from: https://womble.decadent.org.uk/readdir_r-advisory.html
size_t dirent_buf_size(DIR * dirp)
{
    long name_max;
    size_t name_end;
#   if defined(HAVE_FPATHCONF) && defined(HAVE_DIRFD) \
       && defined(_PC_NAME_MAX)
        name_max = fpathconf(dirfd(dirp), _PC_NAME_MAX);
        if (name_max == -1)
#           if defined(NAME_MAX)
                name_max = (NAME_MAX > 255) ? NAME_MAX : 255;
#           else
                return (size_t)(-1);
#           endif
#   else
#       if defined(NAME_MAX)
            name_max = (NAME_MAX > 255) ? NAME_MAX : 255;
#       else
#           error "buffer size for readdir_r cannot be determined"
#       endif
#   endif
    name_end = (size_t)offsetof(struct dirent, d_name) + name_max + 1;
    return (name_end > sizeof(struct dirent)
            ? name_end : sizeof(struct dirent));
}
