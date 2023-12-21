#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void make_response(char *response, int status_code, char *status_message, char *content_type, char *content);

int main(int argc, char **argv) {
	char *response;
	response = malloc(30000);
	make_response(response, 200, "OK", "plain/text", "Hello there!");
	printf("----\n%s\n----\n", response);
	make_response(response, 500, "Internal Server Error", "application/json", "{ \"message\": \"Hello there!\" }");
	printf("----\n%s\n----\n", response);
	free(response);
	exit(0);
}

void make_response(char *buffer, int status_code, char *status_message, char *content_type, char *content) {
    sprintf(buffer, "HTTP/1.1 %d %s\r\n", status_code, status_message);
    strcat(buffer, "Content-Type: ");
    strcat(buffer, content_type);
    strcat(buffer, "\n");
    
    // Calculate the content length
    int content_length = strlen(content);
    char content_length_str[30];
    sprintf(content_length_str, "Content-Length: %d\n", content_length);
    strcat(buffer, content_length_str);
    
    strcat(buffer, "\n");
    strcat(buffer, content);
}