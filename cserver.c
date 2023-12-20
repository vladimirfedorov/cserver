#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 8080

int main(int argc, char* argv[]) {

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
		char response[] = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 5\n\nhello";
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
