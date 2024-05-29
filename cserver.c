#include <signal.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>

// Markdown
#include "md4c/src/md4c-html.h"

// Templates
#include "mustach/mustach-cjson.h"

#include "cserver.h"

// Constants
const char *content_type_text = "text/plain";
const char *content_type_html = "text/html";
const char *content_type_json = "application/json";

void collect_metadata_v2(char *path, cJSON *metadata);

#ifndef CSERVER_TEST                // Exlude main from test target
int main(int argc, char **argv) {
    if (argc < 2) {
        print_help();
    } else if (argc == 3 && strcmp(argv[1], "run") == 0) {
        return start_server(argv[2], true);
    } else if (argc == 3 && strcmp(argv[1], "start") == 0) {
        return start_server(argv[2], false);
    } else if (strcmp(argv[1], "list") == 0) {
        return list_servers();
    } else if (argc == 3 && strcmp(argv[1], "restart") == 0) {
        return restart_server(argv[2]);
    } else if (argc == 3 && strcmp(argv[1], "stop") == 0) {
        return stop_server(argv[2]);
    } else {
        print_help();
    }
    return EXIT_SUCCESS;
}
#endif

// String Functions ///////////////////////////////////////////////////////////

// Trims string in place
char * trim_whitespace(char *str) {
    char *end;
    // Leading spaces
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str; // end of line
    // trailing spaces
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    // new end of string
    *(end+1) = 0;
    return str;
}

string string_init() {
    return (string){ .value = NULL, .length = 0 };
}

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
    string result = string_init();

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

// Server management functions ////////////////////////////////////////////////

int print_help() {
    printf("Usage: \n");
    printf("  cserver run <path>    Run new server in console\n");
    printf("  cserver start <path>  Start new server at <path>\n");
    printf("  cserver restart <id>  Restart server at path\n");
    printf("  cserver list          List all servers\n");
    printf("  cserver stop <id>     Stop server with <id>\n");
    printf("  cserver               Print this help\n");
    return EXIT_SUCCESS;
}

void daemonize(char *path) {
    pid_t pid;

    // Fork off the parent process
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    // If we got a good PID, exit the parent process
    if (pid > 0) exit(EXIT_SUCCESS);

    // Change the file mode mask
    umask(0);

    // Create a new SID for the child process
    if (setsid() < 0) {
        perror("Failed SID");
        exit(EXIT_FAILURE);
    }

    // Change the current working directory
    if ((chdir(path)) < 0) {
        perror("Failed path\n");
        exit(EXIT_FAILURE);
    }

    // Close out the standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int start_server(char* path, bool cli_mode) {

    if (cli_mode) {
        chdir(path);
    } else {
        daemonize(path);
    }

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
    if (config == NULL) config = cJSON_CreateObject();

    int port = read_int(config, "port", PORT);

    // Metadata
    char pages_path[MAX_PATH_LEN];
    snprintf(pages_path, MAX_PATH_LEN, "%s/static", path);
    cJSON *site_metadata = cJSON_CreateObject();
    collect_metadata(site_metadata, pages_path, NULL);
    create_index(site_metadata);
    
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
        char buffer[buffer_len];
        memset(buffer, 0, buffer_len);
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
        char *path = resource_path(url);

        cJSON *context = cJSON_CreateObject();
        add_request(context, method, url, path);
        cJSON_AddItemToObject(context, "config", config);
        cJSON_AddItemToObject(context, "site", site_metadata);

        string response;

        if (path != NULL) {
            const char *content_type = get_content_type(url, path);
            string content = render_page(context, path);
            response = make_response(HTTP_STATUS_200, content_type, content);
            string_free(content);
        } else {
            char *page_404_path = resource_path("/404");
            if (page_404_path != NULL) {
                const char *content_type = get_content_type(url, page_404_path);
                string content = render_page(context, page_404_path);
                response = make_response(HTTP_STATUS_404, content_type, content);
                string_free(content);
            } else {
                string not_found = string_make("File not found.");
                response = make_response(HTTP_STATUS_404, content_type_text, not_found);
                string_free(not_found);
            }
        }

        // char *context_value = cJSON_Print(context);
        // printf("Context:\n%s\n", context_value);
        // free(context_value);
        cJSON_free(context);

        int send_result = send(socket_desc, response.value, response.length, 0);
        if (send_result < 0) {
            perror("send failed.\n");
            exit(EXIT_FAILURE);
        }
        string_free(response);

        // Close the connection
        close(socket_desc);
    }
}

// WARNING: Refactor the code and store running servers in a file
//          or get via IPC.
int get_pid_path(pid_t pid, char *path) {
    char command[256];
    char line[MAX_PATH_LEN];    // Buffer for command output
    char *last_arg = NULL;      // that should point to the last argument
    snprintf(command, sizeof(command), "ps -p %d -o args", pid);
    FILE *fp = popen(command, "r");
    if (fp == NULL) return -1;

    while (fgets(line, sizeof(line)-1, fp) != NULL) {
        char *token = strtok(line, " ");
        while (token != NULL) {
            last_arg = token; // Keep updating last_arg until the end
            token = strtok(NULL, " ");
        }
    }
    if (last_arg == NULL) return -1;
    strcpy(path, last_arg);
    path[strcspn(path, "\n")] = 0;
    return 0;
}

pid_t get_path_pid(char *path) {
    FILE *fp;
    char result[1024];
    char server_path[MAX_PATH_LEN];

    fp = popen("pgrep ^cserver", "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
        exit(EXIT_FAILURE);
    }

    while (fgets(result, sizeof(result)-1, fp) != NULL) {
        pid_t pid = atoi(result);
        memset(server_path, 0, MAX_PATH_LEN);
        if (get_pid_path(pid, server_path) == 0) {
            if (strcmp(trim_whitespace(server_path), path) == 0) {
                pclose(fp);
                return pid;
            }
        }
    }

    pclose(fp);
    return 0;
}

int list_servers() {
    FILE *fp;
    char result[1024];
    char path[MAX_PATH_LEN];

    fp = popen("pgrep ^cserver", "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
        exit(EXIT_FAILURE);
    }

    printf("Running instances:\n");
    while (fgets(result, sizeof(result)-1, fp) != NULL) {
        int pid = atoi(result);
        memset(path, 0, MAX_PATH_LEN);
        get_pid_path(pid, path);
        printf("%i %s\n", pid, path);
    }

    pclose(fp);
    return EXIT_SUCCESS;
}

int restart_server(char *path) {
    pid_t pid = get_path_pid(path);
    if (pid == 0) {
        perror("Error: No running server found for the provided path");
        exit(EXIT_FAILURE);
    }
    if (kill(pid, SIGTERM) == -1) {
        perror("Error sending SIGTERM");
        exit(EXIT_FAILURE);
    }
    waitpid(pid, NULL, 0);
    
    return start_server(path, false);
}

int stop_server(char *id) {
    // TODO: Make sure it kills only cserevr processes
    int pid = atoi(id);
    if (kill(pid, SIGTERM) == -1) {
        perror("Error sending SIGTERM");
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////

void append_path(char *result, size_t result_len, char *p1, char *p2) {
    if (!p1) {
        snprintf(result, result_len, "%s", p2);
    } else if (!p2) {
        snprintf(result, result_len, "%s", p1);
    } else {
        snprintf(result, result_len, "%s/%s", p1, p2);
    }
}

void to_lowercase_and_dash(char *output, const char *input) {
    while (*input) {
        if (isspace((unsigned char)*input)) {
            *output = '-';
        } else {
            *output = tolower((unsigned char)*input);
        }
        output++;
        input++;
    }
    *output = '\0';
}

void store_metadata(cJSON *metadata, char *key, char *value, char *filename) {
    if (!cJSON_HasObjectItem(metadata, key)) {
        cJSON_AddObjectToObject(metadata, key);
    }
    cJSON *key_json = cJSON_GetObjectItem(metadata, key);

    if (strcmp(key, "slug") == 0) {                             // "slug": { "slug-value": "filename" }
        cJSON_AddStringToObject(key_json, value, filename);     
    } else if (strcmp(key, "published") == 0) {                 // "published": { "filename": "date" }
        cJSON_AddStringToObject(key_json, filename, value);     
    } else if (strcmp(key, "tags") == 0) {                      // "tags": { "name": ["filename", ...]  }
        char *tag = strtok(value, ",");
        while (tag != NULL) {
            trim_whitespace(tag);
            if (!cJSON_HasObjectItem(key_json, tag)) {
                cJSON_AddArrayToObject(key_json, tag);
            }
            cJSON *tag_array = cJSON_GetObjectItem(key_json, tag);
            cJSON_AddItemToArray(tag_array, cJSON_CreateString(filename));
            tag = strtok(NULL, ",");
        }
    } else {                                                    // "category": { "name": ["filename", ...]},
        if (!cJSON_HasObjectItem(key_json, value)) {            // "author": { "name": ["filename", ...] }, ...
            cJSON_AddArrayToObject(key_json, value);
        }
        cJSON *value_array = cJSON_GetObjectItem(key_json, value);
        cJSON_AddItemToArray(value_array, cJSON_CreateString(filename));
    }
}

void process_file(cJSON *metadata, cJSON *files, const char *filename, const char *relative_name) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // Store relative paths only, 
    // /path/to/file.md -> /file
    // /path/to/file/index.md -> /file
    // Remove the file extension if present
    char *file_without_ext = strdup(relative_name);
    char *dot = strrchr(file_without_ext, '.');
    if (dot) {
        *dot = '\0';
    }
    // Remove "/index" part if it exists
    char *index_part = strstr(file_without_ext, "/index");
    if (index_part) {
        *index_part = '\0';
    }

    // Store initial value in files object
    char *bare_filename = strrchr(file_without_ext, '/') ? strrchr(file_without_ext, '/') + 1 : file_without_ext;
    cJSON_AddStringToObject(files, file_without_ext, bare_filename);

    char line[1024];
    int metadata_section = 0;
    while (fgets(line, sizeof(line), file)) {
        if (strcmp(line, "---\n") == 0) {
            metadata_section = !metadata_section;  // Toggle metadata section state
            if (!metadata_section) {               // End of metadata section
                break;
            }
        } else if (metadata_section) {
            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';                     // Split the string into key and value
                char *key = line;
                char *value = colon + 1;
                key = trim_whitespace(key);
                value = trim_whitespace(value);
                // Store title directly into files object
                if (strcmp(key, "title") == 0) {
                    cJSON_ReplaceItemInObject(files, file_without_ext, cJSON_CreateString(value));
                }
                store_metadata(metadata, key, value, file_without_ext);
            }
        }
    }

    fclose(file);
    free(file_without_ext);
}

void collect_metadata(cJSON *metadata, char *base_path, char *path) {
    char full_path[MAX_PATH_LEN];
    append_path(full_path, sizeof(full_path), base_path, path);
    DIR *dir = opendir(full_path);
    if (!dir) {
        perror("Failed to open directory");
        return;
    }

    cJSON *files = cJSON_GetObjectItem(metadata, "files");
    if (!files) {
        files = cJSON_CreateObject();
        cJSON_AddItemToObject(metadata, "files", files);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char new_path[1024];
            append_path(new_path, sizeof(new_path), path, entry->d_name);
            collect_metadata(metadata, base_path, new_path);
        } else if (entry->d_type == DT_REG) {
            char *dot = strrchr(entry->d_name, '.');
            if (dot && strcmp(dot, ".md") == 0) {
                char filename[1024];
                append_path(filename, sizeof(filename), full_path, entry->d_name);
                char relative_name[1024];
                append_path(relative_name, sizeof(relative_name), path, entry->d_name);
                process_file(metadata, files, filename, relative_name);
            }
        }
    }

    closedir(dir);
}

void create_index(cJSON *metadata) {
    cJSON *index = cJSON_CreateObject();
    cJSON *categories = cJSON_CreateArray();
    cJSON_AddItemToObject(index, "category", categories);

    // Extract metadata objects required for creating index
    cJSON *category_metadata = cJSON_GetObjectItem(metadata, "category");
    cJSON *files_metadata = cJSON_GetObjectItem(metadata, "files");

    if (!category_metadata || !files_metadata) {
        cJSON_AddItemToObject(metadata, "index", index);
        return;
    }

    cJSON *category;
    cJSON_ArrayForEach(category, category_metadata) {
        const char *category_name = category->string;
        char lowercased_name[256];
        to_lowercase_and_dash(lowercased_name, category_name);

        cJSON *category_object = cJSON_CreateObject();
        cJSON_AddStringToObject(category_object, "name", lowercased_name);
        cJSON_AddStringToObject(category_object, "title", category_name);

        cJSON *pages_array = cJSON_CreateArray();
        cJSON_AddItemToObject(category_object, "pages", pages_array);

        cJSON *page;
        cJSON_ArrayForEach(page, category) {
            const char *page_link = page->valuestring;

            cJSON *page_object = cJSON_CreateObject();
            cJSON_AddStringToObject(page_object, "link", page_link);

            cJSON *page_title = cJSON_GetObjectItem(files_metadata, page_link);
            if (page_title) {
                cJSON_AddStringToObject(page_object, "title", page_title->valuestring);
            }

            cJSON_AddItemToArray(pages_array, page_object);
        }

        cJSON_AddItemToArray(categories, category_object);
    }

    cJSON_AddItemToObject(metadata, "index", index);
}

void store_metadata_v2(char *key, char *value, char *filename, cJSON *metadata) {
    cJSON *meta_array;
    char processed_name[256];
    
    if (!cJSON_HasObjectItem(metadata, key)) {
        meta_array = cJSON_CreateArray();
        cJSON_AddItemToObject(metadata, key, meta_array);
    } else {
        meta_array = cJSON_GetObjectItem(metadata, key);
    }

    to_lowercase_and_dash(processed_name, value);

    cJSON *meta_item = NULL;
    cJSON_ArrayForEach(meta_item, meta_array) {
        if (strcmp(cJSON_GetObjectItem(meta_item, "name")->valuestring, processed_name) == 0) {
            break;
        }
    }

    if (meta_item == NULL) {
        meta_item = cJSON_CreateObject();
        cJSON_AddStringToObject(meta_item, "name", processed_name);
        cJSON_AddStringToObject(meta_item, "title", value);
        cJSON_AddItemToObject(meta_item, "pages", cJSON_CreateArray());
        cJSON_AddItemToArray(meta_array, meta_item);
    }

    cJSON *pages = cJSON_GetObjectItem(meta_item, "pages");
    cJSON *page_info = cJSON_CreateObject();
    cJSON_AddStringToObject(page_info, "title", value);
    cJSON_AddStringToObject(page_info, "link", filename);
    cJSON_AddItemToArray(pages, page_info);
}

void process_file_v2(const char *filename, cJSON *metadata) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    char line[1024];
    int metadata_section = 0;
    char current_title[256] = "";

    while (fgets(line, sizeof(line), file)) {
        if (strcmp(line, "---\n") == 0) {
            metadata_section = !metadata_section;  // Toggle metadata section state
            if (!metadata_section) {  // End of metadata section
                break;
            }
        } else if (metadata_section) {
            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';  // Split the string into key and value
                char *key = line;
                char *value = colon + 1;
                key = trim_whitespace(key);
                value = trim_whitespace(value);
                if (strcmp(key, "title") == 0) {
                    strcpy(current_title, value);
                }
                store_metadata_v2(key, value, (char *)filename, metadata);
            }
        }
    }

    fclose(file);
}

void collect_metadata_v2(char *path, cJSON *metadata) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("Failed to open directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            char next_path[1024];
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            snprintf(next_path, sizeof(next_path), "%s/%s", path, entry->d_name);
            collect_metadata_v2(next_path, metadata);  // Recursively call on subdirectory
        } else if (entry->d_type == DT_REG) {
            char *dot = strrchr(entry->d_name, '.');
            if (dot && strcmp(dot, ".md") == 0) {
                char filepath[1024];
                snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);
                process_file_v2(filepath, metadata);
            }
        }
    }

    closedir(dir);
}

///////////////////////////////////////////////////////////////////////////////

string make_response(char *http_status, const char *content_type, string content) {
    string result = string_init();
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
    if (content.value) {
        strcat(response, content.value);
    }

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

    // Check if there's a directory that matches the path minus the last element
    char* last_slash = strrchr(filename, '/');
    if (last_slash != NULL) {
        *last_slash = '\0'; // Null-terminate to get the preceding directory path
        snprintf(filename + strlen(filename), sizeof(filename) - strlen(filename), "/children.html");
        if (file_exists(filename)) {
            return filename;
        }
    }
    if (last_slash != NULL) {
        *last_slash = '\0'; // Null-terminate to get the preceding directory path
        snprintf(filename + strlen(filename), sizeof(filename) - strlen(filename), "/children.md");
        if (file_exists(filename)) {
            return filename;
        }
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

    // page name component    
    char *last_slash = strrchr(request_path, '/');
    char *page = last_slash ? last_slash + 1 : request_path;  // Point to the component after the last slash
    cJSON *request_page = cJSON_CreateString(page);
    cJSON_AddItemToObject(request, "page", request_page);

    cJSON_AddItemToObject(context, "request", request);
}

void add_references(cJSON *context) {
    cJSON *request = cJSON_GetObjectItem(context, "request");
    cJSON *site = cJSON_GetObjectItem(context, "site");
    cJSON *index = cJSON_GetObjectItem(site, "index");
    cJSON *references = cJSON_CreateObject();

    if (!request) {
        printf("request not found\n");
    }
    if (!index) {
        printf("index not found\n");
    }
    // Ensure all required objects exist
    if (!request || !index) {
        cJSON_AddItemToObject(context, "references", references);
        return;
    }

    // Get the parent and page from the request object
    cJSON *parent_item = cJSON_GetObjectItem(request, "parent");
    cJSON *page_item = cJSON_GetObjectItem(request, "page");

    const char *parent = parent_item ? parent_item->valuestring : NULL;
    const char *page = page_item ? page_item->valuestring : NULL;

    // Case 1: If both parent and page exist in the request
    if (parent && page) {
        cJSON *metadata_array = cJSON_GetObjectItem(index, parent);
        if (metadata_array) {
            cJSON *metadata_item;
            cJSON_ArrayForEach(metadata_item, metadata_array) {
                cJSON *name_item = cJSON_GetObjectItem(metadata_item, "name");
                if (name_item && strcmp(name_item->valuestring, page) == 0) {
                    // Page matches an item within this category, add its pages to references
                    cJSON *pages = cJSON_GetObjectItem(metadata_item, "pages");
                    if (pages) {
                        cJSON_AddItemToObject(references, "pages", cJSON_Duplicate(pages, 1));
                    }
                    break;
                }
            }
        }
    // Case 2: If only page exists in the request
    } else if (page) {
        cJSON *metadata_array = cJSON_GetObjectItem(index, page);
        if (metadata_array) {
            cJSON_AddItemToObject(references, page, cJSON_Duplicate(metadata_array, 1));
        }
    }

    cJSON_AddItemToObject(context, "references", references);
}
int read_int(cJSON *object, char *name, int default_value) {
    cJSON *value_object = cJSON_GetObjectItem(object, name);
    if (value_object == NULL) return default_value;
    double value = cJSON_GetNumberValue(value_object);
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
        add_references(context);
        string md_expanded_content = render_mustache(markdown_content, context);
        string md_html_content = render_markdown(md_expanded_content);
        string_free(file_content);
        string_free(md_expanded_content);
        
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


// Markdown ///////////////////////////////////////////////////////////////////


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
                // printf("input_content.length = %li, result.length = %li\n", input_content.length, result.length);
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
        result.value = input_content.value + input_content.length;
        result.length = 0;
        return result;
    }
}

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
    html_buffer buf = { .output = NULL, .size = 0 };
    string result = string_init();

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


// Mustache ///////////////////////////////////////////////////////////////////


int load_partial(const char *name, struct mustach_sbuf *sbuf) {
    // Example of opening a file named after the partial. Adjust path as necessary.
    char filename[MAX_PATH_LEN];
    snprintf(filename, sizeof(filename), "templates/partials/%s.mustache", name);
    FILE *file = fopen(filename, "r");
    
    if (!file) {
        // Return an error if the file cannot be opened
        // printf("Partial %s not found.\n", filename);
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
        // printf("Tempalte %s not found.\n", filename);
    }
    return template;
}

string render_mustache(string template_content, cJSON *context) {
    string result = { .value = NULL, .length = 0 };

    char* output = NULL;
    size_t output_size = 0;
    FILE* output_stream = open_memstream(&output, &output_size);

    // Perform the mustach processing
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
