#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>

// Markdown
#include "md4c/src/md4c-html.h"

// Templates
#include "mustach/mustach-cjson.h"

#include "cserver.h"

// Constants
const char *content_type_text = "text/plain";
const char *content_type_html = "text/html";
const char *content_type_json = "application/json";

int main(int argc, char **argv) {

    // Request data buffer length
    const size_t buffer_len = 4096;
    // HTTP method length
    const size_t method_len = 8;
    // Request url length
    const size_t url_len = 1024;

    // Read configuration
    cJSON *config = NULL;
    string config_content = read_file("config.json");
    if (config_content.value != NULL) {
        config = cJSON_Parse(config_content.value);
        string_free(config_content);
    }
    int port = read_int(config, "port", PORT);

    // Create socket descriptor
    // man socket(2)
    int server_desc = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);

    // Define the server address
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

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

    // mustach library hook for partials, see mustach-wrap.h
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

        cJSON *context = cJSON_CreateObject();
        add_request(context, method, url, path);
        add_object(context, "config", config);

        string content;
        string response;

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
                string not_found = string_make("File not found.");
                response = make_response(HTTP_STATUS_404, "text/plain", not_found);
                string_free(not_found);
            }
        }

        cJSON_free(context);

        int send_result = send(socket_desc, response.value, response.length, 0);
        if (send_result < 0) {
            perror("send failed.");
            exit(EXIT_FAILURE);
        }
        string_free(content);
        string_free(response);

        // Close the connection
        close(socket_desc);
    }

    return 0;
}

/**
 * String Functions
 */

string string_make(const char* value) {
    size_t value_length = strlen(value);
    string result = { .value = malloc(value_length + 1), .length = value_length };
    strcpy(result.value, value);
    return result;
}

void string_free(string str) {
    if (str.value) free(str.value);
}

string read_file(const char *filename) {
    string result = { .value = NULL, .length = 0};

    FILE *file = fopen(filename, "rb");
    if (file == NULL)  return result;
    
    // File size
    fseek(file, 0, SEEK_END);
    long file_length = ftell(file);
    if (file_length == -1) {
        perror("Failed to determine file size");
        fclose(file);
        return result;
    }
    rewind(file);

    // Allocate memory for file content
    char *content = (char *)malloc(file_length + 1); // +1 for the null terminator
    if (content == NULL) {
        perror("Failed to allocate memory");
        fclose(file);
        return result;
    }

    // Read file into memory
    if (fread(content, sizeof(char), file_length, file) < file_length) {
        perror("Failed to read the file");
        free(content);
        fclose(file);
        return result;
    }
    content[file_length] = '\0'; // Null-terminate the string

    // Close the file
    fclose(file);

    result.value = content;
    result.length = file_length;

    return result;
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
string make_response(char *http_status, const char *content_type, string content) {
    string result = { .value = NULL, .length = 0 };
    // Calculate the lengths of various parts of the HTTP response
    // CRLF is the standard line break (https://www.w3.org/MarkUp/html-spec/html-spec_8.html#SEC8.2.1)
    int status_line_length = snprintf(NULL, 0, "HTTP/1.1 %s\r\n", http_status);
    int content_type_length = snprintf(NULL, 0, "Content-Type: %s\r\n", content_type);
    int content_length_length = snprintf(NULL, 0, "Content-Length: %li\r\n", content.length);
    
    // Calculate the total length of the HTTP response
    // + 2 for \r\n before content
    int total_length = status_line_length + content_type_length + content_length_length + 2 + content.length; 
    
    // Allocate memory for the complete HTTP response
    char *response = (char*)malloc(total_length);
    if (response == NULL) {
        return result;
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
    sprintf(content_length_str, "Content-Length: %li\r\n", content.length);
    strcat(response, content_length_str);
    // Empty line to separate headers from the content
    strcat(response, "\r\n"); 
    // Content
    strcat(response, content.value);

    result.value = response;
    result.length = strlen(response);
    return result;
}


int file_exists(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

char* resource_path(char* request_path) {
    static char filename[MAX_PATH_LEN];
    int filenameLength = snprintf(filename, sizeof(filename), "static%s", request_path);

    // Check if the file exists as is
    if (file_exists(filename)) {
        return filename;
    }

    // Check if a directory exists with an index file
    snprintf(filename + filenameLength, sizeof(filename) - filenameLength, "/index.html");
    if (file_exists(filename)) {
        return filename;
    }

    snprintf(filename + filenameLength, sizeof(filename) - filenameLength, "/index.md");
    if (file_exists(filename)) {
        return filename;
    }

    // Reset the filename back to before checking for index files
    filename[filenameLength] = '\0';

    // Try appending .html and .md to the original URL
    snprintf(filename + filenameLength, sizeof(filename) - filenameLength, ".html");
    if (file_exists(filename)) {
        return filename;
    }

    snprintf(filename + filenameLength, sizeof(filename) - filenameLength, ".md");
    if (file_exists(filename)) {
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

void add_request(cJSON *context, char *method, char *request_path, char *resource_path) {
    cJSON *request = cJSON_CreateObject();
    cJSON *request_method = cJSON_CreateString(method);
    cJSON_AddItemToObject(request, "method", request_method);
    cJSON *request_request_path = cJSON_CreateString(request_path);
    cJSON_AddItemToObject(request, "query", request_request_path);
    cJSON *request_resource_path = cJSON_CreateString(resource_path);
    cJSON_AddItemToObject(request, "resourcePath", request_resource_path);
    cJSON_AddItemToObject(context, "request", request);
}

void add_object(cJSON *context, char *name, cJSON *object) {
    cJSON *o = object;
    if (o == NULL) {
        o = cJSON_CreateObject();
    }
    cJSON_AddItemToObject(context, name, o);
}

int read_int(cJSON *config, char *name, int default_value) {
    cJSON *object = cJSON_GetObjectItem(config, name);
    if (object == NULL) return default_value;
    double value = cJSON_GetNumberValue(object);
    if (isnan(value)) return default_value;
    return (int)value; 
}

string render_page(cJSON *context, char *path) {

    string file_content = read_file(path);
    if (file_content.value == NULL) return file_content;

    if (strends(path, ".md") == 0) {
        
        // Render markdown file
        cJSON *page_metadata = cJSON_CreateObject();
        substring markdown_content = skip_metadata(file_content, page_metadata);
        cJSON_AddItemToObject(context, "page", page_metadata);
        string md_html_content = render_markdown(markdown_content);
        string_free(file_content);
        
        // context.content = rendered markdown data
        cJSON *content = cJSON_CreateString(md_html_content.value);
        cJSON_AddItemToObject(context, "content", content);
        string_free(md_html_content);

        // mustache template for the file
        char *template_name = "default";
        cJSON *page_template_object = cJSON_GetObjectItem(page_metadata, "template");
        if (page_template_object) {
            template_name = page_template_object->valuestring;
        }
        string template = load_template(template_name);
        if (template.value == NULL) {
            template = string_make("{{{content}}}");
        }

        // Render mustache template with the provided content
        string html_content = render_mustache(template, context);
        string_free(template);
        if (page_metadata) cJSON_free(page_metadata);
        if (page_template_object) cJSON_free(page_template_object);
        if (content) cJSON_free(content);

        return html_content;

    } else if (strends(path, ".mustache") == 0) {
        
        // Render mustach file
        string rendered_content = render_mustache(file_content, context);
        if (file_content.value) free(file_content.value);
        return rendered_content;

    } else {
        
        // Send raw file data
        return file_content;

    }
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
substring skip_metadata(string input_content, cJSON *metadata) {
    substring result = { .value = input_content.value, .length = input_content.length };
    // Check if the first line is ---
    if (strncmp(result.value, "---\n", 4) != 0) {
        // If not, return the original input_content
        return result;
    } else {
        // Skip the first line (---)
        char *line = result.value + 4;
        char *next_line = NULL;

        // TODO: Skip commented (#) lines
        while ((next_line = strstr(line, "\n")) != NULL) {
            // Check for the end of the metadata section
            if (line == next_line) {
                result.value = next_line + 1;
                result.length -= result.value - input_content.value;
                // Return the pointer to the beginning of markdown content
                printf("input_content.length = %li, result.length = %li\n", input_content.length, result.length);
                return result;
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
        // If we are here, it means the page has no content, just metadata.
        result.length = 0;
        return result;
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

string render_markdown(string markdown_content) {
    html_buffer buf = {0};
    string result = { .value = NULL, .length = 0 };

    // Parse Markdown to HTML
    if (md_html(markdown_content.value, markdown_content.length, output_callback, &buf, MD_DIALECT_GITHUB, 0) != 0) {
        // Handle parsing error
        if (buf.output) free(buf.output);
        return result;
    }

    result.value = buf.output;
    result.length = buf.size;
    return result;
}

/*
 * Mustache templates
 */

int load_partial(const char *name, struct mustach_sbuf *sbuf) {
    // Example of opening a file named after the partial. Adjust path as necessary.
    char filename[MAX_PATH_LEN];
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

string load_template(char* name) {
    char filename[MAX_PATH_LEN];
    snprintf(filename, sizeof(filename), "templates/%s.mustache", name);
    string template = read_file(filename);
    if (template.value == NULL) {
        printf("Tempalte %s not found.\n", filename);
    }
    return template;
}

string render_mustache(string template_content, cJSON *context) {
    string result = { .value = NULL, .length = 0 };

    char* output = NULL;
    size_t output_size = 0;
    FILE* output_stream = open_memstream(&output, &output_size);

    // Perform the mustach processing
    printf("Rendering template...\n");
    int ret = mustach_cJSON_file(template_content.value, template_content.length, context, Mustach_With_AllExtensions, output_stream);
    fflush(output_stream);
    fclose(output_stream);

    // Check for errors in mustach processing
    if (ret != MUSTACH_OK) {
        fprintf(stderr, "Mustach processing error: %d\n", ret);
        if (output) free(output);
    } else {
        result.value = output;
        result.length = output_size;
    }

    return  result;
}
