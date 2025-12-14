#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 9000
#define NODE_PORT   10000
#define BUF_SIZE    1024
#define MAX_NAME    256


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

void serve_file(const char *filename) {
    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(NODE_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Bind failed (Port %d might be busy): %d\n", NODE_PORT, WSAGetLastError());
        return;
    }
    listen(server, 5);

    printf("Sharing file: %s (Listening on port %d)\n", filename, NODE_PORT);

    while (1) {
        SOCKET client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET) continue;

        FILE *fp = fopen(filename, "rb");
        if (fp) {
            char buffer[BUF_SIZE];
            int n;
            while ((n = fread(buffer, 1, BUF_SIZE, fp)) > 0)
                send(client, buffer, n, 0);
            fclose(fp);
            printf("File sent to a peer.\n");
        } else {
            printf("Error: Could not open file %s to send.\n", filename);
        }
        
        closesocket(client);
    }
    closesocket(server);
}

void register_file(const char *filename) {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    Message msg;
    msg.type = MSG_REGISTER;
    strcpy(msg.entry.filename, filename);
    strcpy(msg.entry.node_ip, "127.0.0.1");
    msg.entry.node_port = NODE_PORT;

    sendto(sock, (char*)&msg, sizeof(msg), 0,
           (struct sockaddr*)&server, sizeof(server));

    closesocket(sock);
    printf("Registered file '%s' with server.\n", filename);
}

void get_file_list() {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    DWORD timeout = 2000; // 2000 ms = 2 giây
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    Message msg;
    msg.type = MSG_LIST; // Gửi yêu cầu LIST
    
    sendto(sock, (char*)&msg, sizeof(msg), 0,
           (struct sockaddr*)&server, sizeof(server));

    printf("\n--- Available Files ---\n");
    int count = 0;
    
    while (1) {
        int len = sizeof(server);
        int n = recvfrom(sock, (char*)&msg, sizeof(msg), 0, NULL, NULL);
        if (n < 0) break;

        if (msg.type == MSG_REPLY) {
            count++;
            printf("%d. File: %-20s | Node: %s:%d\n", 
                   count, msg.entry.filename, msg.entry.node_ip, msg.entry.node_port);
        }
    }

    if (count == 0) {
        printf("No files found or server no response.\n");
    } else {
        printf("-----------------------\nTotal: %d files.\n", count);
    }

    closesocket(sock);
}

void download_file(const char *filename) {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    Message msg;
    msg.type = MSG_LOOKUP;
    strcpy(msg.entry.filename, filename);

    sendto(sock, (char*)&msg, sizeof(msg), 0,
           (struct sockaddr*)&server, sizeof(server));

    int len = sizeof(server);
    recvfrom(sock, (char*)&msg, sizeof(msg), 0, NULL, NULL);
    closesocket(sock);

    if (msg.entry.node_port == 0) {
        printf("File not found on server.\n");
        return;
    }

    printf("Found file at %s:%d. Connecting...\n", msg.entry.node_ip, msg.entry.node_port);

    SOCKET peer = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in peer_addr;

    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(msg.entry.node_port);
    peer_addr.sin_addr.s_addr = inet_addr(msg.entry.node_ip);

    if (connect(peer, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
        printf("Could not connect to peer.\n");
        closesocket(peer);
        return;
    }

    char new_filename[300];
    sprintf(new_filename, "downloaded_%s", filename);

    FILE *fp = fopen(new_filename, "wb");
    if (!fp) {
        printf("Cannot create file to write.\n");
        closesocket(peer);
        return;
    }

    char buffer[BUF_SIZE];
    int n;

    while ((n = recv(peer, buffer, BUF_SIZE, 0)) > 0)
        fwrite(buffer, 1, n, fp);

    fclose(fp);
    closesocket(peer);

    printf("Download complete: Saved as '%s'\n", new_filename);
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed to initialize Winsock.\n");
        return 1;
    }

    int choice;
    char filename[MAX_NAME];
    while(1) {
        printf("\n=== P2P File Sharing ===\n");
        printf("1. Share file\n");
        printf("2. Download file\n");
        printf("3. Show all shared files\n"); // Thêm lựa chọn 3
        printf("4. Exit\n");
        printf("Choice: ");
        scanf("%d", &choice);

        if (choice == 1) {
            printf("File name to share: ");
            scanf("%s", filename);
            FILE *f = fopen(filename, "r");
            if (!f) {
                f = fopen(filename, "w");
                fprintf(f, "This is the content of %s", filename);
                fclose(f);
                printf("(Created dummy file '%s' for testing)\n", filename);
            } else fclose(f);

            register_file(filename);
            serve_file(filename); 
        } else if (choice == 2) {
            printf("File name to download: ");
            scanf("%s", filename);
            download_file(filename);
        } else if (choice == 3) {
            get_file_list();
        } else if (choice == 4) {
            break;
        } else {
            printf("Invalid choice.\n");
        }
    }

    WSACleanup();
    return 0;
}
