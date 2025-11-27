#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h> 

#define SERVER_PORT 8080 
#define BUFFER_SIZE 1024

void execute_file_transfer(int sockfd, const char *filename_to_send) {
    int file_fd_read = open(filename_to_send, O_RDONLY);
    if (file_fd_read < 0) {
        perror("ERROR: failed to open source file");
        return;
    }
    
    if (send(sockfd, filename_to_send, strlen(filename_to_send), 0) < 0) { 
        perror("ERROR sending filename");
        close(file_fd_read);
        return;
    }

    char data_buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    
    printf("Sending file content...\n");

    while ((bytes_read = read(file_fd_read, data_buffer, BUFFER_SIZE)) > 0) {
        if (send(sockfd, data_buffer, bytes_read, 0) < 0) { 
            perror("ERROR sending file data");
            break;
        }
    }

    close(file_fd_read); 
    close(sockfd); 
    printf("File transfer complete and connection closed by Client.\n");
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_hostname_or_ip> <filename_to_send>\n", argv[0]);
        exit(1);
    }
    
    const char *server_hostname = argv[1];
    const char *filename_to_send = argv[2];
    
    int client_fd = socket(AF_INET, SOCK_STREAM, 0); 
    if (client_fd < 0) {
        perror("ERROR creating client socket");
        exit(EXIT_FAILURE);
    }

    struct hostent *server_host = gethostbyname(server_hostname); 
    if (server_host == NULL) {
        fprintf(stderr, "ERROR: Host '%s' not found\n", server_hostname);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server_host->h_addr, server_host->h_length);
    
    server_addr.sin_port = htons(SERVER_PORT); 

    printf("Attempting to connect to %s:%d...\n", server_hostname, SERVER_PORT);
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) { 
        perror("ERROR connecting to server");
        exit(EXIT_FAILURE);
    }

    printf("Connection established. Starting file transfer...\n");
    
    execute_file_transfer(client_fd, filename_to_send);

    return 0;
}