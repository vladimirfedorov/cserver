#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 8080

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
    bind(server_desc, (struct sockaddr *)&address, address_len);

    // Listen for connections
    int listen_result = listen(server_desc, 3);
    if (listen_result < 0) {
        perror("listen failed\n");
    }

    while (1) {
        printf("Waiting for a connection...\n");
        
        // Accept a connection
        int socket_desc = accept(server_desc, (struct sockaddr *)&address, &address_len);
        if (socket_desc < 0) {
            perror("accept failed.\n");
            exit(EXIT_FAILURE);
        }

        // Parse the request and send a response
        char buffer[buffer_len] = {0};
		long recv_result = recv(socket_desc, buffer, buffer_len, 0);
        if (recv_result < 0) {
            perror("recv failed.\n");
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

		// Send the response
        char *response = make_response(200, "OK", "text/plain", "Hello!");
		int send_result = send(socket_desc, response, strlen(response), 0);
        if (send_result < 0) {
            perror("send failed.");
            exit(EXIT_FAILURE);
        }

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
    char content_length_str[content_length_length];
    sprintf(content_length_str, "Content-Length: %d\r\n", content_length);
    strcat(response, content_length_str);
    // Empty line to separate headers from the content
    strcat(response, "\r\n"); 
    // Content
    strcat(response, content);

    return response;
}
