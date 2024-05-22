#ifndef CSERVER_H
#define CSERVER_H

#include <stdbool.h>
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


// Strings ////////////////////////////////////////////////////////////////////


struct cstring {
    char *value;
    size_t length;
};
typedef struct cstring string;      // call string_free
typedef struct cstring substring;   // do not free

/**
 * Initializes a string structure with default values.
 *
 * Returns a string object with the value set to NULL and length set to 0.
 */
string string_init();

/**
 * Creates and returns a string object initialized
 * with the content from a null-terminated string.
 * 
 * Parameters:
 *  - value        A pointer to a null-terminated C string.
 * 
 * Returns a string object with the value field pointing to a newly allocated
 * memory containing a copy of the input string, and length set
 * to the length of the input string.
 */
string string_make(const char* value);

/**
 * Frees the memory allocated for the string object's value field.
 * 
 * Parameters:
 *  - str          The string object whose memory needs to be freed.
 */
void string_free(string str);

/**
 * Reads the entire content of a file specified by the filename
 * into a string object.
 * 
 * Parameters:
 *  - filename     Path to the file to be read.
 * 
 * Returns a string object containing the contents of the file. 
 * If the file cannot be opened, or other errors occur during reading,
 * the function returns an empty `string` object.
 */
string read_file(const char *filename);


// Server management functions ////////////////////////////////////////////////


/**
 * Prints the available CLI commands.
 * 
 * Returns EXIT_SUCCESS.
 */
int print_help();

/**
 * Starts a new instance of `cserver` at path
 * 
 * Parameters:
 *  - path         Path to the server files
 *  - cli_mode     `false` starts the server as a service
 *                 `true` runs it as a CLI app in terminal; closing `cserver`
 *                 with Ctrl+C will stop it.
 * 
 * Returns EXIT_SUCCESS on successful execution; EXIT_FAILURE if the command
 * execution fails.
 */
int start_server(char* path, bool cli_mode);

/**
 * Lists the process IDs of all running instances of `cserver`.
 * 
 * Returns EXIT_SUCCESS on successful execution; EXIT_FAILURE if the command
 * execution fails.
 */
int list_servers();

/**
 * Restarts a running instance of `cserver` at the provided path.
 * 
 * Parameters:
 *  - path         Path to the server files.
 * 
 * Returns EXIT_SUCCESS on successful execution; EXIT_FAILURE if the command
 * execution fails.
 */
int restart_server(char *path);

/**
 * Stops a running `cserver` service with the provided PID.
 * 
 * Parameters:
 *  - id           Process ID
 * 
 * Returns EXIT_SUCCESS on successful execution; EXIT_FAILURE if the command
 * execution fails.
 */
int stop_server(char *id);

/**
 * Collects Markdown metadata and stores it into the provided `metadata`
 * cJSON object.
 * 
 * Parameters:
 *  - path         Root directory of the web server
 *  - metadata     `cJSON` object to store metadata values to.
 */
void collect_metadata(char *path, cJSON *metadata);

/**
 * Generates an HTTP response string.
 *
 * Parameters
 *  - http_status  HTTP status code and message.
 *  - content_type The "Content-Type" header for the response.
 *  - content      The content to include in the response body.
 *
 * Returns a pointer to the dynamically allocated HTTP response string.
 * The caller is responsible for freeing the allocated memory.
 * Returns NULL if memory allocation fails.
 */
string make_response(char *http_status, const char *content_type, string content);

/**
 * Serves static files to the client over a socket connection.
 *
 * Parameters:
 *  - socket       The socket descriptor for the client connection.
 *  - filename     The name of the file to be served from the 'static' folder.
 *
 * Sends an HTTP response header followed by the content of the file.
 *  - sends a 200 OK response and the file's contents if the file exists in the 'static' folder,
 *  - sends a 404 Not Found response if the file does not exist.
 */
void serve_file(int socket, char *filename);

/**
 * Translates the request path into a resource path (i.e., full file name).
 */
char* resource_path(char *request_path);

/**
 * Adds request information into the context object
 */
void add_request(cJSON *context, char *method, char *request_path, char *resource_path);

/**
 * Reads an integer value from a cJSON object; if the value doesn't exist,
 * return the provided default value.
 * 
 * Parameters:
 *  - object          cJSON object containing the `name` key
 *  - name            value key name
 *  - default_value   default integer value
 * 
 * Returns the value for `name` key in the object; if the key is not found
 * ot the value is not a number, it returns the `default_value` value.
 */
int read_int(cJSON *object, char *name, int default_value);

/**
 * Renders a page at the path using the provided context object.
 * 
 * Parameters:
 *  - context      cJSON context object for Mustache renderer.
 *  - path         file path
 * 
 * Returns a string containing HTML of the page; returns raw file content
 * if the file is not a Markdown file or a Mustache template.
 */
string render_page(cJSON *context, char *path);

/**
 * Returns content type value for Content-Type header key.
 * 
 * Parameters:
 *  - request_path     Resource path from the request.
 *  - resource_path    Corresponding file name.
 * 
 * Returns content type value for Content-Type header key.
 */
const char* get_content_type(char *request_path, char *resource_path);


// Markdown ///////////////////////////////////////////////////////////////////


/**
 * Skips metadata and returns the pointer to markdown content inside input_content.
 * 
 * Parameters:
 * - input_content Markdown content with page metadata.
 * - metadata      cJSON object for page metadata.
 * 
 * Returns a substring pointing to the beginning of markdown content.
 * 
 * All metadata parameters are stored in cJSON object.
 * New memory is not allocated here, free input_content only.
 * 
 * Expected metadata format:
 * 
 * ---
 * key: value
 * ---
 * <new line>
 * Markdown content
 */
substring skip_metadata(string input_content, cJSON *metadata);

/**
 * Converts markdown content into HTML
 * 
 * Parameters:
 *  - markdown_content A string object containing the Markdown text
 * 
 * Returns a string object containing the resulting HTML.
 */
string render_markdown(string markdown_content);


// Mustache ///////////////////////////////////////////////////////////////////


/**
 * mustach library hook for partials, see mustach-wrap.h
 */
int load_partial(const char *name, struct mustach_sbuf *sbuf);

/**
 * Loads a Mustache template file from a specified path.
 * 
 * Parameters:
 *  - name         The name of the template file (without the extension
 *                 or path prefix) to load.
 * 
 * Returns a string object containing the content of the template file.
 */
string load_template(char* name);

/**
 * Renders a Mustache template using the provided JSON context
 * 
 * Parameters:
 *  - template_content Mustache template content string
 *  - context          A pointer to a cJSON object that provides the context
 *                     for rendering the template.
 * 
 * Returns a string object containing the rendered output.
 */
string render_mustache(string template_content, cJSON *context);


#ifdef __cplusplus
    }  /* extern "C" { */
#endif

#endif  /* CSERVER_H */
