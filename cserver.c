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


/**
 * Generates an HTTP response string.
 *
 * @param status_code    HTTP status code.
 * @param status_message HTTP status message.
 * @param content_type   The "Content-Type" header for the response.
 * @param content        The content to include in the response body.
 *
 * @return A pointer to the dynamically allocated HTTP response string.
 *         The caller is responsible for freeing the allocated memory using free().
 *         Returns NULL if memory allocation fails.
 */
char* make_response(int status_code, char *status_message, char *content_type, char *content);

char* render_md(char *md_content, size_t md_length);

char* render_mustache(char *template_content, size_t template_length);

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

cJSON* make_context();

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
        // snprintf(filename, sizeof(filename), "%s/%s", STATIC_FOLDER, path);
        if (path != NULL) {
            printf("200 OK\n- request_path: %s\n- resource_path: %s\n", url, path);
            serve_file(socket_desc, path);
        } else {
            printf("404 Not found\n- request_path: %s\n- resource_path: %s\n", url, path);
            char *response = make_response(404, "File not found.", "text/plain", "File not found.");
            int send_result = send(socket_desc, response, strlen(response), 0);
            if (send_result < 0) {
                perror("send failed.");
                exit(EXIT_FAILURE);
            }
        }

		// Send the response
        // char *response = make_response(200, "OK", "text/plain", "Hello!");
		// int send_result = send(socket_desc, response, strlen(response), 0);
        // if (send_result < 0) {
        //     perror("send failed.");
        //     exit(EXIT_FAILURE);
        // }

        // Close the connection
        close(socket_desc);
    }

    return 0;
}

/**
 * Generates an HTTP response string.
 *
 * @param status_code    HTTP status code.
 * @param status_message HTTP status message.
 * @param content_type   The "Content-Type" header for the response.
 * @param content        The content to include in the response body.
 *
 * @return A pointer to the dynamically allocated HTTP response string.
 *         The caller is responsible for freeing the allocated memory using free().
 *         Returns NULL if memory allocation fails.
 */
char* make_response(int status_code, char *status_message, char *content_type, char *content) {
    // Calculate the lengths of various parts of the HTTP response
    // CRLF is the standard line break (https://www.w3.org/MarkUp/html-spec/html-spec_8.html#SEC8.2.1)
    int content_length = strlen(content);
    int status_line_length = snprintf(NULL, 0, "HTTP/1.1 %d %s\r\n", status_code, status_message);
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
    // HTTP
    snprintf(response, total_length, "HTTP/1.1 %d %s\r\n", status_code, status_message);
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

cJSON* make_context() {
    return cJSON_Parse("{}");
}

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
void serve_file(int socket, char *filename) {
    char response_header[1024];
    char buffer[1024];
    ssize_t bytes_read;
    int file_fd;

    // Open the requested file
    file_fd = open(filename, O_RDONLY);

    if (file_fd == -1) {
        // If the file doesn't exist, return a 404 response
        snprintf(response_header, sizeof(response_header), "HTTP/1.1 404 Not Found\r\n\r\n");
        send(socket, response_header, strlen(response_header), 0);
    } else {
        // If the file exists, send a 200 OK response and the file's contents
        snprintf(response_header, sizeof(response_header), "HTTP/1.1 200 OK\r\n\r\n");
        send(socket, response_header, strlen(response_header), 0);

        if (strlen(filename) >= 3 && strcmp(filename + strlen(filename) - 3, ".md") == 0) {
            // Render markdown file
            char* markdown_content = NULL;
            size_t markdown_length = 0;

            while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
                markdown_content = realloc(markdown_content, markdown_length + bytes_read + 1);
                memcpy(markdown_content + markdown_length, buffer, bytes_read);
                markdown_length += bytes_read;
            }

            markdown_content[markdown_length] = '\0';

            char* html_content = render_md(markdown_content, markdown_length);
            size_t html_length = strlen(html_content);
            send(socket, "<!DOCTYPE html>", 15, 0);
            send(socket, html_content, strlen(html_content), 0);
            if (html_content) free(html_content);
            if (markdown_content) free(markdown_content);
        } else if (strlen(filename) >= 9 && strcmp(filename + strlen(filename) - 9, ".mustache") == 0) {
            // Render mustach file
            char* template_content = NULL;
            size_t template_length = 0;

            while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
                template_content = realloc(template_content, template_length + bytes_read + 1);
                memcpy(template_content + template_length, buffer, bytes_read);
                template_length += bytes_read;
            }

            template_content[template_length] = '\0';

            char *html_content = render_mustache(template_content, template_length);
            send(socket, html_content, strlen(html_content), 0);
            if (html_content) free(html_content);
            if (template_content) free(template_content);
        } else {
            // Send raw file data
            while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
                send(socket, buffer, bytes_read, 0);
            }
        }
        close(file_fd);
    }
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

char* render_md(char *md_content, size_t md_length) {
    printf("Markdown %lu bytes:\n%s", md_length, md_content);

    html_buffer buf = {0};

    // Parse Markdown to HTML
    if (md_html(md_content, md_length, output_callback, &buf, 0, 0) != 0) {
        // Handle parsing error
        if (buf.output) free(buf.output);
        return NULL;
    }

    return buf.output; // Return the HTML output
}

char* render_mustache(char *template_content, size_t template_length) {
    printf("Temaplte %lu bytes:\n%s", template_length, template_content);

    cJSON *root = cJSON_Parse("{\"content\": \"<b>Hello</b> there!\"}");

    // Render the template into a dynamic string (buffer growing as needed)
    char* output = NULL;
    size_t output_size = 0;
    FILE* output_stream = open_memstream(&output, &output_size);

    // Perform the mustach processing
    printf("Rendering template...\n");
    int ret = mustach_cJSON_file(template_content, template_length, root, Mustach_With_AllExtensions, output_stream);
    fflush(output_stream);
    fclose(output_stream);

    free(root);

    // Check for errors in mustach processing
    if (ret != MUSTACH_OK) {
        fprintf(stderr, "Mustach processing error: %d\n", ret);
        if (output) free(output);
        return template_content;
    }

    return  output;
}
