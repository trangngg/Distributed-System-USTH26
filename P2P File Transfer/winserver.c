#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 9000
#define BUF_SIZE    1024
#define MAX_NAME    256
#define MAX_FILES   100 

#define MSG_REGISTER 1
#define MSG_LOOKUP   2
#define MSG_REPLY    3
#define MSG_LIST     4

typedef struct {
    char filename[MAX_NAME];
    char node_ip[INET_ADDRSTRLEN];
    int  node_port;
} FileEntry;

typedef struct {
    int type;
    FileEntry entry;
} Message;

FileEntry file_list[MAX_FILES];
int file_count = 0;

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed to initialize Winsock.\n");
        return 1;
    }

    SOCKET server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in server_addr, client_addr;
    int client_len = sizeof(client_addr);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed. Error: %d\n", WSAGetLastError());
        return 1;
    }

    printf("Discovery Server is running on port %d...\n", SERVER_PORT);

    Message msg;

    while (1) {
        int n = recvfrom(server_sock, (char*)&msg, sizeof(msg), 0, 
                         (struct sockaddr*)&client_addr, &client_len);
        
        if (n == SOCKET_ERROR) continue;

             char *client_ip_str = inet_ntoa(client_addr.sin_addr);
        char client_ip[INET_ADDRSTRLEN];
        if (client_ip_str != NULL) {
            strcpy(client_ip, client_ip_str);
        } else {
            strcpy(client_ip, "Unknown");
        }
        switch (msg.type) {
            case MSG_REGISTER: {
                if (file_count < MAX_FILES) {
                    file_list[file_count] = msg.entry;
                    printf("[REGISTER] %s shared file '%s' at port %d\n", 
                           client_ip, msg.entry.filename, msg.entry.node_port);
                    file_count++;
                } else {
                    printf("[ERROR] Server full.\n");
                }
                break;
            }

            case MSG_LOOKUP: {
                printf("[LOOKUP] %s is looking for '%s'\n", client_ip, msg.entry.filename);
                Message reply;
                reply.type = MSG_REPLY;
                reply.entry.node_port = 0; 

                for (int i = 0; i < file_count; i++) {
                    if (strcmp(file_list[i].filename, msg.entry.filename) == 0) {
                        reply.entry = file_list[i];
                        break;
                    }
                }
                sendto(server_sock, (char*)&reply, sizeof(reply), 0, 
                       (struct sockaddr*)&client_addr, client_len);
                break;
            }

            case MSG_LIST: {
                printf("[LIST] %s requested file list.\n", client_ip);
                Message reply;
                reply.type = MSG_REPLY;

                for (int i = 0; i < file_count; i++) {
                    reply.entry = file_list[i];
                    sendto(server_sock, (char*)&reply, sizeof(reply), 0, 
                           (struct sockaddr*)&client_addr, client_len);
                }
                break;
            }
        }
    }

    closesocket(server_sock);
    WSACleanup();
    return 0;
}