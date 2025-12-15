#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_PORT 9000
#define MAX_FILES   100
#define MAX_NAME    256

#define MSG_REGISTER  1
#define MSG_LOOKUP    2
#define MSG_REPLY     3
#define MSG_LIST      4
#define MSG_DISCOVERY 5

typedef struct {
    char filename[MAX_NAME];
    char node_ip[INET_ADDRSTRLEN];
    int  node_port;
} FileEntry;

typedef struct {
    int type;
    FileEntry entry;
} Message;

FileEntry database[MAX_FILES];
int file_count = 0;

int find_file(const char *filename) {
    for (int i = 0; i < file_count; i++) {
        if (strcmp(database[i].filename, filename) == 0)
            return i;
    }
    return -1;
}

int main() {
    int sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    Message msg;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    printf("Discovery Server running on port %d...\n", SERVER_PORT);

    while (1) {
        ssize_t n = recvfrom(sock, &msg, sizeof(msg), 0,
                             (struct sockaddr*)&client_addr, &addr_len);
        if (n <= 0) continue;

        if (msg.type == MSG_REGISTER) {
            if (file_count >= MAX_FILES) {
                printf("[ERROR] Server full\n");
                continue;
            }
            if (find_file(msg.entry.filename) == -1) {
                database[file_count++] = msg.entry;
                printf("[REGISTER] '%s' from %s:%d\n", msg.entry.filename, msg.entry.node_ip, msg.entry.node_port);
            } else {
                printf("[!][File] '%s' already registered\n", msg.entry.filename);
            }
        }
        else if (msg.type == MSG_LOOKUP) {
            Message reply;
            memset(&reply, 0, sizeof(reply));
            reply.type = MSG_REPLY;
            int index = find_file(msg.entry.filename);
            if (index != -1) {
                reply.entry = database[index];
                printf("[LOOKUP] '%s' found\n", msg.entry.filename);
            } else {
                printf("[LOOKUP] '%s' not found\n", msg.entry.filename);
            }
            sendto(sock, &reply, sizeof(reply), 0, (struct sockaddr*)&client_addr, addr_len);
        }
        else if (msg.type == MSG_LIST) {
            Message reply;
            reply.type = MSG_REPLY;
            printf("LIST request received\n");
            for (int i = 0; i < file_count; i++) {
                reply.entry = database[i];
                sendto(sock, &reply, sizeof(reply), 0, (struct sockaddr*)&client_addr, addr_len);
            }
        }
        /* DISCOVERY HANDLER */
        else if (msg.type == MSG_DISCOVERY) {
            printf("[DISCOVERY] Client looking for server from %s\n", inet_ntoa(client_addr.sin_addr));
            Message reply;
            memset(&reply, 0, sizeof(reply));
            reply.type = MSG_REPLY;
            sendto(sock, &reply, sizeof(reply), 0, (struct sockaddr*)&client_addr, addr_len);
        }
    }
    close(sock);
    return 0;
}