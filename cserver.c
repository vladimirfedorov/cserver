#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

// Markdown
#include "md4c/src/md4c-html.h"

// Templates
#include "cjson/cJSON.h"
#include "mustach/mustach-cjson.h"

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

// Constants
const char *content_type_text = "text/plain";
const char *content_type_html = "text/html";
const char *content_type_json = "application/json";

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
char* make_response(char *http_status, const char *content_type, char *content);

char* skip_metadata(char *input_content, cJSON *metadata);

char* render_md(char *md_content, size_t md_length);

char* load_template(char *name);

int load_partial(const char *name, struct mustach_sbuf *sbuf);

char* render_mustache(char *template_content, size_t template_length, cJSON *context);

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

cJSON* make_context(char *method, char *request_path, char *resource_path);

char* render_page(cJSON *context, char *path);

const char* get_content_type(char *request_path, char *resource_path);

int main(int argc, char **argv) {

    // Request data buffer length    
    const size_t buffer_len = 4096;
    // HTTP method length
    const size_t method_len = 8;
    // Request url length
    const size_t url_len = 1024;

    // Create socket descriptor
    // man socket(2)
    int server_desc = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);

    // Define the server address
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    socklen_t address_len = sizeof(address);

    // Bind the socket to the network address and port
    int bind_result = bind(server_desc, (struct sockaddr *)&address, address_len);
    if (bind_result != 0) {
        perror("bind failed");
        return 1;
    }

    // Listen for connections
    int listen_result = listen(server_desc, 3);
    if (listen_result != 0) {
        perror("listen failed");
        return 1;
    }

    mustach_wrap_get_partial = load_partial;

    while (1) {
        printf("Waiting for a connection...\n");
        
        // Accept a connection
        int socket_desc = accept(server_desc, (struct sockaddr *)&address, &address_len);
        if (socket_desc < 0) {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }

        // Parse the request and send a response
        char buffer[buffer_len] = {0};
		long recv_result = recv(socket_desc, buffer, buffer_len, 0);
        if (recv_result < 0) {
            perror("recv failed");
            exit(EXIT_FAILURE);
        }
        printf("%li bytes received\n", recv_result);
        printf("--------------------------------\n");
        printf("%s\n", buffer);
        printf("--------------------------------\n");

		// Parse the request
		char method[method_len];
		char url[url_len];
		sscanf(buffer, "%s %s", method, url);
		printf("Method: %s\nURL: %s\n", method, url);
        char filename[url_len];
        char *path = resource_path(url);

        cJSON *context = make_context(method, url, path);

        char *content;
        char *response;

        if (path != NULL) {
            const char *content_type = get_content_type(url, path);
            content = render_page(context, path);
            response = make_response(HTTP_STATUS_200, content_type, content);
        } else {
            char *page_404_path = resource_path("/404");
            if (page_404_path != NULL) {
                const char *content_type = get_content_type(url, page_404_path);
                content = render_page(context, page_404_path);
                response = make_response(HTTP_STATUS_404, content_type, content);
            } else {
                response = make_response(HTTP_STATUS_404, "text/plain", "File not found.");
            }
        }

        cJSON_free(context);

        int send_result = send(socket_desc, response, strlen(response), 0);
        if (send_result < 0) {
            perror("send failed.");
            exit(EXIT_FAILURE);
        }
        if (content) free(content);
        if (response) free(response);

        // Close the connection
        close(socket_desc);
    }

    return 0;
}

/**
 * Generates an HTTP response string.
 *
 * @param hhtp_status    HTTP status code and message.
 * @param content_type   The "Content-Type" header for the response.
 * @param content        The content to include in the response body.
 *
 * @return A pointer to the dynamically allocated HTTP response string.
 *         The caller is responsible for freeing the allocated memory using free().
 *         Returns NULL if memory allocation fails.
 */
char* make_response(char *http_status, const char *content_type, char *content) {
    // Calculate the lengths of various parts of the HTTP response
    // CRLF is the standard line break (https://www.w3.org/MarkUp/html-spec/html-spec_8.html#SEC8.2.1)
    int content_length = strlen(content);
    int status_line_length = snprintf(NULL, 0, "HTTP/1.1 %s\r\n", http_status);
    int content_type_length = snprintf(NULL, 0, "Content-Type: %s\r\n", content_type);
    int content_length_length = snprintf(NULL, 0, "Content-Length: %d\r\n", content_length);
    
    // Calculate the total length of the HTTP response
    // + 2 for \r\n before content
    int total_length = status_line_length + content_type_length + content_length_length + 2 + content_length; 
    
    // Allocate memory for the complete HTTP response
    char *response = (char*)malloc(total_length);
    if (response == NULL) {
        return NULL;
    }

    // Construct the HTTP response
    // HTTP Status line
    snprintf(response, total_length, "HTTP/1.1 %s\r\n", http_status);
    // Content-Type
    strcat(response, "Content-Type: ");
    strcat(response, content_type);
    strcat(response, "\r\n");
    // Content-Length
    char content_length_str[content_length_length];
    sprintf(content_length_str, "Content-Length: %d\r\n", content_length);
    strcat(response, content_length_str);
    // Empty line to separate headers from the content
    strcat(response, "\r\n"); 
    // Content
    strcat(response, content);

    return response;    
}


int fileExists(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

char* resource_path(char* request_path) {
    static char filename[MAX_PATH_LEN];
    int filenameLength = snprintf(filename, sizeof(filename), "static%s", request_path);

    // Check if the file exists as is
    if (fileExists(filename)) {
        return filename;
    }

    // Check if a directory exists with an index file
    snprintf(filename + filenameLength, sizeof(filename) - filenameLength, "/index.html");
    if (fileExists(filename)) {
        return filename;
    }

    snprintf(filename + filenameLength, sizeof(filename) - filenameLength, "/index.md");
    if (fileExists(filename)) {
        return filename;
    }

    // Reset the filename back to before checking for index files
    filename[filenameLength] = '\0';

    // Try appending .html and .md to the original URL
    snprintf(filename + filenameLength, sizeof(filename) - filenameLength, ".html");
    if (fileExists(filename)) {
        return filename;
    }

    snprintf(filename + filenameLength, sizeof(filename) - filenameLength, ".md");
    if (fileExists(filename)) {
        return filename;
    }

    return NULL;
}

int strends(char *str, char *suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    return strcmp(str + str_len - suffix_len, suffix);
}

// Trims string in place; 
// works with allocated strings
void trim(char *str) {
    char *p = str;
    int len = strlen(p);
    if (len == 0) return;

    while(isspace(p[len - 1])) p[--len] = 0;
    while(*p && isspace(*p)) ++p, --len;

    memmove(str, p, len + 1);
}

const char* get_content_type(char *request_path, char *resource_path) {
    if (strends(resource_path, ".md") == 0 ||
        strends(resource_path, ".html") == 0 ||
        strends(resource_path, ".mustache") == 0) {
        return content_type_html;
    } else if (strends(resource_path, ".json") == 0) {
        return content_type_json;
    } else {
        return content_type_text;
    }
}

cJSON* make_context(char *method, char *request_path, char *resource_path) {
    cJSON *context = cJSON_CreateObject();
    // request
    cJSON *request = cJSON_CreateObject();
    cJSON *request_method = cJSON_CreateString(method);
    cJSON_AddItemToObject(request, "method", request_method);
    cJSON *request_request_path = cJSON_CreateString(request_path);
    cJSON_AddItemToObject(request, "query", request_request_path);
    cJSON *request_resource_path = cJSON_CreateString(resource_path);
    cJSON_AddItemToObject(request, "resourcePath", request_resource_path);
    cJSON_AddItemToObject(context, "request", request);
    return context;
}

char* render_page(cJSON *context, char *path) {

    char buffer[1024];
    ssize_t bytes_read;

    int file_fd;
    // Open the requested file
    file_fd = open(path, O_RDONLY);
    
    if (file_fd == -1) {
        return NULL;
    } 

    if (strends(path, ".md") == 0) {
        // Render markdown file
        char *file_content = NULL;
        size_t file_length = 0;

        while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
            file_content = realloc(file_content, file_length + bytes_read + 1);
            memcpy(file_content + file_length, buffer, bytes_read);
            file_length += bytes_read;
        }
        file_content[file_length] = '\0';

        cJSON *page_metadata = cJSON_CreateObject();
        char *markdown_content = skip_metadata(file_content, page_metadata);
        cJSON_AddItemToObject(context, "page", page_metadata);
        char *md_html_content = render_md(markdown_content, strlen(markdown_content));
        if (file_content) free(file_content);

        cJSON *content = cJSON_CreateString(md_html_content);
        cJSON_AddItemToObject(context, "content", content);
        if (md_html_content) free(md_html_content);

        char *template_name = "default";
        cJSON *page_template_object = cJSON_GetObjectItem(page_metadata, "template");
        if (page_template_object) {
            template_name = page_template_object->valuestring;
        }
        char *template_content = load_template(template_name);
        char *default_temaplte = "{{{content}}}";
        char *html_content;
        if (template_content) {
            html_content = render_mustache(template_content, strlen(template_content), context);
        } else {
            html_content = render_mustache(default_temaplte, strlen(default_temaplte), context);
        }
        if (template_content) free(template_content);
        if (page_metadata) cJSON_free(page_metadata);
        if (page_template_object) cJSON_free(page_template_object);
        if (content) cJSON_free(content);

        return html_content;

    } else if (strends(path, ".mustache") == 0) {
        // Render mustach file
        char* template_content = NULL;
        size_t template_length = 0;

        while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
            template_content = realloc(template_content, template_length + bytes_read + 1);
            memcpy(template_content + template_length, buffer, bytes_read);
            template_length += bytes_read;
        }

        template_content[template_length] = '\0';

        char *html_content = render_mustache(template_content, template_length, context);
        if (template_content) free(template_content);
        return html_content;

    } else {
        // Send raw file data
        char* raw_content = NULL;
        size_t raw_length = 0;
        while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
            raw_content = realloc(raw_content, raw_length + bytes_read + 1);
            memcpy(raw_content + raw_length, buffer, bytes_read);
            raw_length += bytes_read;
        }
        return raw_content;
    }
    close(file_fd);
}

// Markdown renderer

typedef struct {
    char *output;
    size_t size;
} html_buffer;

// Callback function to append the output HTML
void output_callback(const MD_CHAR* text, MD_SIZE size, void* userdata) {
    html_buffer *buf = (html_buffer *)userdata;
    char *new_output = realloc(buf->output, buf->size + size + 1); // +1 for null terminator
    if (!new_output) {
        // Handle allocation failure
        return;
    }
    memcpy(new_output + buf->size, text, size); // Append new HTML fragment
    buf->output = new_output;
    buf->size += size;
    buf->output[buf->size] = '\0'; // Ensure null-termination
}

/**
 * Skips metadata and returns the pointer to markdown content inside input_content.
 * 
 * @param input_content     Markdown content with page metadata.
 * @param metadata          cJSON object for page metadata.
 * 
 * @return  A pointer to the beginning of markdown content.
 * 
 * All metadata parameters are stored in cJSON object.
 * New memory is not allocated here, free input_content only.
 * 
 * Expected metadata format:
 * ---
 * key: value
 * ---
 * <new line>
 * Markdown content
 */
char* skip_metadata(char *input_content, cJSON *metadata) {
    // Check if the first line is ---
    if (strncmp(input_content, "---\n", 4) != 0) {
        // If not, return the original input_content
        return input_content;
    } else {
        // Skip the first line (---)
        char *line = input_content + 4;
        char *next_line = NULL;

        // TODO: Skip commented (#) lines
        while ((next_line = strstr(line, "\n")) != NULL) {
            // Check for the end of the metadata section
            if (line == next_line) {
                // Return the pointer to the beginning of markdown content
                return next_line + 1;
            }

            *next_line = '\0';
            char *colon = strchr(line, ':');
            if (colon != NULL) {
                *colon = '\0';      // change to simplify memory management 
                char *key = line;
                char *value = colon + 1;
                trim(key);
                trim(value);
                cJSON_AddStringToObject(metadata, key, value);
                *colon = ':';       // restore back
            }
            *next_line = '\n';
            line = next_line + 1;
        }
    }
    // If we are here, it means the page has no content, just metadata.
    // Not sure how to proceed with that.
    // Return all page content for now.
    return input_content;
}

char* render_md(char *md_content, size_t md_length) {
    printf("Markdown %lu bytes:\n%s", md_length, md_content);

    html_buffer buf = {0};

    // Parse Markdown to HTML
    if (md_html(md_content, md_length, output_callback, &buf, MD_DIALECT_GITHUB, 0) != 0) {
        // Handle parsing error
        if (buf.output) free(buf.output);
        return NULL;
    }

    return buf.output; // Return the HTML output
}

// Mustache templates

int load_partial(const char *name, struct mustach_sbuf *sbuf) {
    // Example of opening a file named after the partial. Adjust path as necessary.
    char filename[256];
    snprintf(filename, sizeof(filename), "templates/partials/%s.mustache", name);
    FILE *file = fopen(filename, "r");
    
    if (!file) {
        // Return an error if the file cannot be opened
        printf("Partial %s not found.\n", filename);
        return -1;
    }
    
    // Seek to the end to find the file size
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);  // Rewind to the start
    
    // Allocate and read the file content
    sbuf->value = malloc(fsize + 1);
    fread((char *)sbuf->value, 1, fsize, file);
    fclose(file);
    
    // Null-terminate and set size
    ((char *)sbuf->value)[fsize] = '\0';
    sbuf->length = fsize;
    
    return 0;
}

char* load_template(char *name) {
    char filename[256];
    snprintf(filename, sizeof(filename), "templates/%s.mustache", name);
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Tempalte %s not found.\n", filename);
        return  NULL;
    }
    // Seek to the end to find the file size
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);  // Rewind to the start
    
    // Allocate and read the file content
    char *template = malloc(fsize + 1);
    fread(template, 1, fsize, file);
    fclose(file);

    template[fsize] = '\0';
    return template;
}

char* render_mustache(char *template_content, size_t template_length, cJSON *context) {
    printf("Temaplte %lu bytes:\n%s", template_length, template_content);

    // Render the template into a dynamic string (buffer growing as needed)
    char* output = NULL;
    size_t output_size = 0;
    FILE* output_stream = open_memstream(&output, &output_size);

    // Perform the mustach processing
    printf("Rendering template...\n");
    int ret = mustach_cJSON_file(template_content, template_length, context, Mustach_With_AllExtensions, output_stream);
    fflush(output_stream);
    fclose(output_stream);

    // Check for errors in mustach processing
    if (ret != MUSTACH_OK) {
        fprintf(stderr, "Mustach processing error: %d\n", ret);
        if (output) free(output);
        return template_content;
    }

    return  output;
}
