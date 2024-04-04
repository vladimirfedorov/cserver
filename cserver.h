#ifndef CSERVER_H
#define CSERVER_H

#include "cjson/cJSON.h"

#ifdef __cplusplus
    extern "C" {
#endif

// Default port number
#define PORT 3000
// Default folder for static files
#define STATIC_FOLDER "static"
// Max path length
#define MAX_PATH_LEN 4096

// HTTP Status codes
// source: https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
//
// 200 OK
#define HTTP_STATUS_200 "200 OK"
// 404 Not Found
#define HTTP_STATUS_404 "404 Not Found"

// Strings
struct cstring {
    char *value;
    long length;
};
typedef struct cstring string;      // call string_free
typedef struct cstring substring;   // do not free

string string_make(const char* value);
void string_free(string str);
string read_file(const char *filename);

/**
 * Generates an HTTP response string.
 *
 * @param http_status    HTTP status code and message.
 * @param content_type   The "Content-Type" header for the response.
 * @param content        The content to include in the response body.
 *
 * @return A pointer to the dynamically allocated HTTP response string.
 *         The caller is responsible for freeing the allocated memory using free().
 *         Returns NULL if memory allocation fails.
 */
string make_response(char *http_status, const char *content_type, string content);

/**
 * Serves static files to the client over a socket connection.
 *
 * @param socket    The socket descriptor for the client connection.
 * @param filename  The name of the file to be served from the 'static' folder.
 *
 * @note Sends an HTTP response header followed by the content of the file.
 *       - sends a 200 OK response and the file's contents if the file exists in the 'static' folder,
 *       - sends a 404 Not Found response if the file does not exist.
 */
void serve_file(int socket, char *filename);

char* resource_path(char *request_path);

void add_request(cJSON *context, char *method, char *request_path, char *resource_path);

void add_object(cJSON *context, char *name, cJSON *object);

int read_int(cJSON *config, char *name, int default_value);

string render_page(cJSON *context, char *path);

const char* get_content_type(char *request_path, char *resource_path);

substring skip_metadata(string input_content, cJSON *metadata);

string render_markdown(string markdown_content);

string load_template(char* name);

int load_partial(const char *name, struct mustach_sbuf *sbuf);

string render_mustache(string template_content, cJSON *context);

#ifdef __cplusplus
    }  /* extern "C" { */
#endif

#endif  /* CSERVER_H */
