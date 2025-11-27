// SERVER.C - TCP File Transfer Server
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h> 

#define PORT_NUMBER 8080 
#define MAX_PENDING 10   
#define BUFFER_SIZE 1024 

void print_server_info(int sockfd) {
    char hostname[1024];
    struct hostent *host_entry;
    
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        host_entry = gethostbyname(hostname);
        if (host_entry != NULL) {
            char *ip_address = inet_ntoa(*(struct in_addr *)host_entry->h_addr);
            printf("Server IP Address: %s\n", ip_address); 
        } else {
            perror("Error: Cannot resolve hostname to IP");
        }
    } else {
        perror("Error: Cannot get hostname");
    }

    struct sockaddr_in current_addr;
    socklen_t addrlen = sizeof(current_addr);
    
    if (getsockname(sockfd, (struct sockaddr *)&current_addr, &addrlen) == 0) {
        printf("Server Port: %d\n", ntohs(current_addr.sin_port)); 
    } else {
        perror("Error: Could not get socket name/port");
    }
}

void handle_client_session(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    char client_filename[BUFFER_SIZE] = {0}; 
    
    bytes_received = recv(client_socket, client_filename, BUFFER_SIZE - 1, 0); 
    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }
    client_filename[bytes_received] = '\0';
    
    char saved_filename[2 * BUFFER_SIZE]; 
    snprintf(saved_filename, sizeof(saved_filename), "received_%s", client_filename);

    int file_fd_write = open(saved_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd_write < 0) {
        perror("Error: Failed to create local file");
        close(client_socket);
        return;
    }
    printf("\n--- Session started. Receiving file: %s ---\n", saved_filename);

    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        if (write(file_fd_write, buffer, bytes_received) < 0) {
            perror("Error: Failed to write to file");
            break;
        }
    }
    
    if (bytes_received == 0) {  
        printf("File received successfully. End-of-File signal received.\n"); 
    } else if (bytes_received < 0) {
        perror("Error during file reception");
    }

    close(file_fd_write); 
    close(client_socket);  
    printf("--- Session closed ---\n"); 
}

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0); 
    if (listen_fd < 0) {
        perror("Error creating listening socket");
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    
    server_addr.sin_port = htons(PORT_NUMBER); 
    
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) { 
        perror("Error binding socket to address/port");
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening for connections.\n");
    print_server_info(listen_fd);
    printf("------------------------------------------\n");

    if (listen(listen_fd, MAX_PENDING) < 0) { 
        perror("ERROR on listen"); 
        exit(EXIT_FAILURE);
    }
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int connection_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len); 
        
        if (connection_fd < 0) {
            perror("ERROR on accept"); 
            continue; 
        }

        handle_client_session(connection_fd); 
    }

    return 0;
}