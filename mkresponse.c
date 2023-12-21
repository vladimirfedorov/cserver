#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* make_response(char *response, int status_code, char *status_message, char *content_type, char *content);

int main(int argc, char **argv) {
	char *response;
    
    // 1
	response = make_response(response, 200, "OK", "plain/text", "Hello there!");
	printf("----\n%s\n----\n", response);
    free(response);
    
    // 2 
	response = make_response(response, 500, "Internal Server Error", "application/json", "{ \"message\": \"Hello there!\" }");
	printf("----\n%s\n----\n", response);
	free(response);
	
    exit(0);
}

char* make_response(char *buffer, int status_code, char *status_message, char *content_type, char *content) {
    // Calculate the lengths of various parts of the HTTP response
    int status_line_length = snprintf(NULL, 0, "HTTP/1.1 %d %s\r\n", status_code, status_message);
    int content_type_length = snprintf(NULL, 0, "Content-Type: %s\r\n", content_type);
    int content_length = strlen(content);
    
    // Calculate the total length of the HTTP response
    int total_length = status_line_length + content_type_length + content_length + 4; // 4 for \r\n\r\n and null terminator
    
    // Allocate memory for the complete HTTP response
    char *response = (char*)malloc(total_length);
    if (response == NULL) {
        // Handle allocation failure
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
    char content_length_str[30];
    sprintf(content_length_str, "Content-Length: %d\r\n", content_length);
    strcat(response, content_length_str);
    // Empty line to separate headers from the content
    strcat(response, "\r\n"); 
    // Content
    strcat(response, content);

    return response;
}
